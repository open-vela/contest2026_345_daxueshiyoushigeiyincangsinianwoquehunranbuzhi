"""Counter/quantization integration and event-scoring regression checks."""
import copy
import pathlib
import tempfile
import unittest

import numpy as np

from video_replay import Bridge, build_bridge, continuity, match_events, validate_annotations


def pose(angle):
    p = np.ones((8, 3), dtype=np.float32)
    for side in range(2):
        x = .3+side*.4
        for pair, y in enumerate((.1, .3, .5)):
            p[pair*2+side, :2] = x, y
        a = np.deg2rad(angle)
        p[6+side, :2] = x+.2*np.sin(a), .5-.2*np.cos(a)
    return p


class NativeReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.bridge = Bridge(build_bridge(pathlib.Path(cls.directory.name)))

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def test_actual_adaptive_counter_preserves_warmup_baseline(self):
        b = self.bridge
        h = b.create()
        try:
            for i, a in enumerate((166, 166, 150, 140, 150, 166)):
                result = b.step(h, pose(a), i*200, i >= 2)
            self.assertEqual(result[0], 1)
            self.assertEqual(result[6], 1)
        finally:
            b.lib.vf_replay_free(h)

    def test_low_confidence_interrupt_prevents_hidden_rep(self):
        b = self.bridge
        h = b.create()
        try:
            for i, a in enumerate((166, 166, 150, 140, 150, 166)):
                p = pose(a)
                if i == 3:
                    p[6, 2] = .1
                result = b.step(h, p, i*200, i >= 2)
            self.assertEqual(result[0], 0)
            result = b.step(h, pose(166)*np.array([1, 1, .29], dtype=np.float32), 1400, True)
            self.assertEqual(result[3], 0)  # eight >=.20 is insufficient without six >=.30
        finally:
            b.lib.vf_replay_free(h)

    def test_native_preprocessing_matches_2x2_area_and_model_quantization(self):
        rgb = np.random.default_rng(345).integers(0, 256, (192, 192, 3), dtype=np.uint8)
        average = rgb.astype(np.float32).reshape(96, 2, 96, 2, 3).mean(axis=(1, 3))
        for scale, zero in ((.0078431377187, -1), (.0067512495, 20)):
            expected = np.clip(np.rint((average/np.float32(127.5)-1)/np.float32(scale)+zero),
                               -128, 127).astype(np.int8)[None]
            np.testing.assert_array_equal(self.bridge.preprocess(rgb, scale, zero), expected)


class ScoringTests(unittest.TestCase):
    def test_missing_and_extra_cannot_cancel_each_other(self):
        result = match_events([1.1, 9.], [{'complete_sec': 1.}, {'complete_sec': 4.}], .5)
        self.assertEqual((result['missed'], result['extra'], result['matched']), (1, 1, 1))
        self.assertAlmostEqual(result['signed_delay_median_sec'], .1)

    def test_matching_maximizes_cardinality_before_distance(self):
        result = match_events([.3, .6], [{'complete_sec': 0.}, {'complete_sec': .4}], .35)
        self.assertEqual(result['matched'], 2)
        self.assertEqual(match_events([], [], 1.)['matched'], 0)
        self.assertEqual(match_events([1.], [], 1.)['extra'], 1)

    def test_signed_latency_and_bottom_reference(self):
        result = match_events([2.], [{'complete_sec': 2.2, 'bottom_sec': 1.}], .5)
        self.assertAlmostEqual(result['pairs'][0]['delay_sec'], -.2)
        self.assertEqual(result['pairs'][0]['bottom_to_count_sec'], 1.)

    def test_gaps_break_continuity(self):
        result = continuity([0., .1, 2., 2.1, 2.2], [True, True, True, False, True])
        self.assertEqual(result['longest_valid_run_frames'], 2)
        self.assertEqual(result['gaps_over_1sec'], 1)
        self.assertEqual(result['valid_to_invalid_transitions'], 1)

    def test_incomplete_annotation_cannot_be_reviewed_truth(self):
        clip = {'exercise': 'squat', 'warmup_sec': 2., 'end_sec': 10.,
                'expected_reps': 2, 'events_reviewed': True, 'events': [{'complete_sec': 5.}]}
        with self.assertRaisesRegex(ValueError, 'reviewed event count'):
            validate_annotations(clip)
        clip['expected_reps'] = 1
        validate_annotations(clip)
        broken = copy.deepcopy(clip)
        broken['events'][0]['bottom_sec'] = 6.
        with self.assertRaisesRegex(ValueError, 'start <= bottom'):
            validate_annotations(broken)


if __name__ == '__main__':
    unittest.main()
