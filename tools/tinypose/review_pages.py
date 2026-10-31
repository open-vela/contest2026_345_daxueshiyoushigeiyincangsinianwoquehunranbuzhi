#!/usr/bin/env python3
"""Generate numbered review pages without changing images or labels."""
import argparse
import json
import pathlib

from dataset_import import review_sheet, sha256
from keypoint_dataset import load_jsonl


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--labels', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--page-size', type=int, default=48)
    args = parser.parse_args()
    if not 1 <= args.page_size <= 48:
        parser.error('page-size must be between 1 and 48')
    rows = load_jsonl(args.labels)
    if not rows:
        parser.error('no labels to review')
    digest = sha256(args.labels)
    args.output.mkdir(parents=True, exist_ok=False)
    pages = []
    for offset in range(0, len(rows), args.page_size):
        subset = rows[offset:offset + args.page_size]
        start, end = offset + 1, offset + len(subset)
        name = f'review-{start:03d}-{end:03d}.png'
        review_sheet(subset, args.labels.parent, args.output / name,
                     limit=args.page_size, start_index=start)
        pages.append({'image': name, 'start': start, 'end': end,
                      'records': [row['image'] for row in subset]})
    if sha256(args.labels) != digest:
        raise RuntimeError('source labels changed during pagination')
    report = {'source_labels': str(args.labels), 'source_labels_sha256': digest,
              'total': len(rows), 'page_size': args.page_size, 'pages': pages}
    (args.output / 'pages.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'total': len(rows), 'pages': len(pages), 'output': str(args.output)}))


if __name__ == '__main__':
    main()
