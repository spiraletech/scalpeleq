#!/usr/bin/env python3
"""Align genuine compiled client pixels to the asset-kit reference; never retouch actual."""
import argparse
import csv
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageEnhance

parser = argparse.ArgumentParser()
parser.add_argument('--reference', required=True, type=Path)
parser.add_argument('--actual', required=True, type=Path)
parser.add_argument('--before', type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
ref = Image.open(args.reference).convert('RGB')
actual = Image.open(args.actual).convert('RGB')
assert ref.size == (1645, 956), ref.size
assert actual.size == (1344, 756), actual.size
s = 1344 / 1645
pad = (756 - 921 * s) / 2
target = ref.transform(actual.size, Image.Transform.AFFINE,
                       (1/s, 0, 0, 0, 1/s, 35-pad/s),
                       resample=Image.Resampling.BICUBIC)
target.save(args.output / 'REFERENCE-client-1344x756.png')
actual.save(args.output / 'ACTUAL-native-VST3-1344x756.png')
Image.blend(target, actual, .5).save(args.output / 'overlay-50-percent.png')
diff = ImageChops.difference(target, actual)
diff.save(args.output / 'difference-raw.png')
ImageEnhance.Contrast(diff).enhance(2).save(args.output / 'difference-contrast-x2.png')

def mapped(box):
    x, y, w, h = box
    return tuple(round(v) for v in (x*s, (y-35)*s+pad, (x+w)*s, (y+h-35)*s+pad))

mask = np.ones((756, 1344), dtype=bool)
# Audio-, automation-, selection- and state-dependent graphics are excluded
# from material metrics. Raw difference images retain everything unmodified.
excluded = [(121,257,1116,250), (1304,341,163,57), (1484,263,92,270),
            (109,700,178,30), (109,783,178,30)]
for box in excluded:
    x1,y1,x2,y2=mapped(box);mask[y1:y2,x1:x2]=False

regions = {'header':(23,44,1598,80), 'band_rail':(42,141,1563,65),
           'analyzer_housing':(33,216,1583,346), 'navigation':(38,568,1573,62),
           'lower_plate':(25,646,1593,256)}
rows=[]
t=np.asarray(target,dtype=float)
for name,image in [('actual',actual)]+([('before',Image.open(args.before).convert('RGB'))] if args.before else []):
    assert image.size==actual.size
    a=np.asarray(image,dtype=float)
    for region,box in regions.items():
        x1,y1,x2,y2=mapped(box);valid=mask[y1:y2,x1:x2]
        aa=a[y1:y2,x1:x2][valid];tt=t[y1:y2,x1:x2][valid]
        rows.append({'image':name,'region':region,'mae_8bit_rgb':round(float(np.abs(aa-tt).mean()),3),
                     'reference_median_rgb':'/'.join(map(str,np.median(tt,axis=0).astype(int))),
                     'image_median_rgb':'/'.join(map(str,np.median(aa,axis=0).astype(int)))})
with (args.output/'material-diagnostics.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
canvas=Image.new('RGB',(1344*3,790),(28,28,28));draw=ImageDraw.Draw(canvas)
for i,(label,image) in enumerate([('BEFORE: verified 1.0.6',Image.open(args.before).convert('RGB') if args.before else actual),
                                  ('AFTER: supplied reference',target),('ACTUAL: compiled Windows VST3',actual)]):
    draw.text((i*1344+15,10),label,fill='white');canvas.paste(image,(i*1344,34))
canvas.save(args.output/'BEFORE-AFTER-ACTUAL.png')
(args.output/'alignment.json').write_text(json.dumps({'reference':[1645,956], 'host_chrome_px':35,
    'actual_client':[1344,756], 'uniform_scale':s,'vertical_center_pad':pad,
    'actual_resized_or_retouched':False,'metrics_exclusions_reference_rects':excluded,
    'note':'Pixel differences diagnose geometry/material changes; they are not proof of perceptual fidelity.'},indent=2))
print(json.dumps(rows,indent=2))
