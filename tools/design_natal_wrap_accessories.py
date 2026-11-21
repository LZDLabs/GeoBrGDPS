# New neck accessories that wrap the icon: scarf band around the lower part of
# the cube (knot + hanging tail) and Christmas lights around the middle.
# Drawn at 4x and reduced (same cartoon look: dark outline, flat colours, highlight).
import math, sys
from PIL import Image, ImageDraw, ImageFilter
OUT = sys.argv[1]
K = 4
OUTLINE = (38, 22, 34, 255)

def band_mask(w, h, x0, x1, ytop, ybot, sag):
    m = Image.new('L', (w, h), 0); d = ImageDraw.Draw(m)
    pts_top = [(x, ytop + sag * math.sin(math.pi * (x - x0) / (x1 - x0))) for x in range(x0, x1 + 1, 4)]
    pts_bot = [(x, ybot + sag * math.sin(math.pi * (x - x0) / (x1 - x0))) for x in range(x1, x0 - 1, -4)]
    d.polygon(pts_top + pts_bot, fill=255)
    return m

def scarf(name, pattern):
    W, H = 256 * K, 168 * K
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    # pattern texture
    tex = Image.new('RGBA', (W, H)); td = ImageDraw.Draw(tex)
    if pattern == 'listrado':
        td.rectangle((0, 0, W, H), fill=(225, 40, 50))
        for i in range(-H, W + H, 36 * K):
            td.polygon([(i, 0), (i + 16 * K, 0), (i + 16 * K - H, H), (i - H, H)], fill=(250, 245, 240))
    else:
        td.rectangle((0, 0, W, H), fill=(40, 140, 60))
        for i in range(0, W, 40 * K):
            td.rectangle((i, 0, i + 18 * K, H), fill=(200, 35, 45, 255))
        for j in range(0, H, 40 * K):
            td.rectangle((0, j, W, j + 14 * K), fill=(150, 30, 40, 255))
        for i in range(0, W, 40 * K):
            td.line((i + 9 * K, 0, i + 9 * K, H), fill=(245, 205, 60), width=2 * K)
    # band: wraps the cube, darker at both ends (it goes round the back)
    band = band_mask(W, H, 6 * K, 250 * K, 30 * K, 96 * K, 10 * K)
    # tail hanging from the knot (front, right side)
    tail = Image.new('L', (W, H), 0); dt = ImageDraw.Draw(tail)
    dt.polygon([(150 * K, 80 * K), (196 * K, 80 * K), (202 * K, 156 * K), (158 * K, 160 * K)], fill=255)
    knot = Image.new('L', (W, H), 0); dk = ImageDraw.Draw(knot)
    dk.ellipse((150 * K, 46 * K, 202 * K, 102 * K), fill=255)
    from PIL import ImageEnhance
    for mask, shade in ((tail, .85), (band, 1.0), (knot, .8)):
        layer = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        t = tex if shade == 1.0 else ImageEnhance.Brightness(tex).enhance(shade)
        layer.paste(t, (0, 0), mask)
        edge = mask
        for _ in range(6): edge = edge.filter(ImageFilter.MaxFilter(9))
        img.paste(Image.new('RGBA', (W, H), OUTLINE), (0, 0), edge)
        img.alpha_composite(layer)
    # shading: ends of the band darker (wrapping round), highlight on top
    sh = Image.new('RGBA', (W, H), (0, 0, 0, 0)); ds = ImageDraw.Draw(sh)
    for x in range(0, 46 * K):
        a = int(150 * (1 - x / (46 * K)))
        ds.line((6 * K + x, 0, 6 * K + x, H), fill=(0, 0, 0, a))
        ds.line((250 * K - x, 0, 250 * K - x, H), fill=(0, 0, 0, a))
    shm = Image.new('L', (W, H), 0); shm.paste(band, (0, 0))
    img.paste(Image.alpha_composite(img, sh), (0, 0), shm)
    hl = Image.new('RGBA', (W, H), (0, 0, 0, 0)); dh = ImageDraw.Draw(hl)
    for x in range(30 * K, 226 * K, 2):
        y = 38 * K + 10 * K * math.sin(math.pi * (x - 6 * K) / (244 * K))
        dh.line((x, y, x, y + 7 * K), fill=(255, 255, 255, 70))
    img.alpha_composite(hl)
    # fringe at the end of the tail
    df = ImageDraw.Draw(img)
    for i in range(6):
        x = 161 * K + i * 7 * K
        df.line((x, 157 * K, x + 2 * K, 166 * K), fill=OUTLINE, width=4 * K)
    img = img.resize((128, 84), Image.LANCZOS)
    img.save(f'{OUT}/natal_acc_{name}.png')

def lights():
    W, H = 256 * K, 110 * K
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(img)
    def wire_y(x): return 30 * K + 16 * K * math.sin(math.pi * (x - 4 * K) / (248 * K))
    pts = [(x, wire_y(x)) for x in range(4 * K, 252 * K, 4)]
    d.line(pts, fill=OUTLINE, width=12 * K // 2, joint='curve')
    d.line(pts, fill=(40, 110, 50), width=5 * K // 2, joint='curve')
    colors = [(235, 50, 50), (60, 200, 80), (60, 140, 255), (250, 200, 40), (235, 50, 50), (60, 200, 80), (60, 140, 255)]
    for i, c in enumerate(colors):
        x = 26 * K + i * 34 * K; y = wire_y(x)
        # socket
        d.rounded_rectangle((x - 6 * K, y - 2 * K, x + 6 * K, y + 10 * K), radius=2 * K, fill=(60, 70, 60), outline=OUTLINE, width=2 * K)
        # bulb (teardrop)
        bx, by = x, y + 30 * K
        d.ellipse((bx - 13 * K, by - 18 * K, bx + 13 * K, by + 14 * K), fill=c, outline=OUTLINE, width=3 * K)
        d.polygon([(bx - 9 * K, by - 10 * K), (bx + 9 * K, by - 10 * K), (bx, y + 8 * K)], fill=c)
        d.ellipse((bx - 6 * K, by - 10 * K, bx - 1 * K, by - 2 * K), fill=(255, 255, 255, 170))
    # glow
    glow = img.filter(ImageFilter.GaussianBlur(6 * K))
    out = Image.alpha_composite(Image.new('RGBA', img.size, (0, 0, 0, 0)), glow.point(lambda v: v // 3) if False else glow)
    out.alpha_composite(img)
    out.resize((128, 55), Image.LANCZOS).save(f'{OUT}/natal_acc_pisca_pisca.png')

scarf('cachecol_listrado', 'listrado')
scarf('cachecol_xadrez', 'xadrez')
lights()
