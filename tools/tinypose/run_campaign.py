#!/usr/bin/env python3
"""Prepare/run isolated fixed-budget experiments without editing frozen tools."""
import argparse
import copy
import json
import pathlib
import shutil

import numpy as np

from comparison_support import ROOT, BatchSampler, verify_manifest
from dataset_import import sha256
from keypoint_dataset import load_jsonl


def prepare(parent, output, kind):
    parent, output = parent.resolve(), output.resolve()
    if output.exists() or not output.is_relative_to(ROOT/'.secrets'):
        raise ValueError('fresh private experiment required')
    m = copy.deepcopy(verify_manifest(parent))
    for d in m['datasets'].values():
        old, new = parent.parent/d['path'], output/d['path']
        for row in load_jsonl(old):
            if (old.parent/row['image']).resolve() != (new.parent/row['image']).resolve():
                raise ValueError('relative source image changed')
    m['source_hashes'][str(parent.relative_to(ROOT))] = sha256(parent)
    for name in ('run_campaign.py', 'harden_augmentation.py', 'campaign_metrics.py', 'evaluate_negatives.py'):
        p = pathlib.Path('tools/tinypose')/name
        m['source_hashes'][str(p)] = sha256(ROOT/p)
    m['campaign_kind'] = kind
    m['parent_manifest'] = str(parent.relative_to(ROOT))
    m['independent_real_acceptance'] = 'PENDING_NEW_SUBJECT_SESSION_BACKGROUND_DATA'
    if kind == 'negative':
        m['arms'] = [{'id': 'sc_hardened_photo', 'domain_counts': {'sc2336_train':32},
                      'photometric': True, 'harden': True, 'seed':34596}]
        m['negative_policy'] = {'normal_sc_photo':24, 'explicit_empty_gain':4, 'explicit_occlusion':4,
            'sources':'frozen sc2336_train ONLY; original coordinates/soft labels retained outside covered centers',
            'training_materials':['black','gray','cloth','wood'], 'regions':['ankles','torso'],
            'gain':[.25,1.5], 'loss':'original pose_loss; covered confidence zero; coordinates ignored by loss',
            'acceptance':'old observed diagnostics only; not new independent real-world acceptance'}
    elif kind == 'mixed':
        m['arms'] = [{'id':'sc_coco_125_photo','domain_counts':{'sc2336_train':28,'coco_train':4},
                      'photometric':True, 'seed':34596}]
    elif kind == 'seeds':
        m['predeclared_seeds'] = [34596,34597,34598]
        m['reuse'] = {
            '34596_plain':'.secrets/experiments/tinypose-coco-control-v1-20261024/trained-20261025/sc_only',
            '34596_photo':'.secrets/experiments/tinypose-followup-v2-20261025/trained/sc_only_photo'}
        for directory in m['reuse'].values():
            for file in ('training-report.json','tinypose_keypoints_int8.tflite','tinypose_float.keras'):
                p=pathlib.Path(directory)/file;m['source_hashes'][str(p)] = sha256(ROOT/p)
        m['arms'] = [{'id':f'sc_{seed}_{"photo" if photo else "plain"}',
                      'domain_counts':{'sc2336_train':32},'photometric':photo,'seed':seed}
                     for seed in (34597,34598) for photo in (False,True)]
    else:
        raise ValueError('unknown campaign')
    m['limitations'].append('Adaptive follow-up on observed validation; synthetic variants are correlated, not independent holdout.')
    output.mkdir(parents=True)
    for d in m['datasets'].values():
        target=output/d['path'];target.parent.mkdir(exist_ok=True,parents=True)
        shutil.copyfile(parent.parent/d['path'],target)
    path=output/'experiment.json';path.write_text(json.dumps(m,indent=2)+'\n')
    verify_manifest(path)
    print(json.dumps({'manifest':str(path),'sha256':sha256(path),'arms':m['arms']}))


def train(path, output):
    import tensorflow as tf
    from train_keypoints import load_records, pose_loss
    from run_comparison import export_int8
    from harden_augmentation import HardenSampler
    from campaign_metrics import evaluate_model
    path, output=path.resolve(),output.resolve()
    if output.exists() or not output.is_relative_to(ROOT/'.secrets'):
        raise ValueError('fresh private output required')
    m=verify_manifest(path);manifest_hash=sha256(path);b=m['budget']
    devices=tf.config.list_physical_devices('GPU')
    if not devices: raise RuntimeError('GPU required; no CPU training fallback')
    for device in devices:tf.config.experimental.set_memory_growth(device,True)
    tf.config.experimental.enable_op_determinism()
    pools={};records={}
    for role,d in m['datasets'].items():
        x,y,r=load_records([path.parent/d['path']])
        if len(r)!=d['count']:raise ValueError('loader changed frozen count')
        pools[role]=(x,y);records[role]=r
    initial=ROOT/m['initial_checkpoint'];initial_hash=sha256(initial)
    output.mkdir(parents=True)
    report={'status':'RUNNING','mode':'TRAIN','gpu_available':True,
            'manifest_sha256':manifest_hash,'initial_checkpoint_sha256':initial_hash,
            'tensorflow':tf.__version__,'arms':[]}
    for a in m['arms']:
        seed=a['seed'];tf.keras.backend.clear_session();tf.keras.utils.set_random_seed(seed)
        model=tf.keras.models.load_model(initial,compile=False)
        sampler=HardenSampler(pools,records['sc2336_train'],seed) if a.get('harden') else BatchSampler(pools,a['domain_counts'],seed,a['photometric'])
        optimizer=tf.keras.optimizers.Adam(b['learning_rate']);history=[];domain_counts={}
        for epoch in range(b['epochs']):
            losses=[]
            for step in range(b['steps_per_epoch']):
                x,y,domains=sampler.next()
                for domain in domains:domain_counts[domain]=domain_counts.get(domain,0)+1
                with tf.device('/GPU:0'):
                    with tf.GradientTape() as tape:
                        prediction=model(tf.constant(x),training=True)
                        loss=pose_loss(tf.constant(y),prediction)
                    gradients=tape.gradient(loss,model.trainable_variables)
                    if any(g is None or 'GPU:0' not in g.device for g in gradients):raise RuntimeError('gradient off GPU')
                    optimizer.apply_gradients(zip(gradients,model.trainable_variables))
                value=float(loss.numpy())
                if not np.isfinite(value):raise ValueError('nonfinite loss')
                losses.append(value)
            history.append({'epoch':epoch+1,'loss':float(np.mean(losses))})
            print(a['id'],history[-1],flush=True)
        directory=output/a['id'];directory.mkdir()
        model.save(directory/'tinypose_float.keras')
        export=export_int8(model,pools['calibration'][0],directory)
        entry={'id':a['id'],'seed':seed,'device':prediction.device,'optimizer_updates':int(optimizer.iterations.numpy()),
               'history':history,'export':export,'domain_samples':domain_counts,'validation_refit':False,
               'checkpoint_selection':'fixed final epoch; no validation used for training/selection',
               'initial_checkpoint_sha256':initial_hash}
        (directory/'training-report.json').write_text(json.dumps(entry,indent=2)+'\n')
        evaluate_model(directory/'tinypose_keypoints_int8.tflite',path,directory/'evaluation.json')
        report['arms'].append(entry)
    verify_manifest(path)
    if sha256(initial)!=initial_hash or sha256(path)!=manifest_hash:raise ValueError('source changed')
    report.update(status='PASS',source_checkpoint_unchanged=True)
    (output/'training-summary.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'status':'PASS','arms':len(report['arms'])}))


def main():
    p=argparse.ArgumentParser(description=__doc__);subs=p.add_subparsers(dest='command',required=True)
    a=subs.add_parser('prepare');a.add_argument('--parent',type=pathlib.Path,required=True)
    a.add_argument('--output',type=pathlib.Path,required=True);a.add_argument('--kind',choices=('negative','mixed','seeds'),required=True)
    a=subs.add_parser('train');a.add_argument('--manifest',type=pathlib.Path,required=True);a.add_argument('--output',type=pathlib.Path,required=True)
    args=p.parse_args()
    if args.command=='prepare':prepare(args.parent,args.output,args.kind)
    else:train(args.manifest,args.output)


if __name__=='__main__':main()
