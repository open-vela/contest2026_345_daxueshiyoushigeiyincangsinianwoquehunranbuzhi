#!/usr/bin/env python3
"""Extract historical timing evidence; do not present it as a fresh board run."""
import argparse
import json
import pathlib
import re

from comparison_support import ROOT
from dataset_import import sha256

LOGS = {
    'invoke_only': 'hardware-logs/tinypose-portrait-model-v4c-p4-100round-20260920.log',
    'body_check_live': 'hardware-logs/tinypose-body-check-v28-af-final-acceptance-20260920.log',
    'demo_live': 'hardware-logs/velafit-demo-v30-20rep-final-20260920.log',
}
CODE = ('app/sc2336_probe/sc2336_probe_main.c',
        'app/velafit_ai/pipeline/velafit_pose_worker.c',
        'app/velafit_ai/models/velafit_tiny_pose_tflm.cc',
        'app/velafit_ai/algo/squat_fsm.c',
        'app/velafit_ai/pipeline/velafit_pipeline.c',
        'app/velafit_ai/pipeline/velafit_session.c')


def line_record(path, prefix):
    matches = [(i, line) for i, line in enumerate((ROOT/path).read_text().splitlines(), 1)
               if line.startswith(prefix)]
    if len(matches) != 1:
        raise ValueError(f'expected one record: {path} {prefix}')
    number, text = matches[0]
    values = {key: int(value) for key, value in re.findall(r'(\w+)=(\d+)(?=\s|$)', text)}
    return {'path': path, 'line': number, 'raw': text, 'integers': values}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT/'.secrets') or output.exists():
        parser.error('output must be fresh under repository .secrets')
    records = {
        'invoke_only': line_record(LOGS['invoke_only'], 'TINYPOSE-BENCH result='),
        'body_preview_mean': line_record(LOGS['body_check_live'], 'LCD PREVIEW MEAN'),
        'body_callback_and_frontend_age': line_record(LOGS['body_check_live'], 'BODY-CHECK timing'),
        'body_worker': line_record(LOGS['body_check_live'], 'PREVIEW POSE stats'),
        'demo_preview_mean': line_record(LOGS['demo_live'], 'LCD PREVIEW MEAN'),
        'demo_worker': line_record(LOGS['demo_live'], 'PREVIEW POSE stats'),
    }
    timing = records['demo_preview_mean']['integers']
    mean_loop = sum(timing[k] for k in ('capture_us', 'render_us', 'present_us'))
    report = {'status': 'HISTORICAL_EVIDENCE_AND_SOURCE_AUDIT_COMPLETE',
              'fresh_hardware_measurement': False,
              'source_hashes': {p: sha256(ROOT/p) for p in (*LOGS.values(), *CODE,
                                                         'tools/tinypose/audit_latency.py')},
              'records': records,
              'derived_demo_mean_loop_us': mean_loop,
              'derived_demo_mean_loop_fps': 1_000_000/mean_loop,
              'limitations': [
                  'render_us includes foreground model-RGB conversion, publish/latest, HUD; not only drawing.',
                  'Asynchronous callback overlaps foreground work; do not add its elapsed time to loop means.',
                  'capture_to_result is measured at frontend result retrieval, using pre-capture t0, not exposure-to-worker-completion.',
                  'Mean timings across logs/modes/measurement windows cannot be subtracted to infer stage costs.',
                  'PAN return does not establish the actual visible LCD scanout time.',
                  'No new model speed, rep latency distribution, or real-world count accuracy measured.',
              ]}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'status': report['status'], 'historical_demo_loop_us': mean_loop,
                      'historical_demo_loop_fps': report['derived_demo_mean_loop_fps']}))


if __name__ == '__main__':
    main()
