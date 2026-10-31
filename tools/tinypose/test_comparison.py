#!/usr/bin/env python3
"""Regression tests for data isolation and fair comparison batches."""
import json
import pathlib
import tempfile
import unittest

import numpy as np

from comparison_support import (BatchSampler, eligible, photometric, record_digest,
                                verify_approvals, verify_manifest)
from dataset_import import sha256
from prepare_comparison import check_disjoint


class SamplingTests(unittest.TestCase):
    def pools(self):
        rng = np.random.default_rng(123)
        x = rng.uniform(-0.9, 0.9, (10, 96, 96, 3)).astype(np.float32)
        x[:, :4] = -1
        y = rng.uniform(0, 1, (10, 24)).astype(np.float32)
        return {'sc': (x, y), 'coco': (x[::-1], y[::-1])}

    def test_mixed_photo_uses_identical_samples_and_labels(self):
        pools = self.pools()
        plain = BatchSampler(pools, {'sc': 16, 'coco': 16}, 34596)
        augmented = BatchSampler(pools, {'sc': 16, 'coco': 16}, 34596, True)
        for _ in range(3):
            x, y, domains = plain.next()
            photo, photo_y, photo_domains = augmented.next()
            np.testing.assert_array_equal(y, photo_y)
            self.assertEqual(domains, photo_domains)
            self.assertEqual(domains.count('sc'), 16)
            self.assertEqual(domains.count('coco'), 16)
            self.assertEqual(x.shape, (32, 96, 96, 3))
            self.assertFalse(np.array_equal(x, photo))
            self.assertTrue(np.isfinite(photo).all())
            self.assertTrue(np.all((-1 <= photo) & (photo <= 1)))

    def test_photometry_preserves_letterbox_and_geometry_shape(self):
        images = self.pools()['sc'][0]
        before = images.copy()
        result = photometric(images, np.random.default_rng(1))
        np.testing.assert_array_equal(result[:, :4], images[:, :4])
        np.testing.assert_array_equal(images, before)
        self.assertEqual(result.shape, images.shape)

    def test_legacy_negatives_and_low_confidence_filter(self):
        points = [{'confidence': 0.9}] * 4 + [{'confidence': 0.0}] * 4
        self.assertFalse(eligible({'keypoints': points}))
        self.assertTrue(eligible({'keypoints': points, 'sample_kind': 'lower_body_cropped_negative'}))
        self.assertTrue(eligible({'keypoints': [{'confidence': 0.0}]*8,
                                  'sample_kind': 'empty_background_negative'}))


class IsolationTests(unittest.TestCase):
    def test_decoded_image_overlap_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'leakage'):
            check_disjoint({'training': {'same'}, 'validation': {'same'}})
        check_disjoint({'training': {'a'}, 'validation': {'b'}})

    def test_pending_sample_cannot_enter_selected_labels(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            rows = [{'image': 'a.ppm', 'sha256': 'a'}, {'image': 'b.ppm', 'sha256': 'b'}]
            source = root / 'source.jsonl'
            source.write_text(''.join(json.dumps(row)+'\n' for row in rows))
            preview = root / 'preview.png'
            preview.write_bytes(b'preview')
            approval = {'source_labels': str(source), 'source_labels_sha256': sha256(source),
                        'reviewed_preview': str(preview), 'reviewed_preview_sha256': sha256(preview),
                        'records': [{'image': row['image'], 'image_sha256': row['sha256'],
                                     'record_sha256': record_digest(row),
                                     'decision': 'keep' if i == 0 else 'pending'}
                                    for i, row in enumerate(rows)]}
            review = root / 'review.json'
            review.write_text(json.dumps(approval))
            selected = root / 'selected.jsonl'
            selected.write_text(json.dumps(rows[0])+'\n')
            self.assertEqual(verify_approvals(selected, [review]), rows[:1])
            selected.write_text(source.read_text())
            with self.assertRaisesRegex(ValueError, 'exactly match'):
                verify_approvals(selected, [review])
            selected.write_text(json.dumps({**rows[0], 'extra': 'tampered'})+'\n')
            with self.assertRaisesRegex(ValueError, 'changed'):
                verify_approvals(selected, [review])

    def test_frozen_data_tampering_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            source, data = root / 'source', root / 'data.jsonl'
            source.write_bytes(b'source')
            data.write_bytes(b'frozen')
            path = root / 'experiment.json'
            path.write_text(json.dumps({'source_hashes': {str(source): sha256(source)},
                'datasets': {'train': {'path': data.name, 'sha256': sha256(data)}}}))
            verify_manifest(path)
            data.write_bytes(b'tampered')
            with self.assertRaisesRegex(ValueError, 'frozen dataset'):
                verify_manifest(path)


if __name__ == '__main__':
    unittest.main()
