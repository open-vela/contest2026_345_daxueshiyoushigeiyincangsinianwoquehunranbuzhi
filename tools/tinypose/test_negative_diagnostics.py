"""Check diagnostic geometry and confidence-summary semantics."""
import unittest
import numpy as np

from evaluate_negatives import mask_region, summarize


class DiagnosticTests(unittest.TestCase):
    def setUp(self):
        self.image = np.full((192, 192, 3), 70, dtype=np.uint8)
        self.points = [{'x': .4+side*.2, 'y': y, 'confidence': 1.}
                       for y in (.2, .4, .6, .8) for side in range(2)]

    def test_ankle_mask_covers_two_centers_without_mutating_source(self):
        for kind, value in (('ankles_black', 0), ('ankles_gray', 128)):
            with self.subTest(kind=kind):
                result, covered, rect = mask_region(self.image, self.points, kind)
                np.testing.assert_array_equal(covered, [False]*6+[True]*2)
                self.assertTrue(np.all(result[rect[1]:] == value))
                self.assertTrue(np.all(self.image == 70))

    def test_torso_mask_covers_all_upper_centers(self):
        result, covered, rect = mask_region(self.image, self.points, 'torso_gray')
        np.testing.assert_array_equal(covered, [True]*4+[False]*4)
        self.assertTrue(np.all(result[rect[1]:rect[3], rect[0]:rect[2]] == 128))

    def test_no_mask_does_not_fabricate_hidden_points(self):
        result, covered, rect = mask_region(self.image, self.points, 'none')
        np.testing.assert_array_equal(result, self.image)
        self.assertFalse(covered.any())
        self.assertIsNone(rect)

    def test_counting_acceptance_does_not_mean_strict_acceptance_or_body_ready(self):
        record = {'hidden_count': 2, 'hidden_rejected_030': 1, 'hidden_rejected_020': 0,
                  'strict_all_eight': False, 'counting_ready': True,
                  'body_status': 'STABILIZING', 'hidden_reasons': {'synthetic_mask': 2}}
        result = summarize([record])
        self.assertEqual(result['hidden_rejection_at_030'], .5)
        self.assertEqual(result['hidden_rejection_at_020'], 0.)
        self.assertEqual(result['strict_all_eight_accepted'], 0)
        self.assertEqual(result['counting_ready_frames'], 1)
        self.assertEqual(result['body_single_frame_geometry_pass'], 1)
        record.update(hidden_count=0, hidden_rejected_030=0, hidden_reasons={})
        self.assertIsNone(summarize([record])['hidden_rejection_at_030'])


if __name__ == '__main__':
    unittest.main()
