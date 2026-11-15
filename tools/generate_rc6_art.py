#!/usr/bin/env python3
"""Generates the RC6 artwork from vector geometry (requires Pillow + numpy).

Everything here is drawn procedurally so the sprites can be regenerated and
reviewed. Output follows Geometry Dash icon conventions:

* tintable layers are white/grey with baked black outlines (multiplying by
  the player colour keeps the outline black),
* `*-primary.png` is tinted with Col 1, `*-secondary.png` with Col 2,
  `*-glow.png` with the glow colour, `*-detail.png` is never tinted.

Run from the repository root:  python3 tools/generate_rc6_art.py
"""
from __future__ import annotations

import math
import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "resources"
SS = 4  # supersampling factor


# ----------------------------------------------------------------- helpers
def canvas(w: int, h: int, mode: str = "L", fill=0) -> Image.Image:
    return Image.new(mode, (w * SS, h * SS), fill)


def down(img: Image.Image, w: int, h: int) -> Image.Image:
    return img.resize((w, h), Image.LANCZOS)


def poly(mask: Image.Image, points, fill=255) -> None:
    ImageDraw.Draw(mask).polygon([(x * SS, y * SS) for x, y in points], fill=fill)


def dilate(mask: Image.Image, radius_px: int) -> Image.Image:
    size = radius_px * 2 * SS + 1
    return mask.filter(ImageFilter.MaxFilter(size if size % 2 else size + 1))


def compose_layer(body: Image.Image, outline_px: int, shade: bool = True) -> Image.Image:
    """White body with black outline baked in, returned as RGBA (supersampled)."""
    outer = dilate(body, outline_px)
    w, h = body.size
    rgba = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    black = Image.new("RGBA", (w, h), (0, 0, 0, 255))
    rgba.paste(black, (0, 0), outer)
    if shade:
        # Vertical shading: top 100%, bottom ~78% so tints keep GD-like depth.
        grad = np.linspace(255, 200, h, dtype=np.float32)[:, None].repeat(w, axis=1)
        gray = Image.fromarray(grad.astype(np.uint8), "L")
        white = Image.merge("RGBA", (gray, gray, gray, Image.new("L", (w, h), 255)))
    else:
        white = Image.new("RGBA", (w, h), (255, 255, 255, 255))
    rgba.paste(white, (0, 0), body)
    return rgba


def save(img: Image.Image, rel: str, w: int, h: int) -> None:
    path = OUT / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    down(img, w, h).save(path, optimize=True)
    print("wrote", path.relative_to(ROOT))


# ------------------------------------------------------------ arrow skins
# Coordinates are in a 120x120 box, arrow pointing to +x (right), centre 60,60.
SIZE = 120

SKINS = {
    # 1 Flecha: broad arrowhead with a notched tail.
    1: {
        "body": [(104, 60), (52, 22), (52, 44), (16, 44), (26, 60), (16, 76), (52, 76), (52, 98)],
        "accent": [[(88, 60), (60, 38), (60, 50), (30, 50), (35, 60), (30, 70), (60, 70), (60, 82)]],
        "shine": [(58, 30), (92, 56), (86, 56), (58, 36)],
    },
    # 2 Dardo: long kite with a diamond core.
    2: {
        "body": [(108, 60), (60, 30), (14, 50), (24, 60), (14, 70), (60, 90)],
        "accent": [[(84, 60), (60, 46), (40, 60), (60, 74)]],
        "shine": [(62, 36), (96, 57), (90, 57), (62, 42)],
    },
    # 3 Pena: slim shaft, sharp head and two fletching fins.
    3: {
        "body": [(110, 60), (78, 34), (78, 52), (40, 52), (22, 30), (10, 30), (24, 60), (10, 90), (22, 90), (40, 68), (78, 68), (78, 86)],
        "accent": [[(36, 52), (22, 34), (14, 34), (26, 56)], [(36, 68), (22, 86), (14, 86), (26, 64)], [(96, 60), (82, 48), (82, 72)]],
        "shine": [(80, 40), (100, 57), (95, 57), (80, 45)],
    },
    # 4 Cometa: double chevron with a split tail.
    4: {
        "body": [(110, 60), (66, 24), (70, 46), (44, 26), (12, 26), (40, 60), (12, 94), (44, 94), (70, 74), (66, 96)],
        "accent": [[(92, 60), (74, 44), (76, 56), (52, 40), (32, 40), (52, 60), (32, 80), (52, 80), (76, 64), (74, 76)]],
        "shine": [(70, 30), (100, 56), (94, 56), (72, 36)],
    },

    # RC7: shop (5-8), pass (9-10) and crate exclusives (11-16, Comum..Mitica).
    5: {'body': [(110, 60), (72, 26), (76, 48), (36, 30), (14, 44), (50, 60), (14, 76), (36, 90), (76, 72), (72, 94)], 'accent': [[(92, 60), (74, 44), (76, 56), (46, 50), (58, 60), (46, 70), (76, 64), (74, 76)]], 'shine': [(74, 32), (100, 56), (94, 56), (76, 38)]},
    6: {'body': [(112, 60), (96, 52), (96, 40), (110, 34), (84, 30), (84, 52), (52, 52), (52, 40), (18, 40), (26, 60), (18, 80), (52, 80), (52, 68), (84, 68), (84, 90), (110, 86), (96, 80), (96, 68)], 'accent': [[(100, 60), (84, 55), (32, 55), (37, 60), (32, 65), (84, 65)]], 'shine': [(86, 33), (104, 36), (98, 38), (88, 37)]},
    7: {'body': [(112, 60), (96, 46), (56, 44), (40, 28), (30, 28), (38, 46), (22, 52), (22, 68), (38, 74), (30, 92), (40, 92), (56, 76), (96, 74)], 'accent': [[(112, 60), (96, 46), (92, 60), (96, 74)], [(82.3, 63.4), (77.4, 68.3), (70.6, 68.3), (65.7, 63.4), (65.7, 56.6), (70.6, 51.7), (77.4, 51.7), (82.3, 56.6)]], 'shine': [(60, 47), (92, 49), (88, 52), (60, 51)]},
    8: {'body': [(112, 58), (16, 26), (40, 62), (16, 98)], 'accent': [[(112, 58), (40, 62), (56, 76)]], 'shine': [(30, 32), (100, 56), (94, 56), (34, 37)]},
    9: {'body': [(108, 60), (64, 22), (64, 42), (14, 42), (26, 60), (14, 78), (64, 78), (64, 98)], 'accent': [[(84, 60), (60, 44), (36, 60), (60, 76)]], 'shine': [(68, 28), (98, 56), (92, 56), (68, 34)]},
    10: {'body': [(110, 60), (70, 30), (70, 44), (48, 44), (40, 30), (32, 44), (22, 30), (16, 48), (16, 72), (22, 90), (32, 76), (40, 90), (48, 76), (70, 76), (70, 90)], 'accent': [[(70, 53), (18, 53), (18, 67), (70, 67)], [(98, 60), (76, 46), (76, 74)]], 'shine': [(72, 35), (100, 57), (94, 57), (72, 40)]},
    11: {'body': [(108, 60), (68, 26), (68, 46), (14, 46), (14, 74), (68, 74), (68, 94)], 'accent': [[(64, 50), (18, 50), (18, 56), (64, 56)], [(64, 64), (18, 64), (18, 70), (64, 70)]], 'shine': [(72, 32), (98, 56), (92, 56), (72, 38)]},
    12: {'body': [(110, 60), (86, 36), (40, 36), (16, 60), (40, 84), (86, 84)], 'accent': [[(94, 60), (78, 46), (48, 46), (32, 60), (48, 74), (78, 74)]], 'shine': [(44, 40), (84, 40), (80, 44), (46, 44)]},
    13: {'body': [(112, 60), (56, 22), (12, 60), (56, 98)], 'accent': [[(112, 60), (56, 22), (56, 60)], [(56, 60), (12, 60), (56, 98)]], 'shine': [(60, 30), (100, 56), (94, 56), (60, 36)]},
    14: {'body': [(112, 60), (84, 42), (70, 24), (56, 40), (40, 30), (30, 50), (12, 60), (30, 70), (40, 90), (56, 80), (70, 96), (84, 78)], 'accent': [[(100, 60), (74, 48), (52, 60), (74, 72)]], 'shine': [(70, 30), (80, 44), (76, 46), (68, 36)]},
    15: {'body': [(112, 60), (90, 48), (92, 30), (76, 40), (60, 24), (54, 44), (30, 36), (38, 56), (14, 60), (38, 64), (30, 84), (54, 76), (60, 96), (76, 80), (92, 90), (90, 72)], 'accent': [[(78.8, 66.1), (70.1, 74.8), (57.9, 74.8), (49.2, 66.1), (49.2, 53.9), (57.9, 45.2), (70.1, 45.2), (78.8, 53.9)]], 'shine': [(62, 28), (74, 40), (70, 42), (62, 34)]},
    16: {'body': [(114, 60), (80, 52), (84, 30), (66, 46), (56, 20), (48, 46), (26, 32), (36, 54), (10, 60), (36, 66), (26, 88), (48, 74), (56, 100), (66, 74), (84, 90), (80, 68)], 'accent': [[(96, 60), (70, 56), (56, 40), (52, 58), (34, 60), (52, 62), (56, 80), (70, 64)]], 'shine': [(56, 26), (64, 44), (60, 46), (55, 32)]},
}


def arrow_skin(index: int, spec: dict) -> None:
    body = canvas(SIZE, SIZE)
    poly(body, spec["body"])
    accent = canvas(SIZE, SIZE)
    for shape in spec["accent"]:
        poly(accent, shape)
    accent = ImageChops.multiply(accent, body)

    # Primary (Col 1): body minus the accent area, with black outline.
    primary_body = ImageChops.subtract(body, accent)
    primary = compose_layer(primary_body, 3)
    # Accent edges get a thin black line so Col 2 reads clearly.
    edge = ImageChops.subtract(dilate(accent, 1), accent)
    primary.paste(Image.new("RGBA", body.size, (0, 0, 0, 255)), (0, 0), edge)

    secondary = Image.new("RGBA", body.size, (0, 0, 0, 0))
    secondary.paste(Image.new("RGBA", body.size, (255, 255, 255, 255)), (0, 0), accent)

    glow_mask = dilate(body, 7).filter(ImageFilter.GaussianBlur(4 * SS))
    glow = Image.merge("RGBA", (Image.new("L", body.size, 255),) * 3 + (glow_mask,))

    detail = Image.new("RGBA", body.size, (0, 0, 0, 0))
    shine = canvas(SIZE, SIZE)
    poly(shine, spec["shine"])
    shine = ImageChops.multiply(shine, primary_body)
    detail.paste(Image.new("RGBA", body.size, (255, 255, 255, 235)), (0, 0), shine)

    for name, img in (("primary", primary), ("secondary", secondary), ("glow", glow), ("detail", detail)):
        save(img, f"arrow/geobr-arrow-{index}-{name}.png", SIZE, SIZE)


# ------------------------------------------------------------ brown portal
PW, PH = 68, 172  # 4x the 17x43 half-unit footprint of a GD gamemode portal

BROWN_DARK = (74, 41, 18)
BROWN = (139, 87, 42)
BROWN_LIGHT = (205, 142, 78)
AMBER = (255, 196, 102)


def portal_layers() -> None:
    w, h = PW * SS, PH * SS
    cx, cy = w / 2, h / 2
    outer = Image.new("L", (w, h), 0)
    inner = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(outer)
    d.ellipse([cx - 28 * SS, cy - 82 * SS, cx + 28 * SS, cy + 82 * SS], fill=255)
    ImageDraw.Draw(inner).ellipse([cx - 10 * SS, cy - 58 * SS, cx + 10 * SS, cy + 58 * SS], fill=255)
    ring = ImageChops.subtract(outer, inner)
    # Angular GD-style teeth on both rims.
    teeth = Image.new("L", (w, h), 0)
    for k in range(-3, 4):
        y = cy + k * 21 * SS
        span = 82 * SS * math.sqrt(max(0.0, 1 - (k * 21 / 82) ** 2))
        if span <= 0:
            continue
        ImageDraw.Draw(teeth).polygon(
            [(cx - 24 * SS, y - 8 * SS), (cx - 33 * SS, y), (cx - 24 * SS, y + 8 * SS)], fill=255
        )
    ring = ImageChops.lighter(ring, teeth)

    def paint(mask: Image.Image, base, light) -> Image.Image:
        rgba = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        rgba.paste(Image.new("RGBA", (w, h), (0, 0, 0, 255)), (0, 0), dilate(mask, 2))
        grad = Image.linear_gradient("L").resize((w, h)).transpose(Image.FLIP_TOP_BOTTOM)
        col = Image.composite(Image.new("RGBA", (w, h), light + (255,)), Image.new("RGBA", (w, h), base + (255,)), grad)
        rgba.paste(col, (0, 0), mask)
        return rgba

    back = paint(ring, BROWN_DARK, BROWN)
    # Inner amber energy line and arrow emblem on the back layer.
    line = Image.new("L", (w, h), 0)
    ImageDraw.Draw(line).ellipse([cx - 16 * SS, cy - 68 * SS, cx + 16 * SS, cy + 68 * SS], outline=255, width=4 * SS)
    back.paste(Image.new("RGBA", (w, h), AMBER + (210,)), (0, 0), line)

    half = Image.new("L", (w, h), 0)
    ImageDraw.Draw(half).rectangle([0, 0, cx, h], fill=255)
    front_mask = ImageChops.multiply(ring, half)
    front = paint(front_mask, BROWN, BROWN_LIGHT)
    emblem = Image.new("L", (w, h), 0)
    for oy in (-40, 0, 40):
        ex, ey = cx - 19 * SS, cy + oy * SS * math.sqrt(1 - (oy / 82) ** 2) / max(1e-6, math.sqrt(1 - (oy / 82) ** 2))
        ImageDraw.Draw(emblem).polygon(
            [(ex + 7 * SS, ey), (ex - 2 * SS, ey - 8 * SS), (ex - 2 * SS, ey - 3 * SS), (ex - 7 * SS, ey - 3 * SS),
             (ex - 7 * SS, ey + 3 * SS), (ex - 2 * SS, ey + 3 * SS), (ex - 2 * SS, ey + 8 * SS)], fill=255)
    front.paste(Image.new("RGBA", (w, h), (0, 0, 0, 255)), (0, 0), dilate(emblem, 1))
    front.paste(Image.new("RGBA", (w, h), (255, 240, 214, 255)), (0, 0), emblem)
    hl = Image.new("L", (w, h), 0)
    ImageDraw.Draw(hl).arc([cx - 25 * SS, cy - 78 * SS, cx + 25 * SS, cy + 78 * SS], 150, 210, fill=255, width=2 * SS)
    front.paste(Image.new("RGBA", (w, h), (255, 255, 255, 200)), (0, 0), hl)

    save(back, "arrow/geobr-arrow-portal-back.png", PW, PH)
    save(front, "arrow/geobr-arrow-portal-front.png", PW, PH)


def small_effects() -> None:
    spark = canvas(16, 16)
    poly(spark, [(8, 0), (16, 8), (8, 16), (0, 8)])
    rgba = Image.merge("RGBA", (Image.new("L", spark.size, 255),) * 3 + (spark,))
    save(rgba, "arrow/geobr-arrow-spark.png", 16, 16)
    trail = Image.new("L", (64 * SS, 16 * SS), 0)
    alpha = np.tile(np.concatenate([np.linspace(0, 255, 8 * SS), np.linspace(255, 0, 8 * SS)])[:, None], (1, 64 * SS))
    trail_img = Image.merge("RGBA", (Image.new("L", trail.size, 255),) * 3 + (Image.fromarray(alpha.astype(np.uint8)),))
    save(trail_img, "arrow/geobr-arrow-trail.png", 64, 16)


def god_editor_icon() -> None:
    """Pause-menu button: a GD block with a hammer (God Editor)."""
    size = 120
    blockm = canvas(size, size)
    poly(blockm, [(14, 30), (78, 30), (78, 94), (14, 94)])
    img = compose_layer(blockm, 4, shade=True)
    r, g, b, a = img.split()
    rgb = ImageChops.multiply(Image.merge("RGB", (r, g, b)), Image.new("RGB", img.size, (60, 180, 255)))
    colored = Image.merge("RGBA", (*rgb.split(), a))
    inner = canvas(size, size)
    poly(inner, [(24, 40), (68, 40), (68, 84), (24, 84)])
    edge = ImageChops.subtract(dilate(inner, 2), inner)
    colored.paste(Image.new("RGBA", img.size, (255, 255, 255, 230)), (0, 0), edge)
    handle = canvas(size, size)
    poly(handle, [(56, 104), (66, 112), (104, 58), (94, 50)])
    head = canvas(size, size)
    poly(head, [(78, 30), (100, 14), (118, 38), (96, 54)])
    for mask, col in ((handle, (150, 92, 40)), (head, (170, 175, 190))):
        colored.paste(Image.new("RGBA", img.size, (0, 0, 0, 255)), (0, 0), dilate(mask, 3))
        colored.paste(Image.new("RGBA", img.size, col + (255,)), (0, 0), mask)
    shine = canvas(size, size)
    poly(shine, [(84, 30), (98, 20), (102, 25), (88, 35)])
    colored.paste(Image.new("RGBA", img.size, (255, 255, 255, 220)), (0, 0), shine)
    save(colored, "geobr-god-editor.png", size, size)


def collab_icon() -> None:
    """"My Collabs" button: two GD cubes side by side and a hammer (RC9)."""
    size = 120
    out = Image.new("RGBA", (size * SS, size * SS), (0, 0, 0, 0))
    for (x0, y0, w, colour, eye) in ((8, 46, 50, (60, 200, 255), (255, 255, 255)), (46, 32, 56, (255, 210, 60), (255, 255, 255))):
        body = canvas(size, size)
        poly(body, [(x0, y0), (x0 + w, y0), (x0 + w, y0 + w), (x0, y0 + w)])
        out.paste(Image.new("RGBA", out.size, (0, 0, 0, 255)), (0, 0), dilate(body, 4))
        out.paste(Image.new("RGBA", out.size, colour + (255,)), (0, 0), body)
        inner = canvas(size, size)
        m = w * .26
        poly(inner, [(x0 + m, y0 + m), (x0 + w - m, y0 + m), (x0 + w - m, y0 + w - m), (x0 + m, y0 + w - m)])
        out.paste(Image.new("RGBA", out.size, (0, 0, 0, 255)), (0, 0), ImageChops.subtract(dilate(inner, 3), inner))
        # GD cube face: two eyes and a mouth line
        for ex in (x0 + w * .36, x0 + w * .64):
            eyem = canvas(size, size)
            poly(eyem, [(ex - w * .06, y0 + w * .38), (ex + w * .06, y0 + w * .38), (ex + w * .06, y0 + w * .5), (ex - w * .06, y0 + w * .5)])
            out.paste(Image.new("RGBA", out.size, (0, 0, 0, 255)), (0, 0), eyem)
        mouth = canvas(size, size)
        poly(mouth, [(x0 + w * .32, y0 + w * .6), (x0 + w * .68, y0 + w * .6), (x0 + w * .68, y0 + w * .66), (x0 + w * .32, y0 + w * .66)])
        out.paste(Image.new("RGBA", out.size, (0, 0, 0, 255)), (0, 0), mouth)
    handle = canvas(size, size)
    poly(handle, [(66, 112), (76, 118), (112, 64), (102, 58)])
    head = canvas(size, size)
    poly(head, [(86, 40), (106, 26), (120, 48), (100, 62)])
    for mask, col in ((handle, (150, 92, 40)), (head, (175, 180, 195))):
        out.paste(Image.new("RGBA", out.size, (0, 0, 0, 255)), (0, 0), dilate(mask, 3))
        out.paste(Image.new("RGBA", out.size, col + (255,)), (0, 0), mask)
    shine = canvas(size, size)
    poly(shine, [(92, 40), (104, 31), (108, 36), (96, 45)])
    out.paste(Image.new("RGBA", out.size, (255, 255, 255, 220)), (0, 0), shine)
    save(out, "geobr-collab-icon.png", size, size)


# -------------------------------------------------------------- theme art
TW, TH = 1254, 418
TS = 2  # supersampling for the banners


def vgrad(top, bottom):
    t = np.linspace(0, 1, TH * TS)[:, None, None]
    arr = (np.array(top)[None, None, :] * (1 - t) + np.array(bottom)[None, None, :] * t)
    arr = np.repeat(arr, TW * TS, axis=1)
    return Image.fromarray(arr.astype(np.uint8), "RGB")


def layer(img: Image.Image, shapes, color, alpha=255, blur=0.0):
    mask = Image.new("L", img.size, 0)
    dr = ImageDraw.Draw(mask)
    for s in shapes:
        kind = s[0]
        if kind == "poly":
            dr.polygon([(x * TS, y * TS) for x, y in s[1]], fill=alpha)
        elif kind == "ellipse":
            x0, y0, x1, y1 = s[1]
            dr.ellipse([x0 * TS, y0 * TS, x1 * TS, y1 * TS], fill=alpha)
        elif kind == "line":
            dr.line([(x * TS, y * TS) for x, y in s[1]], fill=alpha, width=int(s[2] * TS))
    if blur:
        mask = mask.filter(ImageFilter.GaussianBlur(blur * TS))
    img.paste(Image.new("RGB", img.size, color), (0, 0), mask)


def mirrored(shapes):
    out = []
    for s in shapes:
        if s[0] == "poly":
            out.append(("poly", [(TW - x, y) for x, y in s[1]]))
        elif s[0] == "ellipse":
            x0, y0, x1, y1 = s[1]
            out.append(("ellipse", (TW - x1, y0, TW - x0, y1)))
        elif s[0] == "line":
            out.append(("line", [(TW - x, y) for x, y in s[1]], s[2]))
    return out


def both(shapes):
    return shapes + mirrored(shapes)


def sparkles(img, rng, color, count, region=(0, TW, 0, TH), size=(2, 5), alpha=230):
    shapes = []
    for _ in range(count):
        x = rng.uniform(*region[:2])
        if 380 < x < 874 and rng.random() < 0.75:
            continue  # keep the centre calm, as in the approved banners
        y = rng.uniform(*region[2:])
        r = rng.uniform(*size)
        shapes.append(("poly", [(x, y - r * 2), (x + r * 0.6, y - r * 0.6), (x + r * 2, y), (x + r * 0.6, y + r * 0.6),
                                (x, y + r * 2), (x - r * 0.6, y + r * 0.6), (x - r * 2, y), (x - r * 0.6, y - r * 0.6)]))
    layer(img, shapes, color, alpha)


def mountains(rng, base_y, peaks, width, height, start=0):
    pts = [(start, TH)]
    x = start
    for _ in range(peaks):
        x += width * rng.uniform(0.6, 1.0)
        pts.append((x - width / 2, base_y - height * rng.uniform(0.55, 1.0)))
        pts.append((x, base_y - height * rng.uniform(0.05, 0.25)))
    pts.append((x, TH))
    return ("poly", pts)


def theme_ice(rng):
    img = vgrad((14, 64, 112), (8, 26, 66))
    layer(img, both([mountains(rng, 360, 6, 70, 230)]), (64, 160, 214))
    shards = []
    for i in range(9):
        x = 20 + i * 42
        h = rng.uniform(110, 230)
        shards.append(("poly", [(x - 16, TH), (x - 4, TH - h), (x + 6, TH - h * 0.92), (x + 18, TH)]))
    layer(img, both(shards), (150, 226, 255))
    layer(img, both([("poly", [(x - 3, TH - 6), (x + 2, TH - rng.uniform(80, 180)), (x + 6, TH - 6)]) for x in range(30, 400, 42)]), (232, 251, 255), 200)
    sparkles(img, rng, (220, 248, 255), 70, size=(1.5, 4))
    return img


def theme_forest(rng):
    img = vgrad((18, 72, 52), (6, 28, 26))
    for depth, (col, scale) in enumerate([((20, 92, 60), 1.0), ((12, 64, 42), 1.25), ((6, 40, 28), 1.5)]):
        trees = []
        for i in range(10 - depth * 2):
            x = 14 + i * (46 + depth * 10) + rng.uniform(-6, 6)
            h = rng.uniform(150, 230) * scale * 0.7
            base = TH + 4
            trees.append(("poly", [(x, base - h), (x - 34 * scale * 0.6, base - h * 0.35), (x - 14, base - h * 0.4),
                                   (x - 40 * scale * 0.6, base), (x + 40 * scale * 0.6, base), (x + 14, base - h * 0.4),
                                   (x + 34 * scale * 0.6, base - h * 0.35)]))
        layer(img, both(trees), col)
    sparkles(img, rng, (210, 255, 120), 50, size=(1.2, 3), alpha=210)
    return img


def theme_neon(rng):
    img = vgrad((40, 8, 70), (6, 10, 40))
    grid = []
    horizon = 300
    for i in range(-14, 15):
        grid.append(("line", [(TW / 2 + i * 20, horizon), (TW / 2 + i * 120, TH)], 2))
    for j in range(7):
        y = horizon + (j ** 1.7) * 6
        grid.append(("line", [(0, y), (TW, y)], 2))
    layer(img, grid, (32, 240, 220), 120)
    shapes = []
    for i in range(6):
        x, y, r = 60 + i * 55, 120 + (i % 3) * 60, 18 + (i % 2) * 10
        shapes.append(("poly", [(x, y - r), (x + r, y), (x, y + r), (x - r, y)]))
    layer(img, both(shapes), (255, 60, 210), 255, blur=3)
    layer(img, both(shapes), (255, 170, 245))
    layer(img, [("line", [(0, horizon), (TW, horizon)], 3)], (120, 255, 240))
    return img


def theme_gold(rng):
    img = vgrad((92, 56, 16), (36, 18, 12))
    pillars = []
    for i in range(7):
        x = 24 + i * 52
        h = rng.uniform(120, 260)
        pillars.append(("poly", [(x - 18, TH), (x - 18, TH - h), (x, TH - h - 22), (x + 18, TH - h), (x + 18, TH)]))
    layer(img, both(pillars), (168, 112, 30))
    layer(img, both([("poly", [(x - 6, TH), (x - 6, TH - rng.uniform(90, 220)), (x + 2, TH - 230), (x + 2, TH)]) for x in range(24, 400, 52)]), (255, 214, 90), 220)
    coins = [("ellipse", (x - 13, y - 13, x + 13, y + 13)) for x, y in [(rng.uniform(20, 380), rng.uniform(40, 220)) for _ in range(9)]]
    layer(img, both(coins), (255, 226, 102))
    sparkles(img, rng, (255, 240, 170), 60)
    return img


def theme_eclipse(rng):
    img = vgrad((70, 8, 22), (14, 4, 12))
    cx, cy, r = 1010, 140, 86
    layer(img, [("ellipse", (cx - r * 1.5, cy - r * 1.5, cx + r * 1.5, cy + r * 1.5))], (255, 70, 40), 150, blur=24)
    layer(img, [("ellipse", (cx - r, cy - r, cx + r, cy + r))], (255, 120, 80))
    layer(img, [("ellipse", (cx - r + 14, cy - r + 6, cx + r + 10, cy + r + 2))], (20, 4, 14))
    layer(img, both([mountains(rng, 330, 5, 80, 200)]), (40, 6, 18))
    layer(img, both([mountains(rng, 380, 7, 60, 150)]), (24, 2, 10))
    sparkles(img, rng, (255, 140, 110), 40, size=(1.2, 3), alpha=180)
    return img


def theme_royal(rng):
    img = vgrad((52, 18, 96), (14, 10, 46))
    for _ in range(14):
        x = rng.choice([rng.uniform(0, 380), rng.uniform(874, TW)])
        y = rng.uniform(40, 360)
        r = rng.uniform(50, 120)
        layer(img, [("ellipse", (x - r, y - r * 0.6, x + r, y + r * 0.6))], rng.choice([(150, 60, 210), (90, 50, 200), (210, 90, 190)]), 90, blur=20)
    crown = [("poly", [(80, 300), (90, 230), (125, 270), (160, 210), (195, 270), (230, 230), (240, 300)])]
    layer(img, both(crown), (255, 205, 90))
    layer(img, both([("poly", [(80, 300), (240, 300), (232, 316), (88, 316)])]), (220, 160, 50))
    sparkles(img, rng, (255, 225, 140), 80)
    return img


def theme_astral(rng):
    img = vgrad((10, 42, 92), (6, 14, 46))
    crystals = []
    for i in range(7):
        x = 40 + i * 50 + rng.uniform(-8, 8)
        y = rng.uniform(90, 330)
        s = rng.uniform(16, 34)
        crystals.append(("poly", [(x, y - s * 2.2), (x + s, y - s * 0.8), (x + s, y + s * 0.8), (x, y + s * 2.2), (x - s, y + s * 0.8), (x - s, y - s * 0.8)]))
    layer(img, both(crystals), (90, 220, 255), 255, blur=6)
    layer(img, both(crystals), (170, 240, 255))
    facets = [("poly", [(p[1][0][0], p[1][0][1]), (p[1][1][0], p[1][1][1]), (p[1][0][0], p[1][3][1])]) for p in crystals]
    layer(img, both(facets), (235, 252, 255), 190)
    sparkles(img, rng, (200, 245, 255), 90, size=(1.2, 4))
    return img


THEMES = {
    "ice": theme_ice, "forest": theme_forest, "neon": theme_neon, "gold": theme_gold,
    "eclipse": theme_eclipse, "royal": theme_royal, "astral": theme_astral,
}


def themes() -> None:
    for name, fn in THEMES.items():
        rng = random.Random(f"geobr-{name}")
        img = fn(rng)
        path = OUT / f"geobr-theme-{name}.png"
        img.resize((TW, TH), Image.LANCZOS).save(path, optimize=True)
        print("wrote", path.relative_to(ROOT))


if __name__ == "__main__":
    for index, spec in SKINS.items():
        arrow_skin(index, spec)
    portal_layers()
    small_effects()
    god_editor_icon()
    collab_icon()
    themes()
