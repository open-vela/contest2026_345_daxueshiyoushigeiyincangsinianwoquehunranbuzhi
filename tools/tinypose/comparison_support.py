#!/usr/bin/env python3
"""Immutable input checks and matched-budget sampling for TinyPose comparisons."""
from __future__ import annotations

import hashlib
import json
import pathlib

import numpy as np

from dataset_import import sha256
from keypoint_dataset import KEYPOINTS, load_jsonl

ROOT = pathlib.Path(__file__).resolve().parents[2]


def record_digest(record):
    return hashlib.sha256(json.dumps(record, sort_keys=True).encode()).hexdigest()


def eligible(record):
    """Match the legacy loader's acceptance rule, including negative examples."""
    minimum = {'empty_background_negative': 0,
               'lower_body_cropped_negative': 4}.get(record.get('sample_kind'), 6)
    return sum(float(p['confidence']) >= 0.20 for p in record['keypoints']) >= minimum


def verify_approvals(labels_path, approval_paths):
    approved, pending = {}, set()
    for path in approval_paths:
        approval = json.loads(path.read_text())
        source = ROOT / approval['source_labels']
        if sha256(source) != approval['source_labels_sha256']:
            raise ValueError('reviewed source labels changed')
        preview = ROOT / approval['reviewed_preview']
        if sha256(preview) != approval['reviewed_preview_sha256']:
            raise ValueError('reviewed preview changed')
        for item in approval['records']:
            if item['decision'] == 'keep':
                if item['image'] in approved:
                    raise ValueError('duplicate approval')
                approved[item['image']] = item
            else:
                pending.add(item['image'])
    rows = load_jsonl(labels_path)
    if {row['image'] for row in rows} != set(approved):
        raise ValueError('selected labels do not exactly match approved samples')
    if {row['image'] for row in rows} & pending:
        raise ValueError('unapproved sample included')
    for row in rows:
        item = approved[row['image']]
        if record_digest(row) != item['record_sha256'] or row['sha256'] != item['image_sha256']:
            raise ValueError('approved record changed')
    return rows


def verify_manifest(path):
    manifest = json.loads(path.read_text())
    for source, digest in manifest['source_hashes'].items():
        if sha256(ROOT / source) != digest:
            raise ValueError(f'input changed: {source}')
    for name, dataset in manifest['datasets'].items():
        target = path.parent / dataset['path']
        if sha256(target) != dataset['sha256']:
            raise ValueError(f'frozen dataset changed: {name}')
    return manifest


def photometric(images, rng):
    """Change color only; preserve exact black padding and all label coordinates."""
    rgb = (images + 1.0) * 0.5
    foreground = np.any(rgb > 0, axis=-1, keepdims=True)
    shape = (len(images), 1, 1, 1)
    contrast = rng.uniform(0.8, 1.2, shape)
    gain = rng.uniform(0.7, 1.3, shape)
    gamma = rng.uniform(0.75, 1.4, shape)
    rgb = np.clip((rgb - 0.5) * contrast + 0.5, 0, 1)
    rgb = np.clip(np.power(rgb, gamma) * gain, 0, 1)
    rgb = np.clip(rgb + rng.normal(0, 0.01, rgb.shape), 0, 1)
    return (np.where(foreground, rgb, 0) * 2 - 1).astype(np.float32)


class BatchSampler:
    def __init__(self, pools, domain_counts, seed, use_photo=False):
        self.pools = pools
        self.counts = domain_counts
        self.rng = np.random.default_rng(seed)
        # Keep sample indices and flips identical for mixed vs mixed+photo.
        self.photo_rng = np.random.default_rng(seed + 1)
        self.use_photo = use_photo

    def next(self):
        xs, ys, domains = [], [], []
        for domain, count in self.counts.items():
            x, y = self.pools[domain]
            indices = self.rng.integers(0, len(x), count)
            xs.append(x[indices])
            ys.append(y[indices])
            domains.extend([domain] * count)
        x = np.concatenate(xs).copy()
        y = np.concatenate(ys).reshape(-1, 8, 3).copy()
        flips = self.rng.random(len(x)) < 0.5
        x[flips] = x[flips, :, ::-1, :]
        points = y[flips].copy()
        points[..., 0] = 1 - points[..., 0]
        y[flips] = points[:, (1, 0, 3, 2, 5, 4, 7, 6), :]
        if self.use_photo:
            x = photometric(x, self.photo_rng)
        return x, y.reshape(-1, 24), domains


def valid_keypoints(record):
    if [p['name'] for p in record['keypoints']] != list(KEYPOINTS):
        raise ValueError('keypoint order mismatch')
    if any(not np.isfinite(p[k]) or not 0 <= p[k] <= 1
           for p in record['keypoints'] for k in ('x', 'y', 'confidence')):
        raise ValueError('invalid normalized keypoint')
