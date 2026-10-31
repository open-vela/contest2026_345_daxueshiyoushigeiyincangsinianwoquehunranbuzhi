#!/usr/bin/env python3
"""Verify fixed-budget runs, evaluate unchanged gates and paired seed statistics."""
import json
import pathlib
import numpy as np
from comparison_support import ROOT, verify_manifest
from dataset_import import sha256
from summarize_comparison import evaluate_gates


def cases(r):
    def loc(name):
        x=r['results'][name]
        return {'pck_10px':x['pck10'],'mean_error_px':x['mean_error_px'],'p95_error_px':x['p95_error_px']}
    return {'sc2336_regression-clean':loc('sc-clean'),'coco_validation-clean':loc('coco-clean'),
            'sc2336_empty_development-clean':{'hidden_rejection_rate':r['results']['empty-clean']['hidden_rejection_030']},
            'sc2336_cropped_development-clean':{'hidden_rejection_rate':r['results']['cropped-clean']['hidden_rejection_030']}}


def verified_run(root):
    m=verify_manifest(root/'experiment.json')
    s=json.loads((root/'trained/training-summary.json').read_text())
    if not (s['status']=='PASS' and s['gpu_available'] and s['source_checkpoint_unchanged'] and
            s['manifest_sha256']==sha256(root/'experiment.json')):raise ValueError('invalid GPU summary')
    arms={a['id']:a for a in m['arms']}
    if {a['id'] for a in s['arms']}!=set(arms):raise ValueError('missing arm')
    for a in s['arms']:
        d=root/'trained'/a['id']
        if (a['seed']!=arms[a['id']]['seed'] or a['optimizer_updates']!=140 or len(a['history'])!=10 or
                a['validation_refit'] or 'GPU:0' not in a['device'] or a['export']['bytes']!=44304 or
                sha256(d/'tinypose_keypoints_int8.tflite')!=a['export']['sha256']):raise ValueError('training/export contract')
    return m,s


def main():
    base=ROOT/'.secrets/experiments'
    negative=base/'tinypose-hardening-v3-20261025'
    mixed=base/'tinypose-coco125-photo-v4-20261025'
    seeds=base/'tinypose-seed-replication-v5-20261025'
    load=lambda p:json.loads(p.read_text())
    baseline=load(negative/'v4c-evaluation.json')
    gates={}
    for root in (negative,mixed,seeds):
        m,s=verified_run(root)
        for a in s['arms']:
            r=load(root/'trained'/a['id']/'evaluation.json')
            if r['model_sha256']!=a['export']['sha256']:raise ValueError('evaluation model mismatch')
            gates[a['id']]=evaluate_gates(cases(baseline),cases(r),m['acceptance_proposal'])
    m=verify_manifest(seeds/'experiment.json')
    pairs={}
    # Reuse is checked against original frozen manifests, checkpoint, calibration and budget.
    for kind,name in [('plain','tinypose-coco-control-v1-20261024'),('photo','tinypose-followup-v2-20261025')]:
        old=verify_manifest(base/name/'experiment.json')
        if old['budget']!=m['budget'] or old['initial_checkpoint']!=m['initial_checkpoint'] or old['datasets']!=m['datasets']:
            raise ValueError('reuse settings differ')
        directory=ROOT/m['reuse']['34596_'+kind]
        report=load(directory/'training-report.json')
        if report['optimizer_updates']!=140 or report['domains']!={'sc2336_train':32} or report['validation_refit']:
            raise ValueError('reused arm contract differs')
        if sha256(directory/'tinypose_keypoints_int8.tflite')!=report['export']['sha256']:raise ValueError('reuse hash')
        pairs.setdefault(34596,{})[kind]=load(seeds/f'34596_{kind}-evaluation.json')
    for seed in (34597,34598):
        pairs[seed]={k:load(seeds/'trained'/f'sc_{seed}_{k}'/'evaluation.json') for k in ('plain','photo')}
    statistics={}
    for case in pairs[34596]['plain']['results']:
        statistics[case]={}
        for metric in ('pck10','mean_error_px','p95_error_px','counting_ready','hidden_rejection_030','hidden_rejection_020'):
            if pairs[34596]['plain']['results'][case][metric] is None:continue
            plain=np.array([pairs[s]['plain']['results'][case][metric] for s in (34596,34597,34598)])
            photo=np.array([pairs[s]['photo']['results'][case][metric] for s in (34596,34597,34598)])
            def stat(x):return {'mean':float(x.mean()),'sample_std':float(x.std(ddof=1)),'min':float(x.min()),'max':float(x.max())}
            statistics[case][metric]={'plain':stat(plain),'photo':stat(photo),'paired_photo_minus_plain':stat(photo-plain),
                                     'plain_values':plain.tolist(),'photo_values':photo.tolist()}
    report={'predeclared_seeds':[34596,34597,34598],'reuse_verified':True,'gates':gates,'statistics':statistics,
            'independent_real_acceptance':'PENDING','no_deployment':True,
            'note':'All seeds reported; sample std ddof=1; small reused sets, no confidence interval or rep accuracy claim.'}
    output=base/'tinypose-campaign-summary-20261025.json'
    with output.open('x') as f:json.dump(report,f,indent=2,allow_nan=False)
    print(json.dumps({'output':str(output),'gate_failures':{k:v['failed'] for k,v in gates.items()},
        'pck10':{case:statistics[case]['pck10'] for case in ('sc-clean','sc-dim','sc-bright','coco-clean')}},indent=2))


if __name__=='__main__':main()
