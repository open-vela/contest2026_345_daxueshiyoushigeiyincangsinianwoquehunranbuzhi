#!/usr/bin/env python3
"""Geometry, visibility, split isolation and real video-decoding regression tests."""
import copy
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
import zipfile

import cv2
import numpy as np
from PIL import Image

from dataset_import import COCO_KEYPOINTS, coco_points, frame_image
from curate_coco_subset import visible_span
from import_coco import select_records
from import_workout_videos import validate_clips
from keypoint_dataset import load_jsonl


class GeometryTests(unittest.TestCase):
    def test_body_size_filter_preserves_horizontal_poses(self):
        record = {'keypoints': [{'x': 0.1, 'y': 0.5, 'usable': True},
                                {'x': 0.9, 'y': 0.5, 'usable': True},
                                {'x': 0.0, 'y': 0.0, 'usable': False}]}
        self.assertAlmostEqual(visible_span(record), 0.8)
        record['keypoints'][1]['x'] = 0.2
        self.assertAlmostEqual(visible_span(record), 0.1)

    def test_letterbox_point_matches_image_marker(self):
        pixels = np.zeros((320, 640, 3), np.uint8)
        pixels[150:170, 310:330] = (255, 0, 0)
        image, transform = frame_image(Image.fromarray(pixels))
        points, _ = coco_points([320, 160, 2] * 17, transform)
        self.assertEqual(transform['pad_xy'], [0, 48])
        self.assertEqual((points[0]['x'], points[0]['y']), (0.5, 0.5))
        self.assertGreater(np.asarray(image)[96, 96, 0], 200)

    def test_crop_boundary_and_occlusion_are_not_visible(self):
        _, transform = frame_image(Image.new('RGB', (640, 320)), (100, 20, 500, 300))
        raw = [320, 160, 2] * 17
        raw[5*3:5*3+3] = [90, 160, 2]
        raw[6*3:6*3+3] = [320, 160, 1]
        raw[11*3:11*3+3] = [0, 0, 0]
        points, all_points = coco_points(raw, transform)
        self.assertEqual(points[0]['visibility'], 'outside_crop')
        self.assertEqual(points[1]['visibility'], 'occluded')
        self.assertEqual(points[2]['visibility'], 'unlabeled')
        self.assertTrue(all(not p['usable'] and p['confidence'] == 0 for p in points[:3]))
        self.assertEqual(all_points[6]['coco_visibility'], 1)
        self.assertEqual(len(all_points), 17)

    def test_odd_resize_uses_actual_scale(self):
        _, transform = frame_image(Image.new('RGB', (641, 321)))
        self.assertEqual(transform['resized_wh'], [192, 96])
        self.assertNotEqual(*transform['scale_xy'])
        points, _ = coco_points([320.5, 160.5, 2] * 17, transform)
        self.assertAlmostEqual(points[0]['x'], 0.5)
        self.assertAlmostEqual(points[0]['y'], 0.5)

    def test_crop_outside_image_is_clipped_consistently(self):
        _, transform = frame_image(Image.new('RGB', (100, 200)), (-20, -10, 120, 180))
        self.assertEqual(transform['crop_xyxy'], [0, 0, 100, 180])
        points, _ = coco_points([50, 90, 2] * 17, transform)
        # Odd horizontal padding differs by one pixel; coordinates must use
        # the actual raster transform, not assume a mathematically centered pad.
        self.assertAlmostEqual(points[0]['x'], (42 + 107 / 2) / 192)
        self.assertAlmostEqual(points[0]['y'], 0.5)


def coco_fixture():
    return {'images': [{'id': i, 'license': 4, 'width': 400, 'height': 400}
                       for i in range(12)],
            'categories': [{'id': 1, 'keypoints': list(COCO_KEYPOINTS)}],
            'annotations': [{'id': i, 'image_id': i, 'category_id': 1,
                             'iscrowd': 0, 'keypoints': [200, 200, 2] * 17,
                             'bbox': [100, 20, 200, 300]}
                            for i in range(12)]}


class SelectionTests(unittest.TestCase):
    def test_unlabeled_second_person_and_crowd_excluded(self):
        data = coco_fixture()
        second = copy.deepcopy(data['annotations'][0])
        second['keypoints'] = [0, 0, 0] * 17
        data['annotations'].append(second)
        data['annotations'][1]['iscrowd'] = 1
        selected, _ = select_records(data, 12, 42, {4}, 6)
        self.assertEqual(len(selected), 10)
        self.assertNotIn(0, [row[0]['id'] for row in selected])
        self.assertNotIn(1, [row[0]['id'] for row in selected])

    def test_license_visibility_and_seed(self):
        data = coco_fixture()
        data['images'][0]['license'] = 1
        data['annotations'][1]['keypoints'] = [0, 0, 0] * 17
        first, _ = select_records(data, 5, 34596, {4}, 6)
        second, _ = select_records(data, 5, 34596, {4}, 6)
        self.assertEqual(first, second)
        self.assertTrue(all(row[0]['id'] not in (0, 1) for row in first))

    def test_offline_coco_zip_import_preserves_source_and_split(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            archive_path = root / 'annotations.zip'
            with zipfile.ZipFile(archive_path, 'w') as archive:
                for split, offset in (('train2017', 0), ('val2017', 100)):
                    data = coco_fixture()
                    directory = root / 'cache' / split
                    directory.mkdir(parents=True)
                    data['licenses'] = [{'id': 4, 'name': 'Attribution License',
                                         'url': 'https://creativecommons.org/licenses/by/2.0/'}]
                    for image in data['images']:
                        image['id'] += offset
                        image['file_name'] = f'{image["id"]:012d}.jpg'
                        image['coco_url'] = f'https://example.invalid/{image["id"]}'
                        image['flickr_url'] = f'https://example.invalid/attribution/{image["id"]}'
                        Image.new('RGB', (400, 400), (image['id'], 100, 200)).save(
                            directory / image['file_name'])
                    for ann in data['annotations']:
                        ann['image_id'] += offset
                    archive.writestr(f'annotations/person_keypoints_{split}.json', json.dumps(data))
            output = root / 'out'
            subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name('import_coco.py')),
                            '--annotations-zip', str(archive_path), '--cache', str(root / 'cache'),
                            '--output', str(output), '--train-count', '2', '--validation-count', '2'],
                           check=True, capture_output=True, text=True)
            train = load_jsonl(output / 'train/labels.jsonl')
            validation = load_jsonl(output / 'validation/labels.jsonl')
            self.assertEqual((len(train), len(validation)), (2, 2))
            self.assertFalse({r['source_image_id'] for r in train} &
                             {r['source_image_id'] for r in validation})
            self.assertTrue(all(len(r['keypoints17']) == 17 and r['license']['id'] == 4
                                for r in train + validation))
            report = json.loads((output / 'import-report.json').read_text())
            self.assertEqual(report['cross_split_source_hash_overlap'], 0)


class VideoTests(unittest.TestCase):
    def clip(self, path, subject='subject-a', split='train'):
        return {'path': str(path), 'subject_id': subject, 'split': split,
                'exercise': 'squat', 'license': 'synthetic_test_fixture',
                'source_url': 'generated:test', 'single_person_confirmed': True}

    def test_subject_leakage_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            a, b = root / 'a.avi', root / 'b.avi'
            a.write_bytes(b'a')
            b.write_bytes(b'b')
            with self.assertRaisesRegex(ValueError, 'leaks'):
                validate_clips([self.clip(a), self.clip(b, split='validation')], root)

    def test_source_leakage_rejected_even_with_different_subject(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            a = root / 'a.avi'
            a.write_bytes(b'a')
            with self.assertRaisesRegex(ValueError, 'leaks'):
                validate_clips([self.clip(a), self.clip(a, 'other', 'validation')], root)

    def test_confirmed_single_person_required(self):
        clip = self.clip('missing.mp4')
        clip['single_person_confirmed'] = False
        with self.assertRaisesRegex(ValueError, 'single-person'):
            validate_clips([clip], pathlib.Path('.'))

    def test_real_decoder_sampling_timestamps_and_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            video = root / 'synthetic.avi'
            writer = cv2.VideoWriter(str(video), cv2.VideoWriter_fourcc(*'MJPG'), 12, (128, 96))
            self.assertTrue(writer.isOpened())
            for index in range(48):
                pixels = np.zeros((96, 128, 3), np.uint8)
                pixels[:] = (index * 4, 100, 200)
                writer.write(pixels)
            writer.release()
            clip = {**self.clip(video), 'start_sec': 1.0, 'end_sec': 3.0,
                    'expected_reps': 0, 'phases': [{'stage': 'test_only',
                    'start_sec': 1.0, 'end_sec': 3.0}]}
            manifest = root / 'clips.json'
            manifest.write_text(json.dumps({'clips': [clip]}))
            output = root / 'out'
            subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name(
                'import_workout_videos.py')), '--clips', str(manifest), '--output',
                str(output), '--fps', '3'], check=True, capture_output=True, text=True)
            rows = load_jsonl(output / 'train/manifest.jsonl')
            self.assertEqual(len(rows), 6)
            self.assertTrue(all(1 <= row['timestamp_sec'] < 3 for row in rows))
            self.assertTrue(all(row['stage'] == 'test_only' for row in rows))
            self.assertTrue(all(row['review_status'] == 'unlabeled' for row in rows))
            self.assertEqual(len({row['sha256'] for row in rows}), 6)
            self.assertTrue((output / 'train/review.png').is_file())

    def test_real_teacher_never_approves_holdout(self):
        teacher = pathlib.Path(__file__).resolve().parents[2] / (
            '.secrets/build-inputs/movenet_singlepose_lightning_int8_v4.tflite')
        if not teacher.exists():
            self.skipTest('private teacher model not available')
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            video = root / 'synthetic.avi'
            writer = cv2.VideoWriter(str(video), cv2.VideoWriter_fourcc(*'MJPG'), 10, (128, 96))
            self.assertTrue(writer.isOpened())
            for index in range(10):
                pixels = np.full((96, 128, 3), index * 20, np.uint8)
                writer.write(pixels)
            writer.release()
            manifest = root / 'clips.json'
            manifest.write_text(json.dumps({'clips': [self.clip(video, split='holdout')]}))
            output = root / 'out'
            subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name(
                'import_workout_videos.py')), '--clips', str(manifest), '--output',
                str(output), '--fps', '2', '--teacher', str(teacher)],
                check=True, capture_output=True, text=True)
            rows = load_jsonl(output / 'holdout/review-queue.jsonl')
            self.assertEqual(len(rows), 2)
            self.assertEqual(load_jsonl(output / 'holdout/labels.jsonl'), [])
            self.assertTrue(all(not row['training_eligible'] for row in rows))
            self.assertTrue(all(len(row['keypoints17']) == 17 for row in rows))
            self.assertTrue(all(row['label_source'] == 'movenet_teacher' for row in rows))

    def test_static_frames_keep_timeline_but_mark_training_duplicates(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            video = root / 'static.avi'
            writer = cv2.VideoWriter(str(video), cv2.VideoWriter_fourcc(*'MJPG'), 10, (128, 96))
            self.assertTrue(writer.isOpened())
            for _ in range(20):
                writer.write(np.full((96, 128, 3), 100, np.uint8))
            writer.release()
            manifest = root / 'clips.json'
            manifest.write_text(json.dumps({'clips': [self.clip(video)]}))
            output = root / 'out'
            subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name(
                'import_workout_videos.py')), '--clips', str(manifest), '--output',
                str(output), '--fps', '2'], check=True, capture_output=True, text=True)
            rows = load_jsonl(output / 'train/manifest.jsonl')
            self.assertEqual(len(rows), 4)
            self.assertEqual(sum(row['duplicate_for_training'] for row in rows), 3)
            self.assertEqual([row['timestamp_sec'] for row in rows], [0, 0.5, 1, 1.5])


if __name__ == '__main__':
    unittest.main()
