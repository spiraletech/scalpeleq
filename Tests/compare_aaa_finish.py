"""Compare compiled pixels at native size; never resize the actual plugin."""
from pathlib import Path
import argparse
import json
import shutil
import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFont

parser=argparse.ArgumentParser()
parser.add_argument('--before',type=Path,required=True)
parser.add_argument('--concept',type=Path,required=True)
parser.add_argument('--actual',type=Path,required=True)
parser.add_argument('--color',type=Path,required=True)
parser.add_argument('--geometry',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
args.out.mkdir(parents=True,exist_ok=True)
before=Image.open(args.before).convert('RGB')
if before.size!=(1350,785): raise ValueError('Wrong functional screenshot dimensions')
# The supplied screenshot includes FL Studio chrome. Only its authentic
# 1344x756 editor content is used; no invented host window is composited.
before_crop=(3,27,1347,783)
before=before.crop(before_crop)
actual=Image.open(args.actual).convert('RGB')
color=Image.open(args.color).convert('RGB')
if actual.size!=(1344,756) or color.size!=(1344,756):
    raise ValueError('Compiled UI must remain native 1344x756')
concept=Image.open(args.concept).convert('RGB')
# Approved wide concept has a dark presentation margin, unlike the editor.
# This normalization is for MATERIAL comparison only, never layout authority.
concept_crop=(5,9,1640,929)
concept=concept.crop(concept_crop).resize(actual.size,Image.Resampling.LANCZOS)
before.save(args.out/'before-faceplate.png')
concept.save(args.out/'concept-material-reference.png')
shutil.copyfile(args.actual,args.out/'actual-CARVE.png')
shutil.copyfile(args.color,args.out/'actual-COLOR.png')
shutil.copyfile(args.geometry,args.out/'component-geometry.csv')
Image.blend(before,actual,.5).save(args.out/'aligned-50-percent-overlay.png')
diff=ImageChops.difference(before,actual)
diff.save(args.out/'absolute-difference.png')
try: font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',20)
except OSError: font=ImageFont.load_default()
comparison=Image.new('RGB',(1344*3,790),'#111417')
draw=ImageDraw.Draw(comparison)
for i,(label,image) in enumerate([
    ('BEFORE: functional geometry and control state',before),
    ('CONCEPT: materials only; geometry differs',concept),
    ('ACTUAL: compiled CARVE, unchanged editor bounds',actual)]):
    comparison.paste(image,(1344*i,34));draw.text((1344*i+12,7),label,font=font,fill='#F3F7FA')
comparison.save(args.out/'before-concept-actual.png')
regions={
    'chassis':(660,6,720,18),'faceplate':(450,31,520,43),
    'analyzer':(510,210,650,240),'control_deck':(400,710,600,730),
    'silver_top':(408,112,480,117),'knob_center':(432,615,450,628)}
def sample(image,box):
    rgb=np.asarray(image.crop(box)).reshape(-1,3)
    return '#'+''.join(f'{int(x):02X}' for x in np.median(rgb,axis=0))
metrics={
    'before_source_dimensions':[1350,785],
    'before_host_chrome_crop_ltrb':list(before_crop),
    'editor_dimensions':list(actual.size),
    'concept_material_crop_ltrb':list(concept_crop),
    'fixture':{'band':3,'carve':4,'hz':215.87,'depth_db':0,'q':.300,'carve_on':True},
    'rgb_mean_absolute_difference':float(np.abs(np.asarray(before,dtype=float)-np.asarray(actual,dtype=float)).mean()),
    'samples':{name:{'before':sample(before,box),'concept':sample(concept,box),'actual':sample(actual,box),'roi':list(box)} for name,box in regions.items()},
    'notes':[
        'Pixel difference includes intentionally changed materials and text, not only errors.',
        'Component bounds are checked in the compiled C++ fixture and recorded in CSV.',
        'Concept geometry and larger knob diameters are not substituted for the locked functional layout.',
        'Neither the actual screenshot nor its overlay is a regenerated mockup.',
        'Native JUCE Windows capture does not establish FL Studio host verification.']}
(args.out/'comparison-metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
print(json.dumps(metrics,indent=2))
