from PIL import Image
from collections import deque
from pathlib import Path
root=Path(__file__).resolve().parents[1];base=root/"art-source";res=root/'resources'
# Approved concept sheets; extract assets without repainting or synthetic typography.
sheet=Image.open(base/'buttons.png').convert('RGBA')
for name,box in [('geopass',(661,73,1181,594)),('tier-shop',(73,659,593,1181)),('map',(661,659,1181,1181))]:
 sheet.crop(box).resize((512,512),Image.Resampling.LANCZOS).save(res/f'geobr-button-{name}.png')
im=Image.open(base/'main-levels.png').convert('RGBA');im.crop((22,22,1233,1233)).resize((512,512),Image.Resampling.LANCZOS).save(res/'geobr-button-main-levels.png')
im=Image.open(base/'crates.png').convert('RGBA')
for i,box in enumerate([(55,95,495,418),(550,95,995,418),(1045,95,1490,418),(55,532,495,854),(550,532,995,854),(1045,532,1490,854)],1):
 crop=im.crop(box);p=crop.load();w,h=crop.size;q=deque([(x,0) for x in range(w)]+[(x,h-1) for x in range(w)]+[(0,y) for y in range(h)]+[(w-1,y) for y in range(h)]);seen=set()
 while q:
  x,y=q.popleft()
  if (x,y) in seen or not(0<=x<w and 0<=y<h):continue
  seen.add((x,y));r,g,b,a=p[x,y]
  if r<55 and g<85 and b<145:
   p[x,y]=(0,0,0,0);q.extend([(x+1,y),(x-1,y),(x,y+1),(x,y-1)])
 crop=crop.crop(crop.getbbox());crop.thumbnail((370,304),Image.Resampling.LANCZOS);out=Image.new('RGBA',(384,320));out.alpha_composite(crop,((384-crop.width)//2,(320-crop.height)//2));out.save(res/f'geobr-crate-{i}.png')
im=Image.open(base/'themes.png').convert('RGB')
for i,name in enumerate(['fire','aurora','cosmos']):
 im.crop((0,round(i*im.height/3),im.width,round((i+1)*im.height/3))).save(res/f'geobr-theme-{name}.png')
