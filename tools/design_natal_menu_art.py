#!/usr/bin/env python3
"""Original Christmas art for the GDPS menus (GeoBR Natal 2026).

natal_snowcap.png        pillowy snow for flat tops (the top arm of the menu
                         crosses, rectangle buttons), with rounded icicles
natal_snowcap_round.png  snow that follows the top of a round button; the
                         circle it fits is centred at (128, 152), radius 112
natal_snow_ground.png    tileable snow bank for the menu ground line
natal_frame.png          64x16 candy-cane stripe (repeat texture) for the frame
natal_spark.png          small soft sparkle for the twinkling snow
Usage: python3 tools/design_natal_menu_art.py <repo>
"""
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

S = 4  # supersampling
SNOW_TOP = (255, 255, 255)
SNOW_LOW = (196, 222, 255)
OUTLINE = (92, 132, 196)


def shade(mask, top, bottom):
    """Fills a mask with a soft vertical white->ice gradient, outline and highlight."""
    W, H = mask.size
    grad = Image.new('RGBA', (W, H))
    d = ImageDraw.Draw(grad)
    box = mask.getbbox() or (0, 0, W, H)
    for y in range(H):
        t = min(1, max(0, (y - box[1]) / max(1, box[3] - box[1])))
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        d.line([(0, y), (W, y)], fill=c + (255,))
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    # Shadow under the snow.
    sh = Image.new('RGBA', (W, H), (16, 32, 80, 0))
    sh.putalpha(ImageChops.offset(mask, 0, 3 * S).point(lambda v: v * 90 // 255))
    img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(2 * S)))
    body = Image.composite(grad, Image.new('RGBA', (W, H), (0, 0, 0, 0)), mask)
    img.alpha_composite(body)
    # Outline = mask minus eroded mask.
    eroded = mask.filter(ImageFilter.MinFilter(2 * S + 1))
    ring = ImageChops.subtract(mask, eroded)
    ol = Image.new('RGBA', (W, H), OUTLINE + (0,))
    ol.putalpha(ring)
    img.alpha_composite(ol)
    # Highlight along the top edge.
    hi_mask = ImageChops.subtract(mask, ImageChops.offset(mask, 0, 3 * S)).filter(ImageFilter.GaussianBlur(S))
    hi = Image.new('RGBA', (W, H), (255, 255, 255, 0))
    hi.putalpha(hi_mask.point(lambda v: min(255, v * 2)))
    img.alpha_composite(hi)
    return img


def drips(d, y, xs, rnd, scale=1.0):
    for x in xs:
        w = rnd.uniform(9, 13) * S * scale
        L = rnd.uniform(9, 20) * S * scale
        d.rounded_rectangle([x - w / 2, y - 6 * S, x + w / 2, y + L], radius=w / 2, fill=255)


def flat_cap(out):
    w, h = 256, 96
    W, H = w * S, h * S
    rnd = random.Random(5)
    mask = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(mask)
    base = 54 * S
    d.rounded_rectangle([6 * S, 26 * S, W - 6 * S, base], radius=12 * S, fill=255)
    # Pillowy top: overlapping bumps of different sizes.
    x = 12 * S
    while x < W - 12 * S:
        r = rnd.uniform(14, 22) * S
        d.ellipse([x - r, 30 * S - r * .9, x + r, 30 * S + r * .9], fill=255)
        x += r * 1.25
    drips(d, base, [W * .22, W * .5, W * .76], rnd)
    shaded = shade(mask, SNOW_TOP, SNOW_LOW)
    sparkle(shaded, mask, 6, 9)
    shaded.resize((w, h), Image.LANCZOS).save(out, optimize=True)


def round_cap(out):
    w, h = 256, 160
    W, H = w * S, h * S
    cx, cy, R = 128 * S, 152 * S, 112 * S
    rnd = random.Random(8)
    mask = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(mask)
    # Bumpy outer edge on the upper arc of the circle.
    for a in range(206, 335, 5):
        t = math.radians(a)
        taper = .35 + .65 * math.sin(math.pi * (a - 206) / 128)  # thin at both ends
        r = rnd.uniform(15, 20) * S * taper
        px, py = cx + math.cos(t) * (R + 4 * S * taper), cy + math.sin(t) * (R + 4 * S * taper)
        d.ellipse([px - r, py - r, px + r, py + r], fill=255)
    # Keep only what sits on top of the button (above the circle's upper part).
    inner = Image.new('L', (W, H), 0)
    ImageDraw.Draw(inner).ellipse([cx - R + 8 * S, cy - R + 8 * S, cx + R - 8 * S, cy + R - 8 * S], fill=255)
    mask = ImageChops.subtract(mask, inner)
    d = ImageDraw.Draw(mask)
    for a in (235, 268, 300):
        t = math.radians(a)
        px, py = cx + math.cos(t) * (R - 4 * S), cy + math.sin(t) * (R - 4 * S)
        drips(d, py, [px], rnd, .9)
    clip = Image.new('L', (W, H), 0)
    ImageDraw.Draw(clip).rectangle([0, 0, W, cy - 22 * S], fill=255)
    mask = ImageChops.multiply(mask, clip)
    shaded = shade(mask, SNOW_TOP, SNOW_LOW)
    sparkle(shaded, mask, 5, 12)
    shaded.resize((w, h), Image.LANCZOS).save(out, optimize=True)


def sparkle(img, mask, n, seed):
    rnd = random.Random(seed)
    W, H = img.size
    d = ImageDraw.Draw(img)
    box = mask.getbbox()
    placed = 0
    while placed < n:
        x, y = rnd.uniform(box[0], box[2]), rnd.uniform(box[1], box[1] + (box[3] - box[1]) * .35)
        if mask.getpixel((int(x), int(y))) < 250: continue
        r = rnd.uniform(1, 1.8) * S
        d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 255, 255, 255))
        placed += 1


def ground(out):
    w, h = 512, 48
    W, H = w * S, h * S
    mask = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(mask)
    pts = [(0, H)]
    for i in range(0, 65):
        x = W * i / 64
        y = (10 + 3 * math.sin(i / 64 * math.pi * 6) + 1.5 * math.sin(i / 64 * math.pi * 14)) * S
        pts.append((x, y))
    pts.append((W, H))
    d.polygon(pts, fill=255)
    shade(mask, SNOW_TOP, SNOW_LOW).resize((w, h), Image.LANCZOS).save(out, optimize=True)


def frame(out):
    img = Image.new('RGBA', (64 * S, 16 * S), (250, 250, 250, 255))
    d = ImageDraw.Draw(img)
    for x in range(-32 * S, 96 * S, 16 * S):  # two stripes per 64 px: tiles seamlessly
        d.polygon([(x, 16 * S), (x + 8 * S, 0), (x + 16 * S, 0), (x + 8 * S, 16 * S)], fill=(214, 30, 46, 255))
    d.rectangle([0, 13 * S, 64 * S, 16 * S], fill=(26, 120, 60, 255))
    img.resize((64, 16), Image.LANCZOS).save(out, optimize=True)


def spark(out):
    W = 32 * S
    img = Image.new('RGBA', (W, W), (255, 255, 255, 0))
    d = ImageDraw.Draw(img)
    c = W / 2
    d.polygon([(c, 2 * S), (c + 3 * S, c - 3 * S), (W - 2 * S, c), (c + 3 * S, c + 3 * S), (c, W - 2 * S), (c - 3 * S, c + 3 * S), (2 * S, c), (c - 3 * S, c - 3 * S)], fill=(255, 255, 255, 255))
    img = img.filter(ImageFilter.GaussianBlur(S * .6))
    img.resize((32, 32), Image.LANCZOS).save(out, optimize=True)


def main(repo):
    res = Path(repo) / 'resources' / 'natal'
    flat_cap(res / 'natal_snowcap.png')
    round_cap(res / 'natal_snowcap_round.png')
    ground(res / 'natal_snow_ground.png')
    frame(res / 'natal_frame.png')
    spark(res / 'natal_spark.png')
    print('ok')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '.')
