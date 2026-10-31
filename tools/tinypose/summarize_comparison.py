#!/usr/bin/env python3
"""Verify trained artifacts and compare INT8 metrics with predeclared gates."""
import argparse
import json
import math
import pathlib

from comparison_support import ROOT, verify_manifest
from dataset_import import sha256

CASES = ('sc2336_regression-clean', 'coco_validation-clean', 'coco_validation-dim',
         'coco_validation-bright', 'sc2336_empty_development-clean',
         'sc2336_cropped_development-clean')


def finite_json(value):
    if isinstance(value, dict):
        return all(finite_json(v) for v in value.values())
    if isinstance(value, list):
        return all(finite_json(v) for v in value)
    return not isinstance(value, float) or math.isfinite(value)


def evaluate_gates(baseline, candidate, gates):
    original_sc, sc = baseline['sc2336_regression-clean'], candidate['sc2336_regression-clean']
    epsilon = 1e-12
    checks = {
        'sc2336_pck10_retention': sc['pck_10px'] + epsilon >=
            original_sc['pck_10px'] - gates['sc2336_pck10_max_drop'],
        'sc2336_mean_error_retention': sc['mean_error_px'] <=
            original_sc['mean_error_px'] + gates['sc2336_mean_error_max_increase_px'] + epsilon,
        'sc2336_p95_error_retention': sc['p95_error_px'] <=
            original_sc['p95_error_px'] + gates['sc2336_p95_error_max_increase_px'] + epsilon,
        'coco_pck10_gain': candidate['coco_validation-clean']['pck_10px'] + epsilon >=
            baseline['coco_validation-clean']['pck_10px'] + gates['coco_pck10_target_gain'],
    }
    for role in ('sc2336_empty_development-clean', 'sc2336_cropped_development-clean'):
        checks[role + '_hidden_rejection_retention'] = candidate[role]['hidden_rejection_rate'] + epsilon >= (
            baseline[role]['hidden_rejection_rate'] - gates['development_hidden_rejection_max_drop'])
    return {'checks': checks, 'all_pass': all(checks.values()),
            'failed': [name for name, passed in checks.items() if not passed]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=pathlib.Path, required=True)
    parser.add_argument('--trained', type=pathlib.Path, required=True)
    parser.add_argument('--baseline', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('output already exists')
    manifest = verify_manifest(args.manifest)
    summary = json.loads((args.trained / 'training-summary.json').read_text())
    if (summary['status'] != 'PASS' or summary['mode'] != 'TRAIN' or
            not summary['gpu_available'] or not summary['source_checkpoint_unchanged'] or
            summary['manifest_sha256'] != sha256(args.manifest)):
        raise ValueError('training summary is incomplete or not the frozen GPU run')
    initial_hash = sha256(ROOT / manifest['initial_checkpoint'])
    if initial_hash != summary['initial_checkpoint_sha256']:
        raise ValueError('source checkpoint changed')
    manifest_arms = {arm['id'] for arm in manifest['arms']}
    if {arm['id'] for arm in summary['arms']} != manifest_arms:
        raise ValueError('missing or unexpected training arm')
    inputs = {str(args.manifest): sha256(args.manifest),
              str(args.trained / 'training-summary.json'): sha256(args.trained / 'training-summary.json')}

    def read_cases(directory, expected_model_hash):
        cases = {}
        for name in CASES:
            path = directory / (name + '.json')
            result = json.loads(path.read_text())
            role = name.rsplit('-', 1)[0]
            expected = manifest['datasets'][role]
            if (not finite_json(result) or result['model_sha256'] != expected_model_hash or
                    result['images'] != expected['count'] or
                    set(result['labels_sha256'].values()) != {expected['sha256']}):
                raise ValueError(f'invalid or mismatched evaluation report: {path}')
            inputs[str(path)] = sha256(path)
            cases[name] = result
        return cases

    baseline_hash = sha256(ROOT / manifest['baseline_model'])
    baseline = read_cases(args.baseline, baseline_hash)
    result = {'status': 'PASS', 'manifest_sha256': sha256(args.manifest),
              'baseline_model_sha256': baseline_hash, 'baseline': baseline, 'arms': {},
              'gates': manifest['acceptance_proposal'], 'source_checkpoint_unchanged': True,
              'automatic_deployment': False}
    updates = manifest['budget']['epochs'] * manifest['budget']['steps_per_epoch']
    for arm in summary['arms']:
        directory = args.trained / arm['id']
        path = directory / 'training-report.json'
        if json.loads(path.read_text()) != arm:
            raise ValueError('per-arm report disagrees with run summary')
        inputs[str(path)] = sha256(path)
        model = directory / 'tinypose_keypoints_int8.tflite'
        model_hash = sha256(model)
        if (arm['optimizer_updates'] != updates or len(arm['history']) != manifest['budget']['epochs'] or
                arm['validation_refit'] or 'GPU:0' not in arm['device'] or
                model_hash != arm['export']['sha256'] or model.stat().st_size != arm['export']['bytes'] or
                not finite_json(arm)):
            raise ValueError('training/export contract mismatch')
        inputs[str(model)] = model_hash
        keras = directory / 'tinypose_float.keras'
        inputs[str(keras)] = sha256(keras)
        if inputs[str(keras)] == initial_hash:
            raise ValueError('candidate checkpoint is identical to initial weights')
        cases = read_cases(directory, model_hash)
        checked = evaluate_gates(baseline, cases, manifest['acceptance_proposal'])
        result['arms'][arm['id']] = {
            'metrics': cases, 'gates': checked, 'model_sha256': model_hash,
            'model_bytes': model.stat().st_size, 'optimizer_updates': updates,
            'sc2336_pck10_change_pp': 100 * (cases['sc2336_regression-clean']['pck_10px'] -
                                            baseline['sc2336_regression-clean']['pck_10px']),
            'coco_pck10_change_pp': 100 * (cases['coco_validation-clean']['pck_10px'] -
                                          baseline['coco_validation-clean']['pck_10px']),
            'sc2336_all_eight_change_pp': 100 * (cases['sc2336_regression-clean']['all_eight_accepted_rate'] -
                                                baseline['sc2336_regression-clean']['all_eight_accepted_rate']),
        }
    result['passing_arms'] = [name for name, arm in result['arms'].items() if arm['gates']['all_pass']]
    result['decision'] = ('NO_CANDIDATE_PASSES_KEEP_V4C' if not result['passing_arms']
                          else 'CANDIDATES_REQUIRE_HARDWARE_ACCEPTANCE')
    if {'sc_coco', 'sc_coco_photo'} <= result['arms'].keys():
        plain = result['arms']['sc_coco']['metrics']
        photo = result['arms']['sc_coco_photo']['metrics']
        result['photometric_vs_mixed_pck10_change_pp'] = {
            role: 100 * (photo[role]['pck_10px'] - plain[role]['pck_10px'])
            for role in ('coco_validation-clean', 'coco_validation-dim',
                         'coco_validation-bright', 'sc2336_regression-clean')}
    result['input_hashes'] = inputs
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'decision': result['decision'], 'passing_arms': result['passing_arms'],
                      'results': {name: {'sc_delta_pp': arm['sc2336_pck10_change_pp'],
                                        'coco_delta_pp': arm['coco_pck10_change_pp'],
                                        'failed_gates': arm['gates']['failed']}
                                  for name, arm in result['arms'].items()}}, indent=2))


if __name__ == '__main__':
    main()
