"""Full-faceplate COLOR comparison; never modifies or registers ACTUAL."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops
import numpy as np

p = argparse.ArgumentParser()
p.add_argument('--reference', required=True, type=Path)
p.add_argument('--actual', required=True, type=Path)
p.add_argument('--out', required=True, type=Path)
a = p.parse_args()
expected = '8d7e9de0d37b4253dca5d0479af2eef6a1223b5aefd27e9c75f883877fa48297'
assert hashlib.sha256(a.reference.read_bytes()).hexdigest() == expected, 'Unapproved reference'
ref = Image.open(a.reference).convert('RGB')
actual = Image.open(a.actual).convert('RGB')
assert ref.size == (1536, 864) and actual.size == (1344, 756)
ref = ref.resize(actual.size, Image.Resampling.LANCZOS)
a.out.mkdir(parents=True, exist_ok=True)
ref.save(a.out / 'reference-1344x756.png')
actual.save(a.out / 'actual-1344x756.png')
Image.blend(ref, actual, .5).save(a.out / 'overlay-50.png')
ImageChops.difference(ref, actual).save(a.out / 'difference-absolute.png')
delta = np.abs(np.asarray(ref, dtype=float) - np.asarray(actual, dtype=float))
mask = np.ones((756, 1344), dtype=bool)
# Live lens, peak text, stereo meter segments: exclude from static-material MAE.
for x,y,w,h in [(98,187,1044,253),(1210,280,165,58),(1390,205,92,245)]:
    x,y,w,h = [round(v * .875) for v in (x,y,w,h)]
    mask[y:y+h,x:x+w] = False
metrics = {'size':[1344,756], 'uniform_reference_scale':.875,
           'reference_sha256':expected,
           'actual_sha256':hashlib.sha256(a.actual.read_bytes()).hexdigest(),
           'static_rgb_mae_0_255':float(delta[mask].mean()),
           'full_rgb_mae_0_255':float(delta.mean()),
           'caution':'Pixel metrics are diagnostic; texture, reference lighting and font rasterization differ.'}
(a.out / 'metrics.json').write_text(json.dumps(metrics, indent=2)+'\n')
print(json.dumps(metrics, indent=2))
