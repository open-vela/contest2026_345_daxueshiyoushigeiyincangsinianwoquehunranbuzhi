#!/usr/bin/env python3
"""Paired negative/occlusion diagnostics; static inputs never imply rep accuracy."""
import argparse
from collections import Counter
import ctypes
import json
import pathlib
import subprocess

import numpy as np
from PIL import Image, ImageDraw

from comparison_support import ROOT, verify_manifest
from dataset_import import sha256
from keypoint_dataset import KEYPOINTS, load_jsonl, read_ppm
from video_replay import Bridge, build_bridge, EXPECTED_MODELS, PHOTO, SOURCES, V4C

EXTRA_SOURCES = ('tools/tinypose/evaluate_negatives.py', 'tools/tinypose/negative_probe_bridge.c',
                 'app/velafit_ai/algo/velafit_body_check.c', 'app/velafit_ai/algo/velafit_body_check.h')
STATUSES = ('LOW_CONFIDENCE', 'NOT_FULLY_VISIBLE', 'TOO_CLOSE', 'TOO_FAR',
            'ORIENTATION_INVALID', 'STABILIZING', 'READY')


def mask_region(rgb, points, kind):
    """Diagnostic occluder covers exact known joint centers; no training labels saved."""
    result = rgb.copy()
    if kind == 'none':
        return result, np.zeros(8, dtype=bool), None
    xy = np.array([[p['x'], p['y']] for p in points])*192
    if kind in ('ankles_black', 'ankles_gray'):
        # Full-width occluder strictly below the midpoint between knees/ankles.
        top = int(np.floor((xy[4:6, 1].mean()+xy[6:8, 1].mean())*.5))
        rect = (0, max(0, min(191, top)), 192, 192)
        fill = 0 if kind == 'ankles_black' else 128
    elif kind == 'torso_gray':
        rect = (max(0, int(np.floor(xy[:4, 0].min()))-6),
                max(0, int(np.floor(xy[:4, 1].min()))-6),
                min(192, int(np.ceil(xy[:4, 0].max()))+7),
                min(192, int(np.ceil(xy[:4, 1].max()))+7))
        fill = 128
    else:
        raise ValueError('unknown diagnostic mask')
    left, top, right, bottom = rect
    result[top:bottom, left:right] = fill
    covered = (xy[:, 0] >= left) & (xy[:, 0] < right) & (xy[:, 1] >= top) & (xy[:, 1] < bottom)
    return result, covered, rect


def summarize(rows):
    hidden = sum(r['hidden_count'] for r in rows)
    return {'images': len(rows), 'hidden_points': hidden,
            'hidden_rejection_at_030': sum(r['hidden_rejected_030'] for r in rows)/hidden if hidden else None,
            'hidden_rejection_at_020': sum(r['hidden_rejected_020'] for r in rows)/hidden if hidden else None,
            'strict_all_eight_accepted': sum(r['strict_all_eight'] for r in rows),
            'counting_ready_frames': sum(r['counting_ready'] for r in rows),
            'body_single_frame_geometry_pass': sum(r['body_status'] in ('STABILIZING', 'READY') for r in rows),
            'body_status_counts': dict(Counter(r['body_status'] for r in rows)),
            'hidden_reason_counts': dict(sum((Counter(r['hidden_reasons']) for r in rows), Counter()))}


def main():
    import tensorflow as tf
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    manifest_path, output = args.manifest.resolve(), args.output.resolve()
    if not output.is_relative_to(ROOT/'.secrets') or output.exists():
        parser.error('output must be fresh under repository .secrets')
    manifest = verify_manifest(manifest_path)
    input_hash = sha256(manifest_path)
    sources = {p: sha256(ROOT/p) for p in (*SOURCES, *EXTRA_SOURCES)}
    models = {'v4c': V4C, 'sc_only_photo': PHOTO}
    if any(sha256(p) != EXPECTED_MODELS[n] for n, p in models.items()):
        raise ValueError('model identity changed')
    output.mkdir(parents=True)
    bridge = Bridge(build_bridge(output))
    native = output/'negative_probe.so'
    subprocess.run(['cc', '-std=c99', '-O2', '-shared', '-fPIC',
        '-I'+str(ROOT/'app/velafit_ai/include'), '-I'+str(ROOT/'app/velafit_ai/algo'),
        str(ROOT/EXTRA_SOURCES[1]), str(ROOT/EXTRA_SOURCES[2]), '-lm', '-o', str(native)], check=True)
    lib = ctypes.CDLL(str(native))
    fp = ctypes.POINTER(ctypes.c_float)
    lib.vf_negative_probe.argtypes = [fp, fp]
    interpreters = {}
    for name, path in models.items():
        interpreter = tf.lite.Interpreter(model_path=str(path), num_threads=1,
            experimental_op_resolver_type=tf.lite.experimental.OpResolverType.BUILTIN_REF)
        interpreter.allocate_tensors()
        inp, out = interpreter.get_input_details()[0], interpreter.get_output_details()[0]
        if (list(inp['shape']) != [1, 96, 96, 3] or list(out['shape']) != [1, 24] or
                inp['dtype'] != np.int8 or out['dtype'] != np.int8):
            raise ValueError('unexpected model contract')
        interpreters[name] = interpreter, inp, out
    groups = (
        ('sc_control', 'sc2336_regression', 'none'),
        ('empty_real', 'sc2336_empty_development', 'none'),
        ('cropped_real', 'sc2336_cropped_development', 'none'),
        ('coco_hidden', 'coco_validation', 'none'),
        ('ankles_black_synthetic', 'sc2336_regression', 'ankles_black'),
        ('ankles_gray_synthetic', 'sc2336_regression', 'ankles_gray'),
        ('torso_gray_synthetic', 'sc2336_regression', 'torso_gray'),
    )
    results = {}
    image_hashes = {}
    counts = Counter()
    preview = []
    detail = output/'predictions.jsonl'
    with detail.open('x') as stream:
        for group, role, kind in groups:
            labels = manifest_path.parent/manifest['datasets'][role]['path']
            rows = load_jsonl(labels)
            if group == 'coco_hidden':
                rows = [r for r in rows if any(p['confidence'] < .5 for p in r['keypoints'])]
            for stress, gain in (('clean', 1.), ('dim', .6), ('bright', 1.3)):
                bucket = {name: [] for name in models}
                for index, row in enumerate(rows):
                    path = (labels.parent/row['image']).resolve()
                    digest = sha256(path)
                    if digest != row['frozen_image_sha256']:
                        raise ValueError('image changed')
                    image_hashes[str(path.relative_to(ROOT))] = digest
                    source = read_ppm(path)
                    rgb, covered, rect = mask_region(source, row['keypoints'], kind)
                    if kind != 'none' and not covered.any():
                        raise ValueError('synthetic occluder covers no joint centers')
                    # Match earlier stress tests: keep gain in float RGB96, no intermediate rounding.
                    rgb96 = rgb.astype(np.float32).reshape(96, 2, 96, 2, 3).mean(axis=(1, 3))
                    rgb96 = np.clip(rgb96*gain, 0, 255)
                    hidden = np.array([p['confidence'] < .5 for p in row['keypoints']]) | covered
                    reasons = Counter('synthetic_mask' if covered[i] else
                                      p.get('visibility', row.get('sample_kind', 'manual_hidden'))
                                      for i, p in enumerate(row['keypoints']) if hidden[i])
                    if stress == 'clean' and kind != 'none' and index < 4:
                        preview.append((group, index, rgb, row['keypoints'], covered, rect))
                    for name, (interpreter, inp, out) in interpreters.items():
                        scale, zero = inp['quantization']
                        # clean uses the native firmware preprocessing; float gain stress uses identical math.
                        quant = bridge.preprocess(rgb, scale, zero) if gain == 1 else np.clip(
                            np.rint((rgb96/np.float32(127.5)-1)/np.float32(scale)+zero), -128, 127).astype(np.int8)[None]
                        interpreter.set_tensor(inp['index'], quant)
                        interpreter.invoke()
                        scale, zero = out['quantization']
                        points = np.ascontiguousarray(np.clip((interpreter.get_tensor(out['index']).astype(np.float32)-zero)*scale,
                                                            0, 1).reshape(8, 3))
                        stats = np.empty(4, dtype=np.float32)
                        lib.vf_negative_probe(points.ctypes.data_as(fp), stats.ctypes.data_as(fp))
                        record = {'model': name, 'group': group, 'stress': stress,
                            'original_image': row['original_image'], 'image_sha256': digest,
                            'synthetic_mask_rect': rect, 'hidden_mask': hidden.tolist(),
                            'hidden_reasons': dict(reasons), 'hidden_count': int(hidden.sum()),
                            'hidden_rejected_030': int(np.sum(points[hidden, 2] < .3)),
                            'hidden_rejected_020': int(np.sum(points[hidden, 2] < .2)),
                            'strict_all_eight': bool(stats[0] == 8),
                            'counting_ready': bool(stats[0] >= 6 and stats[1] == 8),
                            'body_status': STATUSES[int(stats[2])], 'points': points.tolist()}
                        bucket[name].append(record)
                        stream.write(json.dumps(record, allow_nan=False)+'\n')
                        counts[name] += 1
                results[group+'-'+stress] = {name: summarize(records) for name, records in bucket.items()}
    sheet = Image.new('RGB', (192*4, 216*3))
    for i, (group, index, rgb, points, covered, rect) in enumerate(preview):
        tile = Image.fromarray(rgb)
        draw = ImageDraw.Draw(tile)
        for j, p in enumerate(points):
            x, y = p['x']*192, p['y']*192
            draw.ellipse((x-2, y-2, x+2, y+2), fill='red' if covered[j] else 'lime')
        x, y = i%4*192, i//4*216
        sheet.paste(tile, (x, y))
        ImageDraw.Draw(sheet).text((x+2, y+193), f'{group} {index+1}', fill='white')
    sheet.save(output/'synthetic-mask-review.png')
    verify_manifest(manifest_path)
    if (sha256(manifest_path) != input_hash or
            any(sha256(ROOT/p) != h for p, h in {**sources, **image_hashes}.items()) or
            any(sha256(models[n]) != h for n, h in EXPECTED_MODELS.items())):
        raise ValueError('input changed during evaluation')
    report = {'status': 'STATIC_NEGATIVE_DIAGNOSTICS_COMPLETE', 'models': EXPECTED_MODELS,
        'tensorflow': tf.__version__, 'manifest_sha256': input_hash, 'keypoint_order': list(KEYPOINTS),
        'source_hashes': sources, 'image_hashes': image_hashes, 'results': results,
        'inference_counts': dict(counts), 'details_sha256': sha256(detail),
        'limitations': ['Static frame gate acceptance is not an actual false rep count.',
            'Native BODY_CHECK is reset for each image; STABILIZING means geometry eligible, not READY.',
            'Synthetic masks and pixel gain are diagnostics, not real-world occlusion/light validation.',
            'COCO hidden includes occluded/unlabeled/outside-crop; see reason counts, not all are occlusion.',
            'SC empty/cropped development samples overlap historical training; no new blind test.',
            'No thresholds were tuned and no firmware/model deployment was changed.']}
    (output/'comparison.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    print(json.dumps({'status': report['status'], 'inferences': dict(counts), 'groups': len(results)}))


if __name__ == '__main__':
    main()
