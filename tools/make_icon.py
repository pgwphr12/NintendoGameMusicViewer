"""Generate the original scope/music icon; no external artwork is used."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parent.parent / 'src/app/resources'
root.mkdir(parents=True, exist_ok=True)
svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
<rect x="8" y="8" width="240" height="240" rx="54" fill="#101b2d"/>
<rect x="22" y="22" width="212" height="212" rx="42" fill="none" stroke="#314761" stroke-width="4"/>
<path d="M44 148H68V104H92V164H116V84H140V148H158" fill="none" stroke="#50e6c6" stroke-width="12" stroke-linejoin="round" stroke-linecap="round"/>
<path d="M178 154V66L214 58V138" fill="none" stroke="#ae8aff" stroke-width="12" stroke-linejoin="round" stroke-linecap="round"/>
<ellipse cx="166" cy="160" rx="19" ry="13" fill="#ae8aff"/>
<ellipse cx="202" cy="144" rx="19" ry="13" fill="#ae8aff"/>
</svg>'''
(root/'viewer.svg').write_text(svg, encoding='utf-8')
scale = 4
im = Image.new('RGBA', (256*scale, 256*scale))
d = ImageDraw.Draw(im)
def box(v): return tuple(int(x*scale) for x in v)
d.rounded_rectangle(box((8,8,248,248)), 54*scale, fill='#101b2d')
d.rounded_rectangle(box((22,22,234,234)),42*scale,outline='#314761',width=4*scale)
def stroke(points, color):
    points = [(x*scale,y*scale) for x,y in points]
    d.line(points, fill=color, width=12*scale, joint='curve')
    for x,y in points: d.ellipse((x-6*scale,y-6*scale,x+6*scale,y+6*scale),fill=color)
stroke([(44,148),(68,148),(68,104),(92,104),(92,164),(116,164),(116,84),(140,84),(140,148),(158,148)],'#50e6c6')
stroke([(178,154),(178,66),(214,58),(214,138)],'#ae8aff')
d.ellipse(box((147,147,185,173)),fill='#ae8aff')
d.ellipse(box((183,131,221,157)),fill='#ae8aff')
im = im.resize((256,256),Image.Resampling.LANCZOS)
im.save(root/'viewer.png')
im.save(root/'viewer.ico',sizes=[(n,n) for n in (16,24,32,48,64,128,256)])
print('Generated original SVG, PNG and seven-size ICO')
