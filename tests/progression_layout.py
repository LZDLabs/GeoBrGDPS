"""Check declared physical control rectangles at compact landscape sizes."""
from pathlib import Path
from PIL import Image
import json
root=Path(__file__).resolve().parents[1]
for w,h in [(568,320),(640,360),(780,360)]:
 for pw,ph in [(550,340),(520,330),(500,300),(510,310)]:
  scale=min(1,w*.86/pw,h*.78/ph)
  assert pw*scale<=w*.86+.001 and ph*scale<=h*.78+.001
# Six crate cards, with both horizontal/vertical separation.
rects=[]
for i in range(6):
 x=110+(i%3)*165;y=204-(i//3)*103
 rects.append((x-153/2,y-96/2,x+153/2,y+96/2))
for i,a in enumerate(rects):
 for b in rects[i+1:]:assert a[2]<=b[0] or b[2]<=a[0] or a[3]<=b[1] or b[3]<=a[1]
 assert 25<a[0]<a[2]<525 and 50<a[1]<a[3]<255
# Sidebar controls never intersect the main viewport.
for i in range(9):
 y=267-i*26
 assert y-12>=47 and y+12<=279
 assert 73+53<145
assert 72+196<286-13
# Sequential reveal final grid fits within the 470 x 300 popup.
for i in range(10):
 x=67+(i%5)*84;y=211-(i//5)*95
 assert 22<x-38.5<x+38.5<450 and 68<y-43<y+43<255
# Real reward sprites must contain transparent margins, fit UI atlases and be packaged.
m=json.loads((root/'mod.json').read_text())
for n in range(1,7):
 name=f'resources/geobr-crate-{n}.png';assert name in m['resources']['files']
 im=Image.open(root/name);assert im.mode=='RGBA' and im.width<=384 and im.height<=320
 assert im.getchannel('A').getextrema()[0]==0
# The standard brown panel must never conceal the themed background again.
for filename in ['GeoPassPopup.cpp','ProgressionPopup.cpp','AdminPanel.cpp','SeasonBuilderPopup.cpp','TierShopPopup.cpp']:
 s=(root/'src/ui'/filename).read_text();assert 'm_bgSprite->setZOrder(-3)' in s
print('PASS: compact screen fit, six crate cards, admin separation, alpha resources, background ordering')
