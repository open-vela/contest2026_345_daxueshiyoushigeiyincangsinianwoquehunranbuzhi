#!/usr/bin/env python3
"""Inventory frozen data and reject cross-split ancestry/person/session leakage."""
import argparse
from collections import Counter, defaultdict
import json
import pathlib

from comparison_support import ROOT, verify_manifest
from dataset_import import sha256
from keypoint_dataset import load_jsonl

FAMILIES={'train':'train','calibration':'train','development':'train',
          'validation_used':'validation_used','acceptance':'acceptance','pending':'pending'}


def audit(records, verify_files=False):
    if not records:return {'status':'PENDING_NO_RECORDINGS','issues':[],'independent_acceptance_records':0}
    issues=[];warnings=[];groups=defaultdict(list);ids=set();acceptance=0
    for r in records:
        if r['id'] in ids:issues.append({'kind':'duplicate_record_id','id':r['id']})
        ids.add(r['id'])
        if r['split'] not in FAMILIES:raise ValueError('unknown split')
        family=FAMILIES[r['split']]
        if family=='pending':continue
        for field in ('sha256','decoded_rgb_sha256','root_source_sha256','subject_id','session_id','background_id'):
            if field=='subject_id' and r.get(field)=='NO_PERSON':
                if r.get('no_person_confirmed') is not True:
                    issues.append({'kind':'empty_scene_not_confirmed','id':r['id']})
                continue
            if r.get(field):groups[field,r[field]].append((family,r['id']))
        # An ancestor may be another raw record's own hash; put all into same namespace.
        for digest in {r.get('sha256'),r.get('root_source_sha256'),*r.get('ancestor_sha256',[])}-{None,''}:
            groups['source_lineage',digest].append((family,r['id']))
        if verify_files:
            path=ROOT/r['path']
            if not path.is_file() or sha256(path)!=r['sha256']:issues.append({'kind':'file_hash','id':r['id']})
        missing=[k for k in ('subject_id','session_id','background_id','root_source_sha256') if not r.get(k)]
        if family=='acceptance':
            acceptance+=1
            if missing:issues.append({'kind':'acceptance_provenance_missing','id':r['id'],'fields':missing})
            if r.get('used_for_training') or r.get('used_for_tuning'):
                issues.append({'kind':'acceptance_already_used','id':r['id']})
        elif missing:warnings.append({'id':r['id'],'unknown_fields':missing})
    for (field,value),entries in groups.items():
        families={e[0] for e in entries}
        if len(families)>1 and (field!='background_id' or 'acceptance' in families):
            issues.append({'kind':'cross_split_'+field,'value':value,'entries':entries})
    return {'status':'FAIL' if issues else 'PROVENANCE_CHECK_PASS' if acceptance else 'LEGACY_ONLY_NO_INDEPENDENT_ACCEPTANCE',
            'issues':issues,'legacy_provenance_warnings':warnings,'independent_acceptance_records':acceptance,
            'counts':dict(Counter(r['split'] for r in records)),
            'note':'PASS covers declared identifiers/hash ancestry, not unrecorded identities or unseen external data.'}


def inventory(manifest, output):
    manifest=manifest.resolve();m=verify_manifest(manifest)
    if output.exists() or not output.resolve().is_relative_to(ROOT/'.secrets'):raise ValueError('fresh private directory required')
    roles={'sc2336_train':'train','coco_train':'train','calibration':'calibration',
           'sc2336_regression':'validation_used','coco_validation':'validation_used',
           'sc2336_empty_development':'development','sc2336_cropped_development':'development'}
    records=[]
    for role,split in roles.items():
        labels=manifest.parent/m['datasets'][role]['path']
        for i,row in enumerate(load_jsonl(labels)):
            path=(labels.parent/row['image']).resolve()
            records.append({'id':f'{role}:{i}','path':str(path.relative_to(ROOT)),'sha256':sha256(path),
                'decoded_rgb_sha256':row.get('decoded_rgb_sha256'),
                'root_source_sha256':row.get('source_sha256'),
                'subject_id':row.get('subject_id'),'session_id':row.get('session'),
                'background_id':None,'split':split,'original_role':role,
                'source_labels':str(labels.relative_to(ROOT)),'source_labels_sha256':sha256(labels),
                'annotation_source':row.get('label_source'),'used_for_training':split in ('train','calibration','development'),
                'used_for_tuning':split=='validation_used',
                'identity_note':'unknown person IDs remain null; session is not a person ID'})
    output.mkdir(parents=True)
    document={'source_manifest':str(manifest.relative_to(ROOT)),'source_manifest_sha256':sha256(manifest),'records':records}
    (output/'registry.json').write_text(json.dumps(document,indent=2)+'\n')
    result=audit(records,True);(output/'audit.json').write_text(json.dumps(result,indent=2)+'\n')
    template={'status':'PENDING_RECORDING_AND_MANUAL_LABELS','records':[],
        'required_each_record':{'id':'unique','path':'relative repository path','sha256':'actual file digest',
            'root_source_sha256':'original uncut video digest','ancestor_sha256':[],
            'subject_id':'stable participant ID OR NO_PERSON for explicitly confirmed empty scene',
            'no_person_confirmed':False,
            'session_id':'unique recording occasion','background_id':'stable scene ID',
            'split':'train OR validation_used OR acceptance','used_for_training':False,'used_for_tuning':False},
        'capture_plan':[{'purpose':'development training','people':'dedicated training participants','split':'train'},
            {'purpose':'tuning','people':'different from training','split':'validation_used'},
            {'purpose':'locked independent acceptance','people':'different from both','split':'acceptance'}],
        'conditions':['normal','dim','bright','empty','partial_body','ankles_cloth','ankles_board','torso_occlusion'],
        'rules':['All clips/frames/crops/augmentations from a source keep the same split.',
                 'Acceptance subjects/sessions/backgrounds must not overlap training/tuning.',
                 'Known empty scene uses subject_id=NO_PERSON, but its raw source/session/background stay isolated.',
                 'Do not train/calibrate on acceptance or rename old validation as new blind test.']}
    (output/'capture-template.json').write_text(json.dumps(template,indent=2)+'\n')
    print(json.dumps({'status':result['status'],'counts':result['counts'],'issues':len(result['issues']),
                      'independent_acceptance_records':result['independent_acceptance_records']}))


def main():
    p=argparse.ArgumentParser();s=p.add_subparsers(dest='command',required=True)
    a=s.add_parser('inventory');a.add_argument('--manifest',type=pathlib.Path,required=True);a.add_argument('--output',type=pathlib.Path,required=True)
    a=s.add_parser('check');a.add_argument('--registry',type=pathlib.Path,required=True);a.add_argument('--output',type=pathlib.Path,required=True)
    a=p.parse_args()
    if a.command=='inventory':inventory(a.manifest,a.output)
    else:
        if a.output.exists():raise ValueError('output exists')
        r=audit(json.loads(a.registry.read_text())['records'],True)
        a.output.write_text(json.dumps(r,indent=2)+'\n');print(r['status'])
        if r['status']=='FAIL':raise SystemExit(1)


if __name__=='__main__':main()
