#!/usr/bin/env python3
"""Evaluate INT8 coordinates/visibility on manual or COCO ground truth, never teacher labels."""
import argparse
from collections import Counter
import json
import pathlib

import numpy as np
import tensorflow as tf

from comparison_support import valid_keypoints
from dataset_import import sha256
from keypoint_dataset import KEYPOINTS, load_jsonl, read_ppm


def evaluate(model, labels, stress='clean'):
    interpreter = tf.lite.Interpreter(model_path=str(model), num_threads=1)
    interpreter.allocate_tensors()
    inp, out = interpreter.get_input_details()[0], interpreter.get_output_details()[0]
    if (list(inp['shape']) != [1, 96, 96, 3] or list(out['shape']) != [1, 24]
            or inp['dtype'] != np.int8 or out['dtype'] != np.int8):
        raise ValueError('unexpected TinyPose INT8 contract')
    errors, by_joint, sources = [], {name: [] for name in KEYPOINTS}, Counter()
    visible = hidden = accepted_visible = rejected_hidden = all_eight = images = 0
    for path in labels:
        for row in load_jsonl(path):
            if row.get('label_source') not in ('manual', 'coco_ground_truth'):
                raise ValueError('evaluation requires independent annotations, not teacher labels')
            valid_keypoints(row)
            rgb = tf.image.resize(read_ppm(path.parent / row['image']), (96, 96), method='area').numpy()
            if stress != 'clean':
                black = np.all(rgb == 0, axis=-1, keepdims=True)
                rgb = np.clip(rgb * (0.6 if stress == 'dim' else 1.3), 0, 255)
                rgb = np.where(black, 0, rgb)
            image = rgb / 127.5 - 1.0
            scale, zero = inp['quantization']
            quantized = np.clip(np.rint(image / scale + zero), -128, 127).astype(np.int8)[None]
            interpreter.set_tensor(inp['index'], quantized)
            interpreter.invoke()
            scale, zero = out['quantization']
            points = ((interpreter.get_tensor(out['index']).astype(np.float32) - zero) * scale).reshape(8, 3)
            scores = np.clip(points[:, 2], 0, 1)
            all_eight += int(np.all(scores >= 0.3))
            images += 1
            sources[row['label_source']] += 1
            for i, expected in enumerate(row['keypoints']):
                if expected['confidence'] >= 0.5:
                    error = float(np.hypot(points[i, 0] - float(expected['x']),
                                           points[i, 1] - float(expected['y'])) * 96)
                    errors.append(error)
                    by_joint[KEYPOINTS[i]].append(error)
                    visible += 1
                    accepted_visible += int(scores[i] >= 0.3)
                else:
                    hidden += 1
                    rejected_hidden += int(scores[i] < 0.3)
    values = np.asarray(errors)
    if not images:
        raise ValueError('empty evaluation set')
    return {
        'model': str(model), 'model_sha256': sha256(model), 'images': images,
        'label_sources': dict(sources), 'stress': stress,
        'stress_note': 'synthetic gain only, not proof of real lighting generalization',
        'normalization': '96x96 image pixels', 'visible_keypoints': visible,
        'mean_error_px': float(values.mean()) if visible else None,
        'median_error_px': float(np.median(values)) if visible else None,
        'p95_error_px': float(np.percentile(values, 95)) if visible else None,
        'pck_5px': float(np.mean(values <= 5)) if visible else None,
        'pck_10px': float(np.mean(values <= 10)) if visible else None,
        'per_keypoint_mean_error_px': {name: float(np.mean(v)) if v else None
                                        for name, v in by_joint.items()},
        'visibility_threshold': 0.3, 'hidden_keypoints': hidden,
        'visible_recall': accepted_visible / visible if visible else None,
        'hidden_rejection_rate': rejected_hidden / hidden if hidden else None,
        'all_eight_accepted_rate': all_eight / images,
        'labels_sha256': {str(path): sha256(path) for path in labels},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=pathlib.Path, required=True)
    parser.add_argument('--labels', type=pathlib.Path, nargs='+', required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--stress', choices=('clean', 'dim', 'bright'), default='clean')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('output already exists')
    report = evaluate(args.model, args.labels, args.stress)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
