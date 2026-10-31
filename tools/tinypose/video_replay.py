#!/usr/bin/env python3
"""Paired offline video inference with the actual TinyPose adaptive C counter."""
from __future__ import annotations

import argparse
from collections import Counter
import ctypes
import json
import math
import pathlib
import subprocess
import sys
import time

import numpy as np

from comparison_support import ROOT
from dataset_import import frame_image, sha256
from import_workout_videos import sample_clip, validate_clips
from keypoint_dataset import KEYPOINTS

V4C = ROOT / '.secrets/tinypose-keypoints-portrait-model-v4c-20260920/tinypose_keypoints_int8.tflite'
PHOTO = ROOT / '.secrets/experiments/tinypose-followup-v2-20261025/trained/sc_only_photo/tinypose_keypoints_int8.tflite'
EXPECTED_MODELS = {'v4c': 'a4713ca86cecc9d7ff31c1df2c8fa9cb495a8c32e0f98b97e3e0d6b2a15cf9c5',
                   'sc_only_photo': 'bb5bf46cade71f030b0c28a83b89598da9fc7fe6131e1a428566ab019aa4a613'}
SOURCES = (
    'tools/tinypose/video_replay.py', 'tools/tinypose/video_replay_bridge.c',
    'tools/tinypose/import_workout_videos.py', 'tools/tinypose/dataset_import.py',
    'tools/tinypose/comparison_support.py', 'tools/tinypose/keypoint_dataset.py',
    'app/velafit_ai/algo/squat_fsm.c', 'app/velafit_ai/algo/squat_fsm.h',
    'app/velafit_ai/algo/geometry.c', 'app/velafit_ai/algo/geometry.h',
    'app/velafit_ai/include/velafit_types.h',
    'app/velafit_ai/models/velafit_tiny_pose_preprocess.c',
    'app/velafit_ai/models/velafit_tiny_pose_preprocess.h',
    'app/velafit_ai/models/velafit_tiny_pose_tflm.cc',
    'app/sc2336_probe/sc2336_probe_main.c',
)


def build_bridge(directory):
    target = directory / 'video_replay_bridge.so'
    command = ['cc', '-std=c99', '-O2', '-shared', '-fPIC',
               '-I' + str(ROOT / 'app/velafit_ai/include'),
               '-I' + str(ROOT / 'app/velafit_ai/algo'),
               '-I' + str(ROOT / 'app/velafit_ai/models'),
               *[str(ROOT / p) for p in (SOURCES[1], SOURCES[6], SOURCES[8], SOURCES[11])],
               '-lm', '-o', str(target)]
    subprocess.run(command, check=True, capture_output=True, text=True)
    return target


class Bridge:
    def __init__(self, path):
        lib = self.lib = ctypes.CDLL(str(path.resolve()))
        fp = ctypes.POINTER(ctypes.c_float)
        lib.vf_replay_create.restype = ctypes.c_void_p
        lib.vf_replay_free.argtypes = [ctypes.c_void_p]
        lib.vf_replay_step.argtypes = [ctypes.c_void_p, fp, ctypes.c_uint, ctypes.c_int, fp]
        lib.velafit_tiny_pose_preprocess_rgb192.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                                          ctypes.c_float, ctypes.c_int]

    def preprocess(self, image, scale, zero):
        rgb = np.ascontiguousarray(image, dtype=np.uint8)
        if rgb.shape != (192, 192, 3):
            raise ValueError('expected RGB192')
        quant = np.empty((1, 96, 96, 3), dtype=np.int8)
        self.lib.velafit_tiny_pose_preprocess_rgb192(rgb.ctypes.data, quant.ctypes.data, scale, zero)
        return quant

    def create(self):
        handle = self.lib.vf_replay_create()
        if not handle:
            raise MemoryError('cannot allocate counter')
        return handle

    def step(self, handle, points, timestamp_ms, capture):
        points = np.ascontiguousarray(points, dtype=np.float32)
        if points.shape != (8, 3) or not np.isfinite(points).all():
            raise ValueError('invalid predicted points')
        if not 0 <= timestamp_ms <= 0xffffffff:
            raise ValueError('video timestamp exceeds counter clock range')
        result = np.empty(8, dtype=np.float32)
        fp = ctypes.POINTER(ctypes.c_float)
        self.lib.vf_replay_step(handle, points.ctypes.data_as(fp), timestamp_ms, capture,
                                result.ctypes.data_as(fp))
        return result


def match_events(predicted, events, window):
    """Chronological one-to-one matching: maximize matches, then minimize |delay|."""
    n, m = len(predicted), len(events)
    score = [[(0, 0.) for _ in range(m + 1)] for _ in range(n + 1)]
    action = {}
    for i in range(n + 1):
        for j in range(m + 1):
            if not i and not j:
                continue
            options = []
            if i:
                options.append((score[i-1][j], 'skip_prediction'))
            if j:
                options.append((score[i][j-1], 'skip_truth'))
            if i and j:
                delay = predicted[i-1] - events[j-1]['complete_sec']
                if abs(delay) <= window:
                    previous = score[i-1][j-1]
                    options.append(((previous[0]+1, previous[1]-abs(delay)), 'match'))
            score[i][j], action[i, j] = max(options, key=lambda item: item[0])
    pairs = []
    i, j = n, m
    while i or j:
        choice = action[i, j]
        if choice == 'match':
            pairs.append({'prediction_index': i-1, 'truth_index': j-1,
                          'delay_sec': predicted[i-1]-events[j-1]['complete_sec'],
                          'bottom_to_count_sec': predicted[i-1]-events[j-1]['bottom_sec']
                          if 'bottom_sec' in events[j-1] else None})
            i, j = i-1, j-1
        elif choice == 'skip_prediction':
            i -= 1
        else:
            j -= 1
    pairs.reverse()
    delays = [p['delay_sec'] for p in pairs]
    return {'matched': len(pairs), 'missed': m-len(pairs), 'extra': n-len(pairs),
            'signed_delay_median_sec': float(np.median(delays)) if delays else None,
            'signed_delay_p95_sec': float(np.percentile(delays, 95)) if delays else None,
            'mean_absolute_delay_sec': float(np.mean(np.abs(delays))) if delays else None,
            'pairs': pairs}


def continuity(timestamps, valid):
    if not timestamps:
        raise ValueError('no evaluated frames')
    longest = 0
    span = 0.
    run = 0
    start = 0.
    breaks = 0
    gaps = np.diff(timestamps)
    for i, (timestamp, accepted) in enumerate(zip(timestamps, valid)):
        discontinuous = i > 0 and timestamp - timestamps[i-1] > 1.
        if discontinuous:
            run = 0
        if accepted:
            if not run:
                start = timestamp
            run += 1
            longest = max(longest, run)
            span = max(span, timestamp-start)
        else:
            breaks += int(i > 0 and valid[i-1])
            run = 0
    return {'valid_frames': int(sum(valid)), 'frames': len(valid),
            'valid_fraction': float(np.mean(valid)), 'longest_valid_run_frames': longest,
            'longest_valid_run_span_sec': span, 'valid_to_invalid_transitions': breaks,
            'gaps_over_1sec': int(np.sum(gaps > 1)),
            'median_frame_interval_sec': float(np.median(gaps)) if len(gaps) else None}


def validate_annotations(clip):
    if clip['exercise'] not in ('squat', 'negative'):
        raise ValueError('this evaluator supports squat and negative clips only')
    warmup = clip.get('warmup_sec', 2.)
    start, end = clip.get('start_sec', 0.), clip.get('end_sec')
    if (not math.isfinite(warmup) or warmup < 0 or
            (end is not None and start+warmup >= end)):
        raise ValueError('invalid standing warmup duration')
    if clip.get('rotation', 0) not in (0, 90, 180, 270):
        raise ValueError('rotation must be clockwise 0/90/180/270')
    expected = clip.get('expected_reps')
    if expected is not None and (type(expected) is not int or expected < 0):
        raise ValueError('expected_reps must be null or a nonnegative integer')
    reviewed = clip.get('events_reviewed', False)
    if type(reviewed) is not bool:
        raise ValueError('events_reviewed must be boolean')
    events = clip.get('events', [])
    last = -1.
    for event in events:
        complete = event['complete_sec']
        if (not math.isfinite(complete) or complete < start+warmup or complete <= last or
                (end is not None and complete >= end)):
            raise ValueError('events must be ordered within the evaluated source-video interval')
        previous = start+warmup
        for key in ('start_sec', 'bottom_sec', 'complete_sec'):
            if key in event:
                value = event[key]
                if not math.isfinite(value) or not previous <= value <= complete:
                    raise ValueError('event must satisfy start <= bottom <= complete')
                previous = value
        last = complete
    if reviewed and (expected is None or len(events) != expected):
        raise ValueError('reviewed event count must equal explicit expected_reps')


def evaluate_videos(clips_path, output, fps=10., window=1., fixture=False):
    import tensorflow as tf
    clips_path, output = clips_path.resolve(), output.resolve()
    if not output.is_relative_to(ROOT / '.secrets') or output.exists():
        raise ValueError('output must be a new directory under repository .secrets')
    document = json.loads(clips_path.read_text())
    clips = document['clips']
    fixture = fixture or document.get('test_fixture_only', False)
    validate_clips(clips, clips_path.parent)
    for clip in clips:
        validate_annotations(clip)
    source_hashes = {p: sha256(ROOT / p) for p in SOURCES}
    input_hash = sha256(clips_path)
    models = {'v4c': V4C, 'sc_only_photo': PHOTO}
    for name, path in models.items():
        if sha256(path) != EXPECTED_MODELS[name]:
            raise ValueError('comparison model hash changed: ' + name)
    output.mkdir(parents=True)
    (output / 'clips-source.json').write_bytes(clips_path.read_bytes())
    bridge_path = build_bridge(output)
    bridge = Bridge(bridge_path)
    interpreters = {}
    for name, path in models.items():
        interpreter = tf.lite.Interpreter(model_path=str(path), num_threads=1,
            experimental_op_resolver_type=tf.lite.experimental.OpResolverType.BUILTIN_REF)
        interpreter.allocate_tensors()
        inp, out = interpreter.get_input_details()[0], interpreter.get_output_details()[0]
        if (list(inp['shape']) != [1, 96, 96, 3] or list(out['shape']) != [1, 24] or
                inp['dtype'] != np.int8 or out['dtype'] != np.int8):
            raise ValueError('unexpected model contract')
        interpreters[name] = (interpreter, inp, out)
    report = {'status': 'RUNNING', 'test_fixture_only': fixture,
              'models': EXPECTED_MODELS, 'manifest_sha256': input_hash,
              'source_hashes': source_hashes, 'bridge_sha256': sha256(bridge_path),
              'python': sys.version, 'tensorflow': tf.__version__,
              'fps_limit': fps, 'match_window_sec': window,
              'keypoint_order': list(KEYPOINTS),
              'count_definition': 'adaptive FSM rep completion during ascent/recovery; not first bottom',
              'pose_valid_rule': '>=6 points at .30; FSM requires all eight >=.20',
              'preprocessing': 'explicit rotation, RGB192 aspect letterbox, native 2x2 area and quantization',
              'limitations': ['Offline source-video event delay excludes camera/queue/LCD and board compute time.',
                             'Phone RGB letterbox does not reproduce SC2336 RAW10 ISP/capture geometry.',
                             'No body-check/session UI simulation; standing warmup then native adaptive counter.',
                             'Validity means confidence acceptance, not ground-truth coordinate correctness.',
                             'BUILTIN_REF host kernels are not a measurement of P4/TFLM inference speed.'],
              'clips': []}
    for index, clip in enumerate(clips):
        handles = {name: bridge.create() for name in models}
        data = {name: {'times': [], 'strict': [], 'counting': [], 'events': [], 'inference_ms': []}
                for name in models}
        time_sources = Counter()
        frame_count = 0
        last_ms = -1
        frame_path = output / f'clip-{index:03d}-frames.jsonl'
        last_timestamp = None
        try:
            with frame_path.open('x') as stream:
                for frame_index, seconds, time_source, image in sample_clip(clip, fps, sys.maxsize):
                    if clip.get('rotation', 0):
                        image = image.rotate(-clip['rotation'], expand=True)
                    image, transform = frame_image(image)
                    rgb = np.asarray(image, dtype=np.uint8)
                    capture = seconds >= clip.get('start_sec', 0)+clip.get('warmup_sec', 2.)
                    ms = round(seconds * 1000)
                    if ms <= last_ms:
                        raise ValueError('millisecond timestamps must strictly increase')
                    last_ms, last_timestamp = ms, seconds
                    row = {'frame_index': frame_index, 'source_sec': seconds,
                           'timestamp_source': time_source, 'capture': capture,
                           'transform': transform, 'models': {}}
                    for name, (interpreter, inp, out) in interpreters.items():
                        quant = bridge.preprocess(rgb, *inp['quantization'])
                        interpreter.set_tensor(inp['index'], quant)
                        before = time.perf_counter()
                        interpreter.invoke()
                        elapsed = (time.perf_counter()-before)*1000
                        scale, zero = out['quantization']
                        points = np.clip((interpreter.get_tensor(out['index']).astype(np.float32)-zero)*scale,
                                         0., 1.).reshape(8, 3)
                        stats = bridge.step(handles[name], points, ms, capture)
                        strict = bool(np.all(points[:, 2] >= .3))
                        row['models'][name] = {'points': points.tolist(), 'strict_all_eight': strict,
                            'counting_ready': bool(stats[3]), 'total_reps': int(stats[0]),
                            'valid_reps': int(stats[1]), 'state': int(stats[2]),
                            'knee_angle': float(stats[4]), 'standing_baseline': float(stats[5]),
                            'rep_event': bool(stats[6]), 'quality_flags': int(stats[7]),
                            'host_reference_inference_ms': elapsed}
                        if capture:
                            d = data[name]
                            d['times'].append(seconds)
                            d['strict'].append(strict)
                            d['counting'].append(bool(stats[3]))
                            d['inference_ms'].append(elapsed)
                            if stats[6]:
                                d['events'].append(seconds)
                    stream.write(json.dumps(row, allow_nan=False)+'\n')
                    time_sources[time_source] += 1
                    frame_count += 1
        finally:
            for handle in handles.values():
                bridge.lib.vf_replay_free(handle)
        if not frame_count or not all(d['times'] for d in data.values()):
            raise ValueError('clip has no frames after warmup')
        if any(e['complete_sec'] > last_timestamp for e in clip.get('events', [])):
            raise ValueError('annotation extends beyond decoded video coverage')
        item = {'path': clip['path'], 'subject_id': clip['subject_id'],
                'video_sha256': clip['_sha256'], 'exercise': clip['exercise'],
                'lighting': clip.get('lighting', 'unspecified'), 'split': clip['split'],
                'frames_including_warmup': frame_count, 'timestamp_sources': dict(time_sources),
                'expected_reps': clip.get('expected_reps'),
                'events_reviewed': clip.get('events_reviewed', False), 'models': {}}
        for name, d in data.items():
            result = {'predicted_reps': len(d['events']), 'event_source_seconds': d['events'],
                      'net_count_error': len(d['events'])-clip['expected_reps']
                          if clip.get('expected_reps') is not None else None,
                      'strict_all_eight_continuity': continuity(d['times'], d['strict']),
                      'counting_ready_continuity': continuity(d['times'], d['counting']),
                      'host_reference_inference_median_ms': float(np.median(d['inference_ms'])),
                      'event_metrics': match_events(d['events'], clip.get('events', []), window)
                          if clip.get('events_reviewed') else None}
            item['models'][name] = result
        report['clips'].append(item)
        print(json.dumps({'clip': index, 'counts': {k: v['predicted_reps'] for k, v in item['models'].items()}}), flush=True)
    if input_hash != sha256(clips_path) or any(sha256(ROOT/p) != h for p, h in source_hashes.items()):
        raise ValueError('input manifest or source code changed during evaluation')
    if any(sha256(models[n]) != h for n, h in EXPECTED_MODELS.items()):
        raise ValueError('model changed during evaluation')
    if any(sha256(c['_path']) != c['_sha256'] for c in clips):
        raise ValueError('video changed during evaluation')
    report['aggregate'] = {}
    reviewed = [c for c in report['clips'] if c['events_reviewed']]
    for name in models:
        metrics = [c['models'][name]['event_metrics'] for c in reviewed]
        delays = [p['delay_sec'] for m in metrics for p in m['pairs']]
        frames = sum(c['models'][name]['counting_ready_continuity']['frames'] for c in report['clips'])
        report['aggregate'][name] = {
            'predicted_reps_all_clips': sum(c['models'][name]['predicted_reps'] for c in report['clips']),
            'reviewed_clips': len(reviewed),
            'reviewed_expected_reps': sum(c['expected_reps'] for c in reviewed) if reviewed else None,
            'missed_reviewed': sum(m['missed'] for m in metrics) if reviewed else None,
            'extra_reviewed': sum(m['extra'] for m in metrics) if reviewed else None,
            'signed_delay_median_sec': float(np.median(delays)) if delays else None,
            'signed_delay_p95_sec': float(np.percentile(delays, 95)) if delays else None,
            'strict_all_eight_valid_fraction': sum(c['models'][name]['strict_all_eight_continuity']['valid_frames']
                                                   for c in report['clips'])/frames,
            'counting_ready_valid_fraction': sum(c['models'][name]['counting_ready_continuity']['valid_frames']
                                                 for c in report['clips'])/frames,
        }
    report['frame_report_hashes'] = {p.name: sha256(p) for p in output.glob('*-frames.jsonl')}
    report['status'] = 'FIXTURE_PIPELINE_PASS' if fixture else 'OFFLINE_REPLAY_COMPLETE'
    (output / 'comparison.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clips', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--fps', type=float, default=10.)
    parser.add_argument('--match-window-sec', type=float, default=1.)
    parser.add_argument('--test-fixture-only', action='store_true')
    args = parser.parse_args()
    if not math.isfinite(args.fps) or not 0 < args.fps <= 30:
        parser.error('fps must be in (0,30]')
    if not math.isfinite(args.match_window_sec) or not 0 < args.match_window_sec <= 5:
        parser.error('match window must be in (0,5]')
    result = evaluate_videos(args.clips, args.output, args.fps, args.match_window_sec, args.test_fixture_only)
    print(json.dumps({'status': result['status'], 'clips': len(result['clips'])}))


if __name__ == '__main__':
    main()
