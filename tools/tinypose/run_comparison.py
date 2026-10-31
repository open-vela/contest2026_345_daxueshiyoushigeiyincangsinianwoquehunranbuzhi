#!/usr/bin/env python3
"""Dry-run a frozen comparison; actual training requires the explicit --train flag."""
import argparse
from collections import Counter
import json
import pathlib

import numpy as np
import tensorflow as tf

from comparison_support import ROOT, BatchSampler, verify_manifest
from dataset_import import sha256
from evaluate_comparison import evaluate
from train_keypoints import build_model, load_records, metadata, pose_loss


def export_int8(model, calibration, directory):
    export = build_model(fixed_batch=True)
    export.set_weights(model.get_weights())

    def representative():
        for image in calibration:
            yield [image[None]]

    converter = tf.lite.TFLiteConverter.from_keras_model(export)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    payload = converter.convert()
    target = directory / 'tinypose_keypoints_int8.tflite'
    target.write_bytes(payload)
    interpreter = tf.lite.Interpreter(model_content=payload, num_threads=1,
        experimental_op_resolver_type=tf.lite.experimental.OpResolverType.BUILTIN_REF)
    interpreter.allocate_tensors()
    inp, out = interpreter.get_input_details()[0], interpreter.get_output_details()[0]
    ops = [row['op_name'] for row in interpreter._get_ops_details() if row['op_name'] != 'DELEGATE']
    if (ops != ['CONV_2D'] * 5 + ['RESHAPE', 'FULLY_CONNECTED'] or
            inp['dtype'] != np.int8 or out['dtype'] != np.int8 or
            list(inp['shape']) != [1, 96, 96, 3] or list(out['shape']) != [1, 24]):
        raise ValueError('export changed the deployed INT8 contract')
    return {'path': str(target), 'sha256': sha256(target), 'bytes': len(payload),
            'input': metadata(inp), 'output': metadata(out), 'operators': ops}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--arm', choices=('all', 'sc_only', 'sc_coco', 'sc_coco_photo'), default='all')
    parser.add_argument('--train', action='store_true',
                        help='explicitly execute full training/export; without this flag only forward checks run')
    args = parser.parse_args()
    args.manifest = args.manifest.resolve()
    args.output = args.output.resolve()
    if not args.output.is_relative_to(ROOT / '.secrets'):
        parser.error('private experiment output must be under repository .secrets')
    if args.output.exists():
        parser.error('output must be a fresh directory')
    manifest = verify_manifest(args.manifest)
    budget = manifest['budget']
    tf.keras.utils.set_random_seed(budget['seed'])
    devices = tf.config.list_physical_devices('GPU')
    if args.train and not devices:
        raise RuntimeError('formal training requires a visible GPU; CPU fallback is not accepted')
    for device in devices:
        tf.config.experimental.set_memory_growth(device, True)
    if args.train:
        tf.config.experimental.enable_op_determinism()
    pools = {}
    for role, dataset in manifest['datasets'].items():
        x, y, accepted = load_records([args.manifest.parent / dataset['path']])
        if len(accepted) != dataset['count']:
            raise ValueError(f'frozen records silently filtered by loader: {role}')
        pools[role] = (x, y)
    checkpoint = ROOT / manifest['initial_checkpoint']
    original_digest = sha256(checkpoint)
    report = {'mode': 'TRAIN' if args.train else 'FORWARD_ONLY_NO_TRAINING',
              'manifest_sha256': sha256(args.manifest), 'tensorflow': tf.__version__,
              'gpu_available': bool(devices), 'initial_checkpoint_sha256': original_digest,
              'dataset_counts': {k: len(v[0]) for k, v in pools.items()}, 'arms': []}
    args.output.mkdir(parents=True)
    for arm in manifest['arms']:
        if args.arm != 'all' and args.arm != arm['id']:
            continue
        if sum(arm['domain_counts'].values()) != budget['batch_size']:
            raise ValueError('batch budget differs across arms')
        tf.keras.backend.clear_session()
        tf.keras.utils.set_random_seed(budget['seed'])
        model = tf.keras.models.load_model(checkpoint, compile=False)
        sampler = BatchSampler(pools, arm['domain_counts'], budget['seed'], arm['photometric'])
        x, y, domains = sampler.next()
        before = [v.numpy().copy() for v in model.trainable_variables]
        prediction = model(x, training=False)
        loss = float(pose_loss(tf.constant(y), prediction).numpy())
        if prediction.shape != (budget['batch_size'], 24) or not np.isfinite(loss):
            raise ValueError('invalid forward batch or loss')
        if any(not np.array_equal(a, b.numpy()) for a, b in zip(before, model.trainable_variables)):
            raise ValueError('forward-only check modified model weights')
        entry = {'id': arm['id'], 'forward_loss': loss, 'domains': dict(Counter(domains)),
                 'input_shape': list(x.shape), 'output_shape': list(prediction.shape),
                 'device': prediction.device, 'forward_weights_unchanged': True}
        print(f'PASS forward {arm["id"]}: domains={entry["domains"]} loss={loss:.6f}', flush=True)
        if args.train:
            # Restart sampler so the preflight batch does not change the training schedule.
            sampler = BatchSampler(pools, arm['domain_counts'], budget['seed'], arm['photometric'])
            optimizer = tf.keras.optimizers.Adam(budget['learning_rate'])
            history = []
            for epoch in range(budget['epochs']):
                losses = []
                for _ in range(budget['steps_per_epoch']):
                    bx, by, _ = sampler.next()
                    with tf.device('/GPU:0'):
                        with tf.GradientTape() as tape:
                            output = model(tf.constant(bx), training=True)
                            value = pose_loss(tf.constant(by), output)
                        gradients = tape.gradient(value, model.trainable_variables)
                        if any(g is None or 'GPU:0' not in g.device for g in gradients):
                            raise RuntimeError('gradients missing or not on GPU')
                        optimizer.apply_gradients(zip(gradients, model.trainable_variables))
                    numeric = float(value.numpy())
                    if not np.isfinite(numeric):
                        raise ValueError('nonfinite training loss')
                    losses.append(numeric)
                vx, vy = pools['coco_validation']
                validation_loss = float(pose_loss(tf.constant(vy), model(vx, training=False)).numpy())
                history.append({'epoch': epoch+1, 'loss': float(np.mean(losses)),
                                'coco_validation_loss': validation_loss})
                print(arm['id'], history[-1], flush=True)
            directory = args.output / arm['id']
            directory.mkdir()
            model.save(directory / 'tinypose_float.keras')
            entry['export'] = export_int8(model, pools['calibration'][0], directory)
            entry['optimizer_updates'] = int(optimizer.iterations.numpy())
            entry['history'] = history
            entry['validation_refit'] = False
            model_path = pathlib.Path(entry['export']['path'])
            for role in ('coco_validation', 'sc2336_regression', 'sc2336_empty_development',
                         'sc2336_cropped_development'):
                labels = args.manifest.parent / manifest['datasets'][role]['path']
                for stress in (('clean', 'dim', 'bright') if role == 'coco_validation' else ('clean',)):
                    result = evaluate(model_path, [labels], stress)
                    (directory / f'{role}-{stress}.json').write_text(json.dumps(result, indent=2) + '\n')
            (directory / 'training-report.json').write_text(json.dumps(entry, indent=2) + '\n')
        report['arms'].append(entry)
    verify_manifest(args.manifest)
    if sha256(checkpoint) != original_digest:
        raise RuntimeError('source checkpoint changed')
    report['source_checkpoint_unchanged'] = True
    report['status'] = 'PASS'
    (args.output / ('training-summary.json' if args.train else 'preflight.json')).write_text(
        json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'mode': report['mode'],
                      'arms': len(report['arms']), 'output': str(args.output)}))


if __name__ == '__main__':
    main()
