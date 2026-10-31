#!/usr/bin/env python3
"""Verify actual CUDA execution and one disposable TinyPose training step."""
import argparse
import hashlib
import json
import pathlib

import numpy as np
import tensorflow as tf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('output already exists')
    digest = hashlib.sha256(args.checkpoint.read_bytes()).hexdigest()
    devices = tf.config.list_physical_devices('GPU')
    if not devices:
        raise RuntimeError('No CUDA GPU detected; CPU fallback is not accepted')
    for device in devices:
        tf.config.experimental.set_memory_growth(device, True)
    tf.config.set_soft_device_placement(False)
    with tf.device('/GPU:0'):
        product = tf.matmul(tf.ones((512, 512)), tf.ones((512, 512)))
    np.testing.assert_allclose(product.numpy(), 512.0)
    if 'GPU:0' not in product.device:
        raise RuntimeError(f'Matmul did not execute on GPU: {product.device}')
    # Load the real checkpoint; update only this in-memory copy, never save it.
    from train_keypoints import pose_loss
    model = tf.keras.models.load_model(args.checkpoint, compile=False)
    initial_weights = [value.numpy().copy() for value in model.trainable_variables]
    rng = np.random.default_rng(34596)
    images = tf.constant(rng.uniform(-1, 1, (8, 96, 96, 3)), tf.float32)
    targets = np.zeros((8, 8, 3), np.float32)
    targets[..., :2] = 0.5
    targets[..., 2] = 1.0
    optimizer = tf.keras.optimizers.Adam(2e-4)
    with tf.device('/GPU:0'):
        with tf.GradientTape() as tape:
            prediction = model(images, training=True)
            loss = pose_loss(tf.constant(targets), prediction)
        gradients = tape.gradient(loss, model.trainable_variables)
        optimizer.apply_gradients(zip(gradients, model.trainable_variables))
    if not np.isfinite(float(loss.numpy())):
        raise RuntimeError('Training step produced nonfinite loss')
    if 'GPU:0' not in prediction.device:
        raise RuntimeError(f'TinyPose forward did not execute on GPU: {prediction.device}')
    if any(value is None or 'GPU:0' not in value.device for value in gradients):
        raise RuntimeError('Some gradients are missing or did not execute on GPU')
    if not any(not np.array_equal(before, after.numpy())
               for before, after in zip(initial_weights, model.trainable_variables)):
        raise RuntimeError('Training step did not update in-memory weights')
    if hashlib.sha256(args.checkpoint.read_bytes()).hexdigest() != digest:
        raise RuntimeError('Source checkpoint changed')
    report = {
        'status': 'PASS', 'tensorflow': tf.__version__, 'keras': tf.keras.__version__,
        'gpu': tf.config.experimental.get_device_details(devices[0]),
        'tensorflow_build': tf.sysconfig.get_build_info(),
        'matmul_device': product.device, 'tinypose_forward_device': prediction.device,
        'training_batch': 8, 'synthetic_smoke_loss': float(loss.numpy()),
        'checkpoint_sha256': digest, 'checkpoint_unchanged': True,
        'gpu_gradients': True, 'in_memory_weights_updated': True,
        'note': 'Synthetic training smoke only; not accuracy or a newly trained model.',
    }
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
