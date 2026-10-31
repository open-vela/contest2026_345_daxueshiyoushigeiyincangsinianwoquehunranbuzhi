import unittest
from eval_registry import audit


def row(id,split,**kw):
    return {'id':id,'split':split,'sha256':'file-'+id,'root_source_sha256':'raw-'+id,
            'subject_id':'person-'+id,'session_id':'session-'+id,'background_id':'scene-'+id,**kw}


class RegistryTests(unittest.TestCase):
    def test_empty_is_pending_not_pass(self):
        self.assertEqual(audit([])['status'],'PENDING_NO_RECORDINGS')

    def test_same_person_or_raw_derivative_cross_split_fails(self):
        for overrides in ({'subject_id':'person-a'},{'ancestor_sha256':['file-a']},
                          {'session_id':'session-a'},{'background_id':'scene-a'}):
            with self.subTest(overrides=overrides):
                self.assertEqual(audit([row('a','train'),row('b','acceptance',**overrides)])['status'],'FAIL')

    def test_unknown_acceptance_identity_fails(self):
        self.assertEqual(audit([row('a','acceptance',subject_id=None)])['status'],'FAIL')

    def test_calibration_development_overlap_is_not_blind_test(self):
        a=row('a','train');b={**a,'id':'b','split':'calibration'};c={**a,'id':'c','split':'development'}
        self.assertEqual(audit([a,b,c])['status'],'LEGACY_ONLY_NO_INDEPENDENT_ACCEPTANCE')

    def test_independent_declared_acceptance_can_pass_but_used_data_cannot(self):
        self.assertEqual(audit([row('a','train'),row('b','acceptance')])['status'],'PROVENANCE_CHECK_PASS')
        self.assertEqual(audit([row('a','acceptance',used_for_tuning=True)])['status'],'FAIL')

    def test_empty_scene_uses_source_and_scene_isolation_not_fake_person_identity(self):
        self.assertEqual(audit([row('a','train',subject_id='NO_PERSON',no_person_confirmed=True),
                                row('b','acceptance',subject_id='NO_PERSON',no_person_confirmed=True)])['status'],
                         'PROVENANCE_CHECK_PASS')
        self.assertEqual(audit([row('a','acceptance',subject_id='NO_PERSON')])['status'],'FAIL')


if __name__=='__main__':unittest.main()
