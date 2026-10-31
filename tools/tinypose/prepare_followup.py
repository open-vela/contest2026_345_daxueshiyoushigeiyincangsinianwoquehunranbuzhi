#!/usr/bin/env python3
"""Freeze a follow-up ablation without changing the original experiment."""
import argparse
import copy
import json
import pathlib
import shutil

from comparison_support import ROOT, verify_manifest
from dataset_import import sha256
from keypoint_dataset import load_jsonl


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    parent, output = args.parent.resolve(), args.output.resolve()
    if not output.is_relative_to(ROOT / '.secrets') or output.exists():
        parser.error('output must be a fresh directory under repository .secrets')
    # Copying labels verbatim is valid only if all relative image paths keep
    # resolving to the exact same files. Check this before creating outputs.
    manifest = copy.deepcopy(verify_manifest(parent))
    if manifest['budget']['batch_size'] != 32:
        raise ValueError('follow-up ratios require the original batch size of 32')
    for dataset in manifest['datasets'].values():
        old = parent.parent / dataset['path']
        new = output / dataset['path']
        for row in load_jsonl(old):
            if (old.parent / row['image']).resolve() != (new.parent / row['image']).resolve():
                raise ValueError('output location would change relative image paths')
    manifest['parent_manifest'] = str(parent.relative_to(ROOT))
    manifest['parent_manifest_sha256'] = sha256(parent)
    manifest['source_hashes'][str(parent.relative_to(ROOT))] = sha256(parent)
    for name in ('prepare_followup.py', 'summarize_comparison.py'):
        path = pathlib.Path('tools/tinypose') / name
        manifest['source_hashes'][str(path)] = sha256(ROOT / path)
    manifest['arms'] = [
        {'id': 'sc_only_photo', 'domain_counts': {'sc2336_train': 32}, 'photometric': True},
        {'id': 'sc_coco_125', 'domain_counts': {'sc2336_train': 28, 'coco_train': 4}, 'photometric': False},
        {'id': 'sc_coco_250', 'domain_counts': {'sc2336_train': 24, 'coco_train': 8}, 'photometric': False},
    ]
    manifest['followup_policy'] = {
        'order': 'SC-only photometric first, then 12.5% and 25% COCO without photometry',
        'comparison': 'same original checkpoint, seed, budget, calibration and acceptance gates',
        'validation_reuse': 'adaptive follow-up on previously inspected validation; not a fresh blind test',
    }
    manifest['limitations'].append(manifest['followup_policy']['validation_reuse'])
    output.mkdir(parents=True)
    for dataset in manifest['datasets'].values():
        target = output / dataset['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(parent.parent / dataset['path'], target)
    target = output / 'experiment.json'
    target.write_text(json.dumps(manifest, indent=2) + '\n')
    verify_manifest(target)
    print(json.dumps({'manifest': str(target), 'sha256': sha256(target), 'arms': manifest['arms']}))


if __name__ == '__main__':
    main()
