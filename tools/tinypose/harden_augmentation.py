"""Training-only negative augmentation; never changes frozen labels or thresholds."""
import numpy as np
from comparison_support import BatchSampler


def occlude(image, points, rng, material, region):
    image, points = image.copy(), points.reshape(8, 3).copy()
    side = image.shape[0]
    xy = points[:, :2]*side
    if region == 'ankles':
        top = int(np.clip(np.floor((xy[4:6, 1].mean()+xy[6:8, 1].mean())/2), 0, side-1))
        rect = (0, top, side, side)
    elif region == 'torso':
        rect = (max(0, int(xy[:4, 0].min())-3), max(0, int(xy[:4, 1].min())-3),
                min(side, int(xy[:4, 0].max())+4), min(side, int(xy[:4, 1].max())+4))
    else:
        raise ValueError('unknown region')
    l, t, r, b = rect
    h, w = b-t, r-l
    yy, xx = np.indices((h, w))
    if material == 'black':
        patch = np.zeros((h, w, 3))
    elif material == 'gray':
        patch = np.full((h, w, 3), rng.uniform(.15, .8))
    elif material == 'cloth':
        color = rng.uniform(.1, .9, (1, 1, 3))
        patch = np.clip(color + rng.normal(0, .035, (h, w, 3)), 0, 1)
    elif material == 'wood':
        base = np.array([.43, .27, .13])[None, None]
        patch = np.clip(base+.08*np.sin(xx[..., None]*rng.uniform(.4, 1.3))+
                        rng.normal(0, .02, (h, w, 3)), 0, 1)
    else:
        raise ValueError('unknown material')
    image[t:b, l:r] = patch*2-1
    covered = (xy[:, 0] >= l) & (xy[:, 0] < r) & (xy[:, 1] >= t) & (xy[:, 1] < b)
    points[covered, 2] = 0.
    return image.astype(np.float32), points.reshape(24), covered


class HardenSampler:
    def __init__(self, pools, records, seed):
        self.base = BatchSampler(pools, {'sc2336_train': 32}, seed, True)
        self.rng = np.random.default_rng(seed+7919)
        x, y = pools['sc2336_train']
        self.empty = np.array([i for i,r in enumerate(records)
                               if r.get('sample_kind') == 'empty_background_negative'])
        self.people = np.array([i for i,r in enumerate(records)
                                if not r.get('sample_kind') and
                                sum(p['confidence'] >= .2 for p in r['keypoints']) >= 6])
        if not len(self.empty) or not len(self.people):
            raise ValueError('missing training-only negative or positive sources')
        self.x, self.y = x, y

    def next(self):
        x, y, domains = self.base.next()
        for j in range(24, 28):
            i = self.rng.choice(self.empty)
            rgb = (self.x[i]+1)*.5
            black = np.all(rgb == 0, axis=-1, keepdims=True)
            rgb = np.clip(np.power(rgb, self.rng.uniform(.7, 1.5))*self.rng.uniform(.25, 1.5), 0, 1)
            x[j] = np.where(black, 0, rgb)*2-1
            y[j] = self.y[i].copy()
            y[j].reshape(8, 3)[:, 2] = 0
            domains[j] = 'training_empty_gain'
        for j in range(28, 32):
            i = self.rng.choice(self.people)
            material = ('black', 'gray', 'cloth', 'wood')[self.rng.integers(4)]
            region = ('ankles', 'torso')[self.rng.integers(2)]
            x[j], y[j], _ = occlude(self.x[i], self.y[i], self.rng, material, region)
            domains[j] = 'training_occlusion_'+material
        return x, y, domains
