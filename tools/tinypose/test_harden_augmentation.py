import unittest
import numpy as np
from harden_augmentation import occlude, HardenSampler


class HardenTests(unittest.TestCase):
    def test_all_materials_zero_only_covered_confidence_and_preserve_input(self):
        x=np.full((96,96,3),.2,dtype=np.float32)
        y=np.array([[.4+i%2*.2,.2+i//2*.2,.8] for i in range(8)],dtype=np.float32)
        for material in ('black','gray','cloth','wood'):
            for region in ('ankles','torso'):
                with self.subTest(material=material,region=region):
                    image,target,covered=occlude(x,y,np.random.default_rng(17),material,region)
                    self.assertTrue(covered.any());self.assertTrue(np.isfinite(image).all())
                    self.assertTrue(np.all((image>=-1)&(image<=1)))
                    t=target.reshape(8,3)
                    np.testing.assert_array_equal(t[:,:2],y[:,:2])
                    self.assertTrue(np.all(t[covered,2]==0))
                    np.testing.assert_array_equal(t[~covered,2],y[~covered,2])
                    self.assertTrue(np.all(x==.2));self.assertTrue(np.all(y[:,2]==.8))

    def test_negatives_are_sampled_from_training_pool_and_deterministic(self):
        x=np.random.default_rng(1).uniform(-1,1,(4,96,96,3)).astype(np.float32)
        y=np.array([[[.4+i%2*.2,.2+i//2*.2,.8] for i in range(8)]]*4,dtype=np.float32).reshape(4,24)
        y[0].reshape(8,3)[:,2]=0
        r=[{'sample_kind':'empty_background_negative','keypoints':[{'confidence':0}]*8}]+[{'keypoints':[{'confidence':.8}]*8}]*3
        pools={'sc2336_train':(x,y)}
        a=HardenSampler(pools,r,42);b=HardenSampler(pools,r,42)
        ax,ay,d=a.next();bx,by,_=b.next()
        np.testing.assert_array_equal(ax,bx);np.testing.assert_array_equal(ay,by)
        self.assertEqual(d[24:28],['training_empty_gain']*4)
        self.assertTrue(np.all(ay[24:28].reshape(4,8,3)[:,:,2]==0))


if __name__=='__main__':unittest.main()
