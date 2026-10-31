#!/usr/bin/env python3
"""Freeze approved data, leakage checks and a fixed-budget v4c comparison plan."""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shlex

import numpy as np

from comparison_support import ROOT, eligible, valid_keypoints, verify_approvals
from dataset_import import sha256
from keypoint_dataset import load_jsonl, read_ppm, write_jsonl

MODEL = pathlib.Path('.secrets/tinypose-keypoints-portrait-model-v4c-20260920')
COCO = pathlib.Path('.secrets/datasets/coco-singleperson-v1')
SC = pathlib.Path('.secrets/tinypose-keypoints-portrait')
BASELINE_SHA = 'a4713ca86cecc9d7ff31c1df2c8fa9cb495a8c32e0f98b97e3e0d6b2a15cf9c5'


def v4c_sources():
    # Read an argv record; never execute shell text from historical logs.
    log = ROOT / 'hardware-logs/tinypose-portrait-model-v4c-training-20260920.log'
    first = log.read_text().splitlines()[0]
    command = re.search(r'COMMAND="(.*?)" TERM=', first).group(1)
    argv = shlex.split(command)
    a, b, c = argv.index('--train-labels'), argv.index('--validation-labels'), argv.index('--output')
    return [pathlib.Path(p) for p in argv[a+1:b] + argv[b+1:c]], log


def check_disjoint(groups):
    names = list(groups)
    for i, first in enumerate(names):
        for second in names[i+1:]:
            if groups[first] & groups[second]:
                raise ValueError(f'exact decoded image leakage: {first} / {second}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    os.chdir(ROOT)
    args.output = args.output.resolve()
    if not args.output.is_relative_to(ROOT / '.secrets'):
        parser.error('private frozen data must be under repository .secrets')
    if args.output.exists():
        parser.error('output must be a new directory')
    baseline = MODEL / 'tinypose_keypoints_int8.tflite'
    if sha256(baseline) != BASELINE_SHA:
        raise ValueError('v4c baseline hash changed')
    if sha256(MODEL / 'tinypose_float.keras') != '3dee944b4e9feed1a7b242e1f3602232a419d68fbf4f6f7bc50ee1986234199b':
        raise ValueError('v4c float checkpoint hash changed')
    train_labels = COCO / 'train/labels-size-filtered.jsonl'
    train_approvals = [COCO / 'train' / name for name in (
        'review-approval-001-048.json', 'review-approval-049-096.json',
        'review-approval-097-108.json')]
    val_labels = COCO / 'validation/labels-reviewed.jsonl'
    val_approval = COCO / 'validation/review-decisions-001-028.json'
    train = verify_approvals(train_labels, train_approvals)
    val = verify_approvals(val_labels, [val_approval])
    if (len(train), len(val)) != (108, 27):
        raise ValueError('unexpected reviewed dataset counts')
    pending_path = COCO / 'validation/labels-pending-review.jsonl'
    pending = load_jsonl(pending_path)
    if len(pending) != 1 or pending[0]['source_image_id'] != 371529:
        raise ValueError('pending validation sample changed')
    sc_paths, history = v4c_sources()
    sc_holdout = [SC / f'holdout-session{i:02d}-20260920/manual-labels.jsonl' for i in (1, 2)]
    source_files = {baseline, MODEL / 'tinypose_float.keras', MODEL / 'training-report.json',
                    history.relative_to(ROOT), train_labels, val_labels, pending_path,
                    *train_approvals, val_approval, *sc_paths, *sc_holdout}
    # Freeze all support code and the environment specification used by this run.
    source_files.update(pathlib.Path('tools/tinypose') / name for name in (
        'prepare_comparison.py', 'comparison_support.py', 'run_comparison.py',
        'evaluate_comparison.py', 'keypoint_dataset.py', 'train_keypoints.py',
        'gpu-requirements.lock.txt'))
    groups, datasets, skipped = {}, {}, []
    args.output.mkdir(parents=True)
    data_dir = args.output / 'data'
    data_dir.mkdir()

    def freeze(name, paths, training=False):
        rows, pixels_seen = [], set()
        for labels_path in paths:
            source_files.add(labels_path)
            for row in load_jsonl(labels_path):
                valid_keypoints(row)
                if training and not eligible(row):
                    skipped.append({'labels': str(labels_path), 'image': row['image'],
                                    'reason': 'legacy_loader_insufficient_visible_points'})
                    continue
                image_path = (labels_path.parent / row['image']).resolve()
                image = read_ppm(image_path)
                if image.shape != (192, 192, 3):
                    raise ValueError(f'not RGB192: {image_path}')
                digest = sha256(image_path)
                if row.get('sha256') and row['sha256'] != digest:
                    raise ValueError(f'image metadata hash mismatch: {image_path}')
                pixel_digest = hashlib.sha256(image.tobytes()).hexdigest()
                if pixel_digest in pixels_seen:
                    raise ValueError(f'duplicate decoded image in {name}')
                pixels_seen.add(pixel_digest)
                source_files.add(image_path.relative_to(ROOT))
                rows.append({**row, 'image': os.path.relpath(image_path, data_dir),
                             'frozen_image_sha256': digest, 'decoded_rgb_sha256': pixel_digest,
                             'original_labels': str(labels_path), 'original_image': row['image'],
                             'experiment_role': name})
        target = data_dir / f'{name}.jsonl'
        write_jsonl(target, rows)
        groups[name] = pixels_seen
        datasets[name] = {'path': str(target.relative_to(args.output)), 'count': len(rows),
                          'sha256': sha256(target)}
        return rows

    sc_rows = freeze('sc2336_train', sc_paths, training=True)
    coco_rows = freeze('coco_train', [train_labels], training=True)
    freeze('coco_validation', [val_labels])
    freeze('sc2336_regression', sc_holdout)
    if (len(sc_rows), len(coco_rows), datasets['sc2336_regression']['count']) != (210, 108, 24):
        raise ValueError('archived SC2336 or approved COCO sample counts differ')
    check_disjoint(groups)
    for row in pending:
        pixels = read_ppm(pending_path.parent / row['image'])
        digest = hashlib.sha256(pixels.tobytes()).hexdigest()
        if any(digest in values for values in groups.values()):
            raise ValueError('pending sample leaked into experiment')

    # These are historical development negatives, not independent test sets.
    freeze('sc2336_empty_development', [SC / 'empty-background-session01-20260920/manual-labels.jsonl'])
    freeze('sc2336_cropped_development', [SC / 'negative-ankles-cropped-session01-20260920/manual-labels.jsonl'])

    # Same calibration data for all trained arms; no validation/holdout images.
    rng = np.random.default_rng(34596)
    calibration = []
    for rows in (sc_rows, coco_rows):
        calibration.extend(rows[i] for i in rng.choice(len(rows), 50, replace=False))
    target = data_dir / 'calibration.jsonl'
    write_jsonl(target, calibration)
    datasets['calibration'] = {'path': str(target.relative_to(args.output)),
                               'count': 100, 'sha256': sha256(target)}
    source_hashes = {str(path): sha256(path) for path in sorted(source_files)}
    manifest = {
        'schema_version': 1, 'status': 'PREPARED_NOT_TRAINED',
        'baseline_model': str(baseline), 'initial_checkpoint': str(MODEL / 'tinypose_float.keras'),
        'datasets': datasets, 'source_hashes': source_hashes,
        'leakage_checks': {'main_four_roles_decoded_image_overlap': 0,
                           'pending_sample_included': False,
                           'development_negatives_may_overlap_training': True},
        'skipped_legacy_records': skipped,
        'budget': {'seed': 34596, 'epochs': 10, 'steps_per_epoch': 14,
                   'batch_size': 32, 'learning_rate': 0.0002},
        'arms': [
            {'id': 'sc_only', 'domain_counts': {'sc2336_train': 32}, 'photometric': False},
            {'id': 'sc_coco', 'domain_counts': {'sc2336_train': 16, 'coco_train': 16}, 'photometric': False},
            {'id': 'sc_coco_photo', 'domain_counts': {'sc2336_train': 16, 'coco_train': 16}, 'photometric': True},
        ],
        'training_policy': {
            'validation_never_refit': True, 'checkpoint_selection': 'fixed final epoch, no early stopping',
            'sampling': 'with replacement; equal optimizer updates; horizontal flip p=0.5',
            'confidence': 'unchanged legacy soft scores and COCO binary visibility targets',
            'quantization': 'INT8, same fixed 100 training-only calibration images for all trained arms',
        },
        'acceptance_proposal': {
            'sc2336_pck10_max_drop': 0.01, 'sc2336_mean_error_max_increase_px': 0.5,
            'sc2336_p95_error_max_increase_px': 1.0,
            'development_hidden_rejection_max_drop': 0.02,
            'coco_pck10_target_gain': 0.05,
            'note': 'engineering comparison gates, not statistical proof or rep-count accuracy',
        },
        'limitations': [
            'SC2336 24-frame regression set has been used in historical acceptance; not a fresh blind test.',
            'COCO official image split does not guarantee unseen person identities.',
            'Empty/cropped development negatives overlap training; they only check regressions.',
            'No new person videos, Fit3D data, or pending validation #28 are included.',
            'Synthetic lighting stress is not SC2336 real-lighting acceptance.',
            'Fixed calibration differs from historical v4c; use sc_only to control extra fine-tuning/export effects.',
        ],
    }
    output = args.output / 'experiment.json'
    output.write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'status': manifest['status'], 'datasets': datasets,
                      'skipped_legacy_records': len(skipped), 'source_files': len(source_hashes),
                      'manifest': str(output)}, indent=2))


if __name__ == '__main__':
    main()
