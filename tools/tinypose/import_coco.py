#!/usr/bin/env python3
"""Import a deterministic, license-filtered, single annotated person COCO subset."""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor
import json
import math
import pathlib
import random
import re
import time
import urllib.request
import zipfile

from PIL import Image

from dataset_import import (COCO_KEYPOINTS, MOVENET_INDICES, coco_points, frame_image,
                            image_quality, review_sheet, save_frame, sha256, validate_record)
from keypoint_dataset import write_jsonl

COCO_BASE = 'https://s3.amazonaws.com/images.cocodataset.org'


def posture_bucket(annotation):
    raw = annotation['keypoints']
    angles = []
    for indexes in ((11, 13, 15), (12, 14, 16)):
        if any(raw[i * 3 + 2] != 2 for i in indexes):
            continue
        a, b, c = [raw[i*3:i*3+2] for i in indexes]
        u, v = [a[i]-b[i] for i in (0, 1)], [c[i]-b[i] for i in (0, 1)]
        length = math.hypot(*u) * math.hypot(*v)
        if length:
            angles.append(math.degrees(math.acos(max(-1, min(1,
                sum(u[i]*v[i] for i in (0, 1)) / length)))))
    if not angles:
        return 'unknown_knee'
    return 'deep_bend' if min(angles) < 100 else 'bent' if min(angles) < 155 else 'straight'


def select_records(data, limit, seed, licenses, minimum_visible):
    people = defaultdict(list)
    for ann in data['annotations']:
        if ann['category_id'] == 1:
            people[ann['image_id']].append(ann)
    category = next(c for c in data['categories'] if c['id'] == 1)
    if tuple(category['keypoints']) != COCO_KEYPOINTS:
        raise ValueError('unexpected COCO keypoint order')
    buckets, rejected = defaultdict(list), Counter()
    for image in data['images']:
        annotations = people[image['id']]
        if len(annotations) != 1 or annotations[0].get('iscrowd', 0):
            rejected['not_single_annotated_person'] += 1
            continue
        ann = annotations[0]
        if image.get('license') not in licenses:
            rejected['license_not_selected'] += 1
            continue
        points = ann['keypoints']
        if len(points) != 51:
            rejected['missing_keypoints'] += 1
            continue
        visible = sum(points[i*3+2] == 2 for i in MOVENET_INDICES)
        if visible < minimum_visible or ann['bbox'][3] < 100:
            rejected['too_small_or_insufficient_visible_points'] += 1
            continue
        if any(points[i*3+2] and not (0 <= points[i*3] < image['width'] and
               0 <= points[i*3+1] < image['height']) for i in MOVENET_INDICES):
            rejected['annotated_point_outside_image'] += 1
            continue
        bucket = posture_bucket(ann)
        buckets[bucket].append((image, ann, bucket))
    eligible = {key: len(value) for key, value in buckets.items()}
    rng = random.Random(seed)
    for key in sorted(buckets):
        buckets[key].sort(key=lambda item: item[0]['id'])
        rng.shuffle(buckets[key])
    selected = []
    # Round robin across pose bins; bin names are geometry, not exercise labels.
    while len(selected) < limit:
        before = len(selected)
        for key in sorted(buckets):
            if buckets[key] and len(selected) < limit:
                selected.append(buckets[key].pop())
        if len(selected) == before:
            break
    return selected, {'eligible_by_posture': eligible, 'rejected': dict(rejected)}


def download_image(split, image, cache):
    filename = image['file_name']
    if not re.fullmatch(r'\d{12}\.jpg', filename):
        raise ValueError('unexpected COCO filename')
    path = cache / split / filename
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        with Image.open(path) as decoded:
            decoded.verify()
        return path
    partial = path.with_suffix('.jpg.part')
    url = f'{COCO_BASE}/{split}/{filename}'
    for attempt in range(4):
        try:
            with urllib.request.urlopen(url, timeout=60) as response, partial.open('wb') as stream:
                while block := response.read(1024 * 1024):
                    stream.write(block)
            with Image.open(partial) as decoded:
                decoded.verify()
            partial.replace(path)
            return path
        except (OSError, ValueError):
            if attempt == 3:
                raise
            time.sleep(attempt + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--annotations-zip', type=pathlib.Path, required=True)
    parser.add_argument('--cache', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--train-count', type=int, default=200)
    parser.add_argument('--validation-count', type=int, default=50)
    parser.add_argument('--seed', type=int, default=34596)
    parser.add_argument('--license-ids', type=int, nargs='+', default=[4],
                        help='default COCO license 4 (CC BY); preserve per-image attribution')
    parser.add_argument('--min-visible', type=int, choices=(6, 7, 8), default=6)
    parser.add_argument('--framing', choices=('full-frame', 'person-context'), default='full-frame')
    parser.add_argument('--download', action='store_true', help='download selected images only')
    args = parser.parse_args()
    if args.train_count < 1 or args.validation_count < 1:
        parser.error('counts must be positive')
    args.output.mkdir(parents=True, exist_ok=False)
    report = {'source': 'COCO 2017', 'annotations_sha256': sha256(args.annotations_zip),
              'seed': args.seed, 'license_ids': args.license_ids, 'framing': args.framing,
              'split_guarantee': 'official image split; COCO does not supply person identities',
              'visibility_policy': 'v2=visible target 1; v1/v0=target 0, coordinates masked in loss',
              'exercise_labels': 'none; knee-angle bins are not squat/burpee labels', 'splits': {}}
    hashes_by_split = []
    with zipfile.ZipFile(args.annotations_zip) as archive:
        for split, count in (('train2017', args.train_count), ('val2017', args.validation_count)):
            with archive.open(f'annotations/person_keypoints_{split}.json') as stream:
                data = json.load(stream)
            selected, stats = select_records(data, count, args.seed, set(args.license_ids), args.min_visible)
            if len(selected) != count:
                raise ValueError(f'{split}: only {len(selected)} eligible images; requested {count}')
            licensing = {item['id']: item for item in data['licenses']}
            directory = args.output / ('train' if split == 'train2017' else 'validation')
            directory.mkdir()
            if args.download:
                with ThreadPoolExecutor(max_workers=4) as pool:
                    sources = list(pool.map(lambda row: download_image(split, row[0], args.cache), selected))
            else:
                sources = [args.cache / split / image['file_name'] for image, _, _ in selected]
            records = []
            for index, ((image, ann, bucket), source) in enumerate(zip(selected, sources)):
                with Image.open(source) as decoded:
                    if decoded.size != (image['width'], image['height']):
                        raise ValueError(f'image dimensions do not match annotations: {source}')
                    crop = None
                    if args.framing == 'person-context':
                        x, y, w, h = ann['bbox']
                        side = max(w, h) * 1.5
                        cx, cy = x + w/2, y + h/2
                        crop = (cx-side/2, cy-side/2, cx+side/2, cy+side/2)
                    framed, transform = frame_image(decoded, crop)
                points, points17 = coco_points(ann['keypoints'], transform)
                record = {
                    **save_frame(framed, directory, f'coco-{image["id"]:012d}.ppm'),
                    'source': 'coco2017', 'source_image_id': image['id'],
                    'source_annotation_id': ann['id'], 'source_split': split,
                    'split': directory.name, 'group_id': f'coco-image-{image["id"]}',
                    'source_sha256': sha256(source), 'source_url': image.get('coco_url'),
                    'attribution_url': image.get('flickr_url'), 'license': licensing[image['license']],
                    'transform': transform, 'capture_index': index, 'posture_bin': bucket,
                    'keypoints': points, 'keypoints17': points17,
                    'source_keypoints_coco': ann['keypoints'],
                    'label_source': 'coco_ground_truth',
                    'review_status': 'automatic_import_requires_visual_review',
                    'confidence_semantics': 'binary_visibility_target_not_teacher_probability',
                    'quality': image_quality(framed),
                }
                validate_record(record, directory)
                records.append(record)
            write_jsonl(directory / 'labels.jsonl', records)
            write_jsonl(directory / 'manifest.jsonl', [
                {k: v for k, v in row.items() if k not in ('keypoints', 'keypoints17', 'source_keypoints_coco')}
                for row in records])
            review_sheet(records, directory, directory / 'review.png')
            hashes_by_split.append({row['source_sha256'] for row in records})
            report['splits'][directory.name] = {
                **stats, 'images': len(records),
                'selected_by_posture': dict(Counter(row['posture_bin'] for row in records)),
                'visible_keypoints': sum(sum(p['usable'] for p in row['keypoints']) for row in records),
                'labels_sha256': sha256(directory / 'labels.jsonl'),
            }
            print(f'PASS {split}: {len(records)} images', flush=True)
            del data
    overlap = hashes_by_split[0] & hashes_by_split[1]
    if overlap:
        raise ValueError('identical source images found across splits')
    report['cross_split_source_hash_overlap'] = 0
    report['status'] = 'IMPORTED_PENDING_VISUAL_REVIEW'
    (args.output / 'import-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
