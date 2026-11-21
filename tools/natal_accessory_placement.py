# Placement of every Natal accessory from its real art (visible box), in cube units
# (cube = 1, centre 0,0, top edge +0.5). Writes SQL and a proof sheet.
import glob, os, json, sys
from PIL import Image, ImageDraw, ImageFont
RES = '/home/user/continua-o-Radio/resources/natal'
NEW = RES  # art already in resources
def art(id):
    p = f'{NEW}/natal_acc_{id}.png'
    return Image.open(p if os.path.exists(p) else f'{RES}/natal_acc_{id}.png').convert('RGBA')
# (anchor, width, a, b, extra): bottom: visible bottom at 0.5 - a, x shift b
#                                top: visible top at a, x shift b; center: centre at (b, a)
P = {}
for c in ['coroa_abismo','coroa_bengala','coroa_doce','coroa_dourada','coroa_espinhos','coroa_gelo','coroa_gelo_real','coroa_infernal','coroa_rosa','coroa_roxa','coroa_sombria']:
    P[c] = ('bottom', .98, .10, 0)
P.update({
 'cartola_azevinho': ('bottom', 1.0, .14, 0), 'cartola_floco': ('bottom', 1.0, .14, 0),
 'gorro_papai_noel': ('bottom', 1.22, .30, .07), 'gorro_elfo': ('bottom', 1.22, .28, .05),
 'chifres_diabo': ('bottom', 1.08, .14, 0), 'tiara_rena': ('bottom', 1.35, .34, 0),
 'protetor_orelha': ('bottom', 1.55, .66, 0), 'louros_dourados': ('bottom', 1.3, .34, 0),
 'laco_vermelho': ('bottom', .72, .08, .2), 'laco_xadrez': ('bottom', .72, .08, .2), 'laco_sombrio': ('bottom', .72, .08, .2),
 'azevinho': ('bottom', .62, .10, -.2),
 'aureola': ('bottom', .9, -.14, 0),
 'barba_noel': ('top', 1.0, -.08, 0),
 'cachecol_listrado': ('top', 1.16, -.1, 0), 'cachecol_xadrez': ('top', 1.16, -.1, 0),
 'pisca_pisca': ('center', 1.3, -.1, 0),
 'gravata_borboleta': ('center', .56, -.36, 0), 'guirlanda': ('center', .7, -.4, 0),
 'sino_dourado': ('top', .42, -.16, 0), 'sinos_natal': ('top', .64, -.18, 0),
 'asas_anjo': ('center', 2.0, .1, 0), 'asas_morcego': ('center', 2.0, .1, 0),
})
for f in ['anel_saturno','estrela_cadente','estrela_dourada','floco_neve','planeta_roxo']: P[f] = ('center', .52, .66, .66)
for f in ['bengala_doce','boneco_gengibre','boneco_neve','meia_natal','presente','rena']: P[f] = ('hcenter', .78, -.12, -.82)
rows = {}
for id, (anchor, W, a, b) in P.items():
    im = art(id); w, h = im.size; longest = max(w, h)
    x0, y0, x1, y1 = im.getchannel('A').point(lambda v: 255 if v > 40 else 0).getbbox()
    if anchor == 'hcenter':   # size by height (side props)
        s = W * longest / (y1 - y0)
    else:
        s = W * longest / (x1 - x0)
    u = s / longest
    offx = -((x0 + x1) / 2 - w / 2) * u + (b if anchor in ('bottom', 'top') else 0)
    if anchor == 'bottom': offy = .5 - a + (y1 - h / 2) * u
    elif anchor == 'top': offy = a - (h / 2 - y0) * u
    else:
        offy = a - ((y0 + y1) / 2 - h / 2) * -u
        offy = a + ((y0 + y1) / 2 - h / 2) * u
        offx += b
    rows[id] = (round(s, 3), round(offx, 3), round(offy, 3))
json.dump(rows, open('placement.json', 'w'), indent=1)
# proof sheet: GD-like cube (30 units -> 4 px per unit)
U = 120; cell = 300
ids = sorted(rows); cols = 9
sheet = Image.new('RGBA', (cols * cell, ((len(ids) + cols - 1) // cols) * cell), (95, 95, 110, 255))
d = ImageDraw.Draw(sheet); f = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 18)
behind = {'asas_anjo', 'asas_morcego'}
for i, id in enumerate(ids):
    cx = (i % cols) * cell + cell // 2; cy = (i // cols) * cell + cell // 2 + 10
    s, ox, oy = rows[id]; im = art(id); longest = max(im.size)
    k = U * s / longest; spr = im.resize((max(1, int(im.width * k)), max(1, int(im.height * k))), Image.LANCZOS)
    px, py = cx + ox * U - spr.width / 2, cy - oy * U - spr.height / 2
    if id in behind: sheet.alpha_composite(spr, (int(px), int(py)))
    d.rectangle((cx - U / 2, cy - U / 2, cx + U / 2, cy + U / 2), fill=(120, 255, 0), outline=(0, 0, 0), width=6)
    d.rectangle((cx - U / 5, cy - U / 5, cx + U / 5, cy + U / 5), fill=(0, 255, 255), outline=(0, 0, 0), width=6)
    if id not in behind: sheet.alpha_composite(spr, (int(px), int(py)))
    d.text((cx, (i // cols) * cell + 14), id, font=f, fill='white', anchor='mm')
sheet.convert('RGB').save('proof.jpg', quality=88)
print(len(rows))
