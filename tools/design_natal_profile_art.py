#!/usr/bin/env python3
"""Original art for the GeoBR Natal 2026 profile themes and frames.

geobr-theme-natal_noite.png     snowy village at night (moon, stars, lit windows)
geobr-theme-natal_floresta.png  snowy pine forest under a green aurora
geobr-theme-natal_bengala.png   candy-cane stripes with mint swirls
geobr-theme-natal_lareira.png   brick fireplace with fire, stockings and garland
natal_frame_antlers.png         one reindeer antler (mirrored in game for the pair)
Usage: python3 tools/design_natal_profile_art.py <repo>
"""
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

W, H = 1254, 418


def gradient(top, bottom):
    img = Image.new("RGB", (W, H))
    d = ImageDraw.Draw(img)
    for y in range(H):
        t = y / (H - 1)
        d.line([(0, y), (W, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    return img


def snowfall(img, n, seed, alpha=200):
    rnd = random.Random(seed)
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for _ in range(n):
        x, y, r = rnd.uniform(0, W), rnd.uniform(0, H), rnd.choice([1.5, 2, 2.5, 3.5])
        d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 255, 255, rnd.randint(alpha // 2, alpha)))
    img.paste(layer, (0, 0), layer)


def hills(d, base, color, amp, freq, phase):
    pts = [(0, H)]
    for x in range(0, W + 20, 20):
        pts.append((x, base - amp * (0.6 * math.sin(x * freq + phase) + 0.4 * math.sin(x * freq * 2.3 + phase * 1.7))))
    pts.append((W, H))
    d.polygon(pts, fill=color)


def noite(res):
    img = gradient((10, 16, 48), (52, 40, 96))
    d = ImageDraw.Draw(img)
    rnd = random.Random(3)
    for _ in range(160):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H * .6)
        r = rnd.choice([1, 1, 1.5, 2])
        d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 255, 230))
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([1000, 30, 1150, 180], fill=(255, 250, 210, 90))
    img.paste(glow.filter(ImageFilter.GaussianBlur(25)), (0, 0), glow.filter(ImageFilter.GaussianBlur(25)))
    d.ellipse([1035, 60, 1115, 140], fill=(255, 248, 205))
    d.ellipse([1058, 52, 1128, 122], fill=(30, 28, 72))                 # crescent
    hills(d, 300, (70, 86, 140), 30, .006, 1)
    # village
    for i, x in enumerate(range(60, W, 150)):
        w, h = 80 + (i % 3) * 14, 60 + (i % 2) * 20
        y = 320 - h
        d.rectangle([x, y, x + w, 340], fill=(40, 30, 50))
        d.polygon([(x - 10, y), (x + w / 2, y - 45), (x + w + 10, y)], fill=(70, 30, 40))
        d.polygon([(x - 10, y), (x + w / 2, y - 45), (x + w + 10, y), (x + w + 4, y - 6), (x + w / 2, y - 38), (x - 4, y - 6)], fill=(245, 250, 255))
        for wx in (x + 14, x + w - 34):
            d.rectangle([wx, y + 18, wx + 20, y + 40], fill=(255, 205, 90))
            d.line([(wx + 10, y + 18), (wx + 10, y + 40)], fill=(40, 30, 50), width=2)
    hills(d, 350, (232, 240, 255), 18, .009, 2)
    hills(d, 385, (200, 215, 245), 14, .013, 4)
    snowfall(img, 220, 5)
    img.save(res / "geobr-theme-natal_noite.png", optimize=True)


def pine(d, x, base, h, dark, snow):
    w = h * .55
    d.rectangle([x - 6, base - 18, x + 6, base], fill=(70, 45, 30))
    for k in range(4):
        top = base - 18 - h * (k + 1) / 4 - h * .15
        bw = w * (1 - k * .2)
        y0 = base - 18 - h * k / 4
        d.polygon([(x - bw / 2, y0), (x + bw / 2, y0), (x, top)], fill=dark)
        d.polygon([(x - bw * .18, top + h * .1), (x + bw * .18, top + h * .1), (x, top)], fill=snow)


def floresta(res):
    img = gradient((8, 30, 40), (20, 70, 80))
    aur = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ad = ImageDraw.Draw(aur)
    for i in range(6):
        y = 80 + i * 12
        ad.line([(x, y + 40 * math.sin(x * .006 + i)) for x in range(0, W, 10)], fill=(90, 255, 170, 70), width=26)
    aur = aur.filter(ImageFilter.GaussianBlur(14))
    img.paste(aur, (0, 0), aur)
    d = ImageDraw.Draw(img)
    rnd = random.Random(7)
    for _ in range(90):
        x, y = rnd.uniform(0, W), rnd.uniform(0, 160)
        d.ellipse([x - 1, y - 1, x + 1, y + 1], fill=(220, 255, 240))
    for row, (base, h, col) in enumerate(((330, 130, (16, 70, 60)), (370, 160, (12, 92, 62)), (418, 190, (10, 120, 70)))):
        for x in range(-40 + row * 50, W + 80, 120 - row * 10):
            pine(d, x + rnd.uniform(-15, 15), base, h * rnd.uniform(.8, 1.1), col, (240, 248, 255))
        if row == 1: hills(d, 380, (226, 238, 255), 10, .01, 3)
    snowfall(img, 200, 9)
    img.save(res / "geobr-theme-natal_floresta.png", optimize=True)


def bengala(res):
    img = Image.new("RGB", (W, H), (205, 28, 44))
    d = ImageDraw.Draw(img)
    for x in range(-H, W + H, 70):
        d.polygon([(x, H), (x + 34, H), (x + 34 + H, 0), (x + H, 0)], fill=(250, 246, 240))
        d.polygon([(x + 34, H), (x + 44, H), (x + 44 + H, 0), (x + 34 + H, 0)], fill=(80, 190, 120))
    shade = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shade)
    for y in range(H):
        a = int(110 * (y / H) ** 1.6)
        sd.line([(0, y), (W, y)], fill=(90, 0, 20, a))
    img.paste(shade, (0, 0), shade)
    d = ImageDraw.Draw(img)
    rnd = random.Random(2)
    for _ in range(14):   # mint swirls
        cx, cy, r = rnd.uniform(40, W - 40), rnd.uniform(40, H - 40), rnd.uniform(18, 34)
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 255, 255), outline=(150, 20, 35), width=3)
        for k in range(6):
            a0 = k * 60
            d.pieslice([cx - r + 4, cy - r + 4, cx + r - 4, cy + r - 4], a0, a0 + 30, fill=(220, 30, 50))
    snowfall(img, 120, 4, 160)
    img.save(res / "geobr-theme-natal_bengala.png", optimize=True)


def lareira(res, acc):
    img = gradient((78, 32, 26), (40, 16, 14))
    d = ImageDraw.Draw(img)
    for r in range(0, H, 28):  # bricks
        off = 0 if (r // 28) % 2 else 40
        for c in range(-80, W, 80):
            d.rectangle([c + off + 2, r + 2, c + off + 78, r + 26], fill=(122, 50, 38))
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([380, 160, 880, 520], fill=(255, 150, 50, 140))
    glow = glow.filter(ImageFilter.GaussianBlur(50))
    img.paste(glow, (0, 0), glow)
    d = ImageDraw.Draw(img)
    d.rectangle([420, 130, 840, 418], fill=(150, 140, 135))          # fireplace
    d.rectangle([470, 190, 790, 418], fill=(30, 16, 14))
    d.rectangle([400, 110, 860, 140], fill=(110, 70, 45))            # mantel
    for k, (col, sc) in enumerate((((255, 80, 20), 1.0), ((255, 160, 40), .7), ((255, 230, 120), .42))):
        for fx in (560, 630, 700):
            hh = 150 * sc
            d.polygon([(fx - 45 * sc, 418), (fx + 45 * sc, 418), (fx + 10 * sc, 418 - hh * .7), (fx, 418 - hh), (fx - 14 * sc, 418 - hh * .6)], fill=col)
    d.rectangle([520, 395, 740, 418], fill=(90, 55, 35))
    for name, x, y, h in (("meia_natal", 450, 140, 90), ("meia_natal", 560, 140, 90), ("meia_natal", 700, 140, 90),
                          ("guirlanda", 1040, 90, 150), ("sinos_natal", 180, 90, 110), ("presente", 950, 330, 90), ("presente", 1050, 340, 70)):
        im = Image.open(acc / f"natal_acc_{name}.png").convert("RGBA")
        im = im.resize((int(im.width * h / im.height), h), Image.LANCZOS)
        img.paste(im, (int(x - im.width / 2), int(y)), im)
    img.save(res / "geobr-theme-natal_lareira.png", optimize=True)


def antler(res, acc):
    """A reindeer antler: curved main beam with tapering tines pointing up, caramel
    colour with a dark outline, a holly sprig at the base and a red bauble."""
    S = 4
    w, h = 240, 260
    img = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    def bez(p0, p1, p2, n=40):
        return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
                 (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in [i / n for i in range(n + 1)]]

    beam = bez((150, 250), (40, 170), (78, 30))
    tines = []
    for t, (dx, dy), length in ((.30, (1.0, -.9), 70), (.55, (1.0, -1.1), 66), (.78, (.8, -1.2), 50), (.12, (-.9, -.6), 36)):
        i = int(t * (len(beam) - 1))
        x0, y0 = beam[i]
        nrm = math.hypot(dx, dy)
        tip = (x0 + dx / nrm * length, y0 + dy / nrm * length)
        ctrl = (x0 + dx / nrm * length * .3, y0 + dy / nrm * length * .8)
        tines.append((bez((x0, y0), ctrl, tip, 20), 16 * (1 - t * .5)))
    strokes = [(beam, 26)] + tines

    def draw(width_add, color):
        for pts, w0 in strokes:
            n = len(pts)
            for k, (x, y) in enumerate(pts):
                r = (w0 * (1 - .55 * k / max(1, n - 1)) + width_add) * S / 2
                d.ellipse([x * S - r, y * S - r, x * S + r, y * S + r], fill=color)
    draw(7, (84, 46, 20, 255))
    draw(0, (200, 146, 88, 255))
    for pts, w0 in strokes:  # light edge
        for k, (x, y) in enumerate(pts[:-2]):
            r = max(1.5, w0 * .18 * (1 - .5 * k / len(pts))) * S
            d.ellipse([(x - w0 * .18) * S - r, (y - 2) * S - r, (x - w0 * .18) * S + r, (y - 2) * S + r], fill=(236, 196, 140, 255))
    img = img.resize((w, h), Image.LANCZOS)
    holly = Image.open(acc / "natal_acc_azevinho.png").convert("RGBA")
    holly = holly.resize((70, int(holly.height * 70 / holly.width)), Image.LANCZOS)
    img.alpha_composite(holly, (112, 200))
    dr = ImageDraw.Draw(img)
    x, y = tines[1][0][-1]
    dr.line([(x, y), (x, y + 18)], fill=(240, 220, 120, 255), width=2)
    dr.ellipse([x - 9, y + 16, x + 9, y + 34], fill=(220, 30, 45, 255), outline=(120, 10, 20, 255), width=2)
    dr.ellipse([x - 5, y + 19, x - 1, y + 23], fill=(255, 200, 200, 255))
    img.save(res / "natal" / "natal_frame_antlers.png", optimize=True)


def main(repo):
    res = Path(repo) / "resources"
    noite(res); floresta(res); bengala(res); lareira(res, res / "natal"); antler(res, res / "natal")
    print("ok")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else ".")
