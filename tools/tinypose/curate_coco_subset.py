#!/usr/bin/env python3
"""Create size-filtered label lists beside imported COCO images; retain raw pool."""
import argparse
import json
import pathlib

from dataset_import import review_sheet, sha256
from keypoint_dataset import load_jsonl, write_jsonl


def visible_span(record):
    points = [point for point in record['keypoints'] if point['usable']]
    if len(points) < 2:
        return 0.0
    # Horizontal floor poses must not be excluded solely for small Y range.
    return max(max(p[axis] for p in points) - min(p[axis] for p in points)
               for axis in ('x', 'y'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dataset', type=pathlib.Path, required=True)
    parser.add_argument('--min-body-span', type=float, default=0.30)
    parser.add_argument('--exclude-image-ids', type=int, nargs='*', default=[])
    args = parser.parse_args()
    if not 0 < args.min_body_span <= 1:
        parser.error('min-body-span must be in (0,1]')
    report_path = args.dataset / 'curation-report.json'
    paths = [args.dataset / split / filename for split in ('train', 'validation')
             for filename in ('labels-size-filtered.jsonl', 'size-review-queue.jsonl',
                              'review-size-filtered.png')]
    if report_path.exists() or any(path.exists() for path in paths):
        parser.error('curation output already exists; preserve it or use a new import directory')
    report = {'status': 'SIZE_FILTERED_PENDING_HUMAN_REVIEW',
              'min_visible_body_span': args.min_body_span,
              'exclude_image_ids': args.exclude_image_ids, 'splits': {},
              'note': 'Size gate only; not an exercise classifier or completed visual approval.'}
    for split in ('train', 'validation'):
        directory = args.dataset / split
        raw_path = directory / 'labels.jsonl'
        rows = load_jsonl(raw_path)
        kept, excluded = [], []
        for record in rows:
            reason = ('explicit_image_exclusion' if record['source_image_id'] in args.exclude_image_ids
                      else 'body_too_small_in_model_input' if visible_span(record) < args.min_body_span
                      else None)
            if reason:
                excluded.append({**record, 'curation_reason': reason})
            else:
                kept.append(record)
        if not kept:
            raise ValueError(f'no samples survived size gate: {split}')
        output = directory / 'labels-size-filtered.jsonl'
        write_jsonl(output, kept)
        write_jsonl(directory / 'size-review-queue.jsonl', excluded)
        review_sheet(kept, directory, directory / 'review-size-filtered.png')
        report['splits'][split] = {'raw': len(rows), 'kept': len(kept),
                                 'excluded': len(excluded), 'raw_labels_sha256': sha256(raw_path),
                                 'filtered_labels_sha256': sha256(output)}
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
