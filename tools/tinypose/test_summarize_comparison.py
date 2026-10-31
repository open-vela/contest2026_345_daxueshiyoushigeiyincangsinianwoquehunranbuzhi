"""Check that public gains cannot conceal target-domain regression."""
import copy
import unittest

from summarize_comparison import evaluate_gates, finite_json


class AcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.baseline = {
            'sc2336_regression-clean': {'pck_10px': .95, 'mean_error_px': 4., 'p95_error_px': 9.},
            'coco_validation-clean': {'pck_10px': .1},
            'sc2336_empty_development-clean': {'hidden_rejection_rate': 1.},
            'sc2336_cropped_development-clean': {'hidden_rejection_rate': 1.},
        }
        self.gates = {'sc2336_pck10_max_drop': .01, 'sc2336_mean_error_max_increase_px': .5,
                      'sc2336_p95_error_max_increase_px': 1., 'coco_pck10_target_gain': .05,
                      'development_hidden_rejection_max_drop': .02}

    def test_inclusive_thresholds(self):
        candidate = copy.deepcopy(self.baseline)
        candidate['sc2336_regression-clean'] = {
            'pck_10px': .94, 'mean_error_px': 4.5, 'p95_error_px': 10.}
        candidate['coco_validation-clean']['pck_10px'] = .15
        candidate['sc2336_empty_development-clean']['hidden_rejection_rate'] = .98
        self.assertTrue(evaluate_gates(self.baseline, candidate, self.gates)['all_pass'])

    def test_public_gain_cannot_override_domain_or_negative_failure(self):
        for role, metric, value, failure in (
            ('sc2336_regression-clean', 'pck_10px', .93, 'sc2336_pck10_retention'),
            ('sc2336_empty_development-clean', 'hidden_rejection_rate', .97,
             'sc2336_empty_development-clean_hidden_rejection_retention'),
        ):
            with self.subTest(role=role):
                candidate = copy.deepcopy(self.baseline)
                candidate['coco_validation-clean']['pck_10px'] = .5
                candidate[role][metric] = value
                result = evaluate_gates(self.baseline, candidate, self.gates)
                self.assertFalse(result['all_pass'])
                self.assertIn(failure, result['failed'])

    def test_nonfinite_reports_are_rejected(self):
        self.assertFalse(finite_json({'history': [{'loss': float('nan')}]}))
        self.assertFalse(finite_json([float('inf')]))
        self.assertTrue(finite_json({'error': None, 'pck': .5}))


if __name__ == '__main__':
    unittest.main()
