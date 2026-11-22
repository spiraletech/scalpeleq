#!/usr/bin/env python3
"""Deterministic 1.0.8.5 Formidable material exporter.

The SVG masters remain the human-readable design source. This Pillow exporter
mirrors their geometry for CI environments without an SVG raster delegate.
No text, control value, or DSP state is baked into these surfaces.
"""
from pathlib import Path
from PIL import Image, ImageDraw
import hashlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Assets/Scalpel/Formidable/1.0.8.5"
S = 2


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(4))


def vertical(size, stops):
    w, h = size
    image = Image.new("RGBA", (w, h))
    pixels = image.load()
    for y in range(h):
        u = y / max(1, h - 1)
        for j in range(len(stops) - 1):
            if stops[j][0] <= u <= stops[j + 1][0]:
                p, a = stops[j]
                q, b = stops[j + 1]
                c = lerp(a, b, (u - p) / max(1e-9, q - p))
                break
        else:
            c = stops[-1][1]
        for x in range(w):
            pixels[x, y] = c
    return image


def rr_mask(size, box, radius):
    mask = Image.new("L", size)
    ImageDraw.Draw(mask).rounded_rectangle(box, radius=radius, fill=255)
    return mask


def composite_clipped(base, layer, mask):
    alpha = Image.composite(layer.getchannel("A"), Image.new("L", base.size), mask)
    layer = layer.copy(); layer.putalpha(alpha)
    base.alpha_composite(layer)


def downsample(image):
    return image.resize((image.width // S, image.height // S), Image.Resampling.LANCZOS)


def shell():
    size = (1670 * S, 942 * S)
    image = Image.new("RGBA", size)
    d = ImageDraw.Draw(image)
    d.rounded_rectangle((3*S,3*S,1667*S,939*S),47*S,fill=(0,0,0,226))
    body = vertical(size, [(0,(24,26,29,255)),(.18,(10,11,13,255)),(.52,(3,4,5,255)),(.8,(16,18,20,255)),(1,(2,3,4,255))])
    mask = rr_mask(size,(7*S,7*S,1663*S,935*S),43*S)
    image.alpha_composite(Image.composite(body,Image.new("RGBA",size),mask))
    sweep = Image.new("RGBA", size)
    sd = ImageDraw.Draw(sweep)
    sd.polygon([(-70*S,45*S),(500*S,-30*S),(1740*S,-8*S),(1740*S,96*S),(790*S,145*S),(-70*S,142*S)],fill=(255,255,255,24))
    sd.polygon([(-30*S,790*S),(470*S,690*S),(1050*S,985*S),(1730*S,710*S),(1730*S,948*S),(-30*S,948*S)],fill=(255,255,255,7))
    composite_clipped(image,sweep,mask)
    d = ImageDraw.Draw(image)
    for inset, colour, width in [(7,(226,227,226,205),4),(13,(255,255,252,55),2),(18,(0,0,0,220),3)]:
        d.rounded_rectangle((inset*S,inset*S,(1670-inset)*S,(942-inset)*S),(47-inset)*S,outline=colour,width=width*S)
    return downsample(image)


def graph_glass():
    size=(1529*S,392*S); image=Image.new("RGBA",size)
    mask=rr_mask(size,(2*S,2*S,1527*S,390*S),22*S)
    layer=Image.new("RGBA",size); d=ImageDraw.Draw(layer)
    d.polygon([(-80*S,-10*S),(650*S,-10*S),(240*S,230*S),(-80*S,320*S)],fill=(255,255,255,22))
    d.polygon([(1040*S,392*S),(1529*S,212*S),(1529*S,392*S)],fill=(255,255,255,9))
    d.polygon([(395*S,-10*S),(660*S,-10*S),(535*S,392*S),(390*S,392*S)],fill=(255,255,255,3))
    composite_clipped(image,layer,mask)
    d=ImageDraw.Draw(image)
    d.rounded_rectangle((2*S,2*S,1527*S,390*S),22*S,outline=(248,248,246,78),width=2*S)
    d.rounded_rectangle((5*S,5*S,1524*S,387*S),18*S,outline=(0,0,0,170),width=2*S)
    return downsample(image)


def control_glass():
    size=(1570*S,317*S); image=Image.new("RGBA",size)
    d=ImageDraw.Draw(image); d.rounded_rectangle((0,0,1570*S,317*S),28*S,fill=(0,0,0,220))
    body=vertical(size,[(0,(10,11,13,255)),(.32,(2,3,4,255)),(.72,(5,6,7,255)),(1,(1,2,3,255))])
    mask=rr_mask(size,(2*S,2*S,1568*S,315*S),26*S)
    image.alpha_composite(Image.composite(body,Image.new("RGBA",size),mask))
    layer=Image.new("RGBA",size); ld=ImageDraw.Draw(layer)
    ld.polygon([(-30*S,-10*S),(700*S,-10*S),(260*S,185*S),(-30*S,235*S)],fill=(255,255,255,18))
    ld.polygon([(1140*S,317*S),(1570*S,172*S),(1570*S,317*S)],fill=(255,255,255,9))
    ld.polygon([(860*S,0),(1010*S,0),(910*S,317*S),(825*S,317*S)],fill=(255,255,255,2))
    composite_clipped(image,layer,mask)
    d=ImageDraw.Draw(image)
    d.rounded_rectangle((2*S,2*S,1568*S,315*S),26*S,outline=(225,226,224,145),width=3*S)
    d.rounded_rectangle((7*S,7*S,1563*S,310*S),21*S,outline=(255,255,255,25),width=S)
    d.rounded_rectangle((10*S,10*S,1560*S,307*S),19*S,outline=(0,0,0,190),width=2*S)
    return downsample(image)


def key(size, selected=False, hover=False, pressed=False, tab=False):
    w,h=size; size2=(w*S,h*S); image=Image.new("RGBA",size2); d=ImageDraw.Draw(image)
    radius=(11 if tab else 13)*S
    d.rounded_rectangle((3*S,5*S,(w-3)*S,(h-3)*S),radius,fill=(0,0,0,185))
    if pressed:
        box=(5*S,6*S,(w-5)*S,(h-8)*S); top=(8,9,10,255); bottom=(19,21,23,255)
    else:
        box=(2*S,2*S,(w-2)*S,(h-10)*S); top=((52,55,58,255) if hover else (42,44,47,255)); bottom=(5,6,7,255)
    face=vertical(size2,[(0,top),(.24,(17,19,21,255)),(.75,bottom),(1,(23,25,27,255))])
    m=rr_mask(size2,box,radius); image.alpha_composite(Image.composite(face,Image.new("RGBA",size2),m))
    d=ImageDraw.Draw(image)
    d.rounded_rectangle(box,radius,outline=(240,240,237,170 if hover else 135),width=2*S)
    d.line((14*S,12*S,(w-14)*S,12*S),fill=(255,255,255,50 if hover else 34),width=2*S)
    if selected:
        x0=(110 if tab else 26)*S; x1=(w-110 if tab else w-26)*S; y=(h-12)*S
        for width,alpha in [(10,22),(4,230)]: d.rounded_rectangle((x0,y-width*S//2,x1,y+width*S//2),2*S,fill=(239,25,38,alpha))
    return downsample(image)


def save(name,image):
    image.save(OUT/name,optimize=True)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    save("f1085_lacquer_shell.png",shell())
    save("f1085_graph_glass.png",graph_glass())
    save("f1085_control_glass.png",control_glass())
    for state in ("idle","hover","selected","pressed"):
        flags={"selected":state=="selected","hover":state=="hover","pressed":state=="pressed"}
        save(f"f1085_band_{state}.png",key((408,124),**flags))
        save(f"f1085_tab_{state}.png",key((1028,112),tab=True,**flags))
    files=sorted(OUT.glob("f1085_*.png"))
    with (OUT/"SHA256SUMS.txt").open("w",encoding="utf-8") as out:
        for file in files: out.write(f"{hashlib.sha256(file.read_bytes()).hexdigest()}  {file.name}\n")


if __name__ == "__main__": main()
