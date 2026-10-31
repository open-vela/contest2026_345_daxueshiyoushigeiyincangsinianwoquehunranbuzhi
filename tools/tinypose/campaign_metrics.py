"""Comparable INT8 localization and fixed-gate negative metrics for all candidates."""
import argparse
import json
import pathlib
import numpy as np

from comparison_support import verify_manifest
from dataset_import import sha256
from evaluate_negatives import mask_region
from keypoint_dataset import load_jsonl, read_ppm


def evaluate_model(model, manifest_path, output):
    import tensorflow as tf
    if output.exists():raise ValueError('evaluation output exists')
    m=verify_manifest(manifest_path)
    interpreter=tf.lite.Interpreter(model_path=str(model),num_threads=1,
        experimental_op_resolver_type=tf.lite.experimental.OpResolverType.BUILTIN_REF)
    interpreter.allocate_tensors();inp=interpreter.get_input_details()[0];out=interpreter.get_output_details()[0]
    results={}
    groups=[('sc','sc2336_regression','none'),('coco','coco_validation','none'),
            ('empty','sc2336_empty_development','none'),('cropped','sc2336_cropped_development','none'),
            ('ankles_black','sc2336_regression','ankles_black'),('ankles_gray','sc2336_regression','ankles_gray'),
            ('torso_gray','sc2336_regression','torso_gray'),('ankles_checker_unseen','sc2336_regression','ankles_checker')]
    for group,role,mask in groups:
        label_path=manifest_path.parent/m['datasets'][role]['path']
        records=load_jsonl(label_path)
        for stress,gain in (('clean',1.),('dim',.6),('bright',1.3)):
            errors=[];hidden=reject3=reject2=ready=strict=0
            for row in records:
                rgb=read_ppm(label_path.parent/row['image'])
                rgb,covered,rect=mask_region(rgb,row['keypoints'],'ankles_gray' if mask=='ankles_checker' else mask)
                if mask=='ankles_checker':
                    l,t,r,b=rect;yy,xx=np.indices((b-t,r-l))
                    texture=np.where(((xx//5+yy//5)%2)[...,None],np.array([35,70,160]),np.array([200,170,40]))
                    rgb[t:b,l:r]=texture
                image=tf.image.resize(rgb,(96,96),method='area').numpy()
                image=np.clip(image*gain,0,255)/127.5-1
                scale,zero=inp['quantization']
                q=np.clip(np.rint(image/scale+zero),-128,127).astype(np.int8)[None]
                interpreter.set_tensor(inp['index'],q);interpreter.invoke()
                scale,zero=out['quantization']
                points=((interpreter.get_tensor(out['index']).astype(np.float32)-zero)*scale).reshape(8,3)
                scores=np.clip(points[:,2],0,1)
                ready+=int(np.sum(scores>=.3)>=6 and np.all(scores>=.2))
                strict+=int(np.all(scores>=.3))
                for i,p in enumerate(row['keypoints']):
                    if p['confidence']>=.5 and not covered[i]:
                        errors.append(float(np.hypot(points[i,0]-p['x'],points[i,1]-p['y'])*96))
                    else:
                        hidden+=1;reject3+=int(scores[i]<.3);reject2+=int(scores[i]<.2)
            v=np.asarray(errors)
            results[group+'-'+stress]={'images':len(records),'visible_points':len(errors),'hidden_points':hidden,
                'pck10':float(np.mean(v<=10)) if len(v) else None,
                'mean_error_px':float(v.mean()) if len(v) else None,
                'p95_error_px':float(np.percentile(v,95)) if len(v) else None,
                'counting_ready':ready,'strict_ready':strict,
                'hidden_rejection_030':reject3/hidden if hidden else None,
                'hidden_rejection_020':reject2/hidden if hidden else None}
    report={'model_sha256':sha256(model),'manifest_sha256':sha256(manifest_path),
            'results':results,'real_independent_acceptance':'PENDING',
            'note':'Old regression/development data and synthetic diagnostics; COCO hidden includes unlabeled points.'}
    output.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print(json.dumps({'evaluated':str(model),'output':str(output)}),flush=True)
    return report


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--model',type=pathlib.Path,required=True)
    p.add_argument('--manifest',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True)
    a=p.parse_args();evaluate_model(a.model,a.manifest,a.output)
