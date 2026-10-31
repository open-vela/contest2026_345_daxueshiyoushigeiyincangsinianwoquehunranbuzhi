#!/usr/bin/env python3
"""Re-run archived v4c manual holdout metrics without changing source assets."""
import argparse
import datetime
import hashlib
import importlib.metadata
import json
import math
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
MODEL = pathlib.Path('.secrets/tinypose-keypoints-portrait-model-v4c-20260920')
DATA = pathlib.Path('.secrets/tinypose-keypoints-portrait')
EXPECTED_SHA = 'a4713ca86cecc9d7ff31c1df2c8fa9cb495a8c32e0f98b97e3e0d6b2a15cf9c5'


def compare(expected, actual, location='root'):
    if isinstance(expected, dict):
        if expected.keys() != actual.keys():
            raise AssertionError(f'{location}: report fields differ')
        for key in expected:
            compare(expected[key], actual[key], f'{location}.{key}')
    elif isinstance(expected, float):
        if not math.isclose(expected, actual, rel_tol=0, abs_tol=1e-6):
            raise AssertionError(f'{location}: {expected} != {actual}')
    elif expected != actual:
        raise AssertionError(f'{location}: {expected} != {actual}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=pathlib.Path, required=True,
                        help='new report directory, relative to repository root')
    args = parser.parse_args()
    os.chdir(ROOT)
    args.output.mkdir(parents=True, exist_ok=False)
    model = MODEL / 'tinypose_keypoints_int8.tflite'
    if hashlib.sha256(model.read_bytes()).hexdigest() != EXPECTED_SHA:
        raise RuntimeError('v4c model SHA256 mismatch')
    holdout = [DATA / f'holdout-session{i:02d}-20260920/manual-labels.jsonl'
               for i in (1, 2)]
    cases = [
        ('quality-report.json', 'evaluate_keypoints.py', holdout),
        ('visibility-fullbody-report.json', 'evaluate_visibility.py', holdout),
        ('visibility-cropped-report.json', 'evaluate_visibility.py',
         [DATA / 'negative-ankles-cropped-session01-20260920/manual-labels.jsonl']),
        ('visibility-empty-background-report.json', 'evaluate_visibility.py',
         [DATA / 'empty-background-session01-20260920/manual-labels.jsonl']),
        ('visibility-close-development-report.json', 'evaluate_visibility.py',
         [DATA / 'train-close-fullbody-session01-20260920/manual-labels.jsonl']),
    ]
    files = {model, MODEL / 'tinypose_float.keras', MODEL / 'training-report.json',
             pathlib.Path('tools/tinypose/keypoint_dataset.py')}
    for name, script, labels in cases:
        files.add(MODEL / name)
        files.add(pathlib.Path('tools/tinypose') / script)
        for path in labels:
            files.add(path)
            for line in path.read_text().splitlines():
                record = json.loads(line)
                files.add(path.parent / record['image'])
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)}
    results = []
    env = {**os.environ, 'CUDA_VISIBLE_DEVICES': '-1', 'TF_CPP_MIN_LOG_LEVEL': '2'}
    for name, script, labels in cases:
        command = [sys.executable, str(ROOT / 'tools/tinypose' / script),
                   '--model', str(model), '--labels', *map(str, labels),
                   '--output', str(args.output / name)]
        process = subprocess.run(command, env=env, capture_output=True, text=True)
        (args.output / (name + '.log')).write_text(process.stdout + process.stderr)
        process.check_returncode()
        expected = json.loads((MODEL / name).read_text())
        actual = json.loads((args.output / name).read_text())
        compare(expected, actual)
        results.append({'report': name, 'archived_metrics_match': True})
        print(f'PASS {name}', flush=True)
    for path, digest in hashes.items():
        if hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'Source asset changed: {path}')
    report = {
        'status': 'PASS', 'model_sha256': EXPECTED_SHA,
        'created_at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'python': sys.version, 'executable': sys.executable,
        'packages': {package: importlib.metadata.version(package)
                     for package in ('numpy', 'keras', 'h5py')},
        'comparison_absolute_tolerance': 1e-6, 'results': results,
        'source_hashes': hashes, 'source_assets_unchanged': True,
        'evaluation_backend': 'CPU TFLite; GPU training is verified separately',
        'note': 'Close-development subset is not independent acceptance; '
                '24-frame holdout metrics are not squat rep accuracy.',
    }
    dependencies = subprocess.run([sys.executable, '-m', 'pip', 'freeze'],
                                  check=True, capture_output=True, text=True)
    (args.output / 'environment.lock.txt').write_text(dependencies.stdout)
    (args.output / 'reproduction.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
