#!/usr/bin/env python3
"""Generate the three tintable, native-rig GeoBR robot atlases."""

from __future__ import annotations

import plistlib
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "resources" / "robot-pack"
TILE = 64
AA = 4
WHITE = (255, 255, 255, 255)
BLACK = (0, 0, 0, 255)
CLEAR = (0, 0, 0, 0)


def layer() -> Image.Image:
    return Image.new("RGBA", (TILE * AA, TILE * AA), CLEAR)


def scaled_box(box):
    return tuple(round(value * AA) for value in box)


def scaled_points(points):
    return [(round(x * AA), round(y * AA)) for x, y in points]


def polygon(draw: ImageDraw.ImageDraw, points, fill):
    draw.polygon(scaled_points(points), fill=fill)


def line(draw: ImageDraw.ImageDraw, points, fill=BLACK, width=2):
    draw.line(scaled_points(points), fill=fill, width=round(width * AA), joint="curve")


def ellipse(draw: ImageDraw.ImageDraw, box, fill, outline=None, width=1):
    draw.ellipse(scaled_box(box), fill=fill, outline=outline, width=round(width * AA))


def rounded(draw: ImageDraw.ImageDraw, box, radius, fill, outline=None, width=1):
    draw.rounded_rectangle(
        scaled_box(box), radius=round(radius * AA), fill=fill,
        outline=outline, width=round(width * AA),
    )


def downsample(image: Image.Image) -> Image.Image:
    return image.resize((TILE, TILE), Image.Resampling.LANCZOS)


def make_glow(*images: Image.Image) -> Image.Image:
    alpha = Image.new("L", images[0].size, 0)
    for image in images:
        alpha = ImageChops.lighter(alpha, image.getchannel("A"))
    alpha = alpha.filter(ImageFilter.MaxFilter(17))
    glow = Image.new("RGBA", alpha.size, WHITE)
    glow.putalpha(alpha)
    return glow


def sneaker_head():
    primary, secondary, extra = layer(), layer(), layer()
    p, s, e = map(ImageDraw.Draw, (primary, secondary, extra))
    outer = [(7, 14), (42, 8), (57, 16), (54, 47), (41, 53), (9, 47)]
    inner = [(11, 17), (40, 12), (53, 19), (50, 43), (39, 48), (13, 43)]
    band = [(10, 35), (51, 31), (50, 43), (39, 48), (13, 43)]
    polygon(p, outer, BLACK)
    polygon(p, inner, WHITE)
    polygon(p, band, CLEAR)
    line(p, [(10, 35), (51, 31)], BLACK, 2.4)
    polygon(s, band, WHITE)
    ellipse(e, (34, 18, 47, 31), BLACK)
    ellipse(e, (37, 21, 44, 28), WHITE)
    line(e, [(47, 13), (44, 45)], BLACK, 1.4)
    return primary, secondary, extra


def sneaker_body():
    primary, secondary, extra = layer(), layer(), layer()
    p, s = ImageDraw.Draw(primary), ImageDraw.Draw(secondary)
    rounded(p, (20, 12, 45, 48), 6, BLACK)
    rounded(p, (24, 16, 41, 44), 4, WHITE)
    polygon(p, [(9, 22), (24, 16), (27, 26), (15, 35), (7, 31)], BLACK)
    polygon(p, [(12, 24), (23, 20), (23, 25), (15, 31), (11, 29)], WHITE)
    polygon(p, [(40, 18), (55, 24), (57, 33), (48, 37), (39, 27)], BLACK)
    polygon(p, [(43, 22), (52, 26), (53, 31), (49, 33), (43, 26)], CLEAR)
    polygon(s, [(43, 22), (52, 26), (53, 31), (49, 33), (43, 26)], WHITE)
    polygon(p, [(25, 34), (41, 34), (45, 51), (34, 56), (20, 50)], BLACK)
    polygon(p, [(27, 38), (38, 38), (41, 49), (34, 52), (24, 48)], CLEAR)
    polygon(s, [(27, 38), (38, 38), (41, 49), (34, 52), (24, 48)], WHITE)
    return primary, secondary, extra


def high_top_shoe(mirror=False, compact=False):
    primary, secondary, extra = layer(), layer(), layer()
    p, s = ImageDraw.Draw(primary), ImageDraw.Draw(secondary)
    if compact:
        outer = [(10, 27), (30, 23), (39, 32), (56, 37), (58, 47), (52, 52), (13, 52), (6, 45)]
        inner = [(12, 30), (29, 27), (37, 35), (53, 40), (54, 46), (50, 48), (14, 48), (10, 44)]
        panel = [(36, 35), (53, 40), (54, 46), (49, 48), (39, 44)]
    else:
        outer = [(8, 13), (31, 9), (39, 29), (55, 36), (59, 48), (53, 55), (12, 55), (5, 47)]
        inner = [(12, 17), (28, 14), (34, 32), (51, 39), (54, 48), (50, 51), (14, 51), (10, 46)]
        panel = [(34, 32), (51, 39), (54, 48), (49, 51), (39, 47)]
    if mirror:
        outer = [(64 - x, y) for x, y in outer]
        inner = [(64 - x, y) for x, y in inner]
        panel = [(64 - x, y) for x, y in panel]
    polygon(p, outer, BLACK)
    polygon(p, inner, WHITE)
    polygon(p, panel, CLEAR)
    polygon(s, panel, WHITE)
    y_values = (31, 35, 39, 43) if not compact else (34, 38, 42)
    if mirror:
        for y in y_values:
            line(p, [(29, y - 4), (39, y)], BLACK, 1.2)
    else:
        for y in y_values:
            line(p, [(25, y - 4), (35, y)], BLACK, 1.2)
    line(p, [(10, 48), (53, 48)] if not mirror else [(11, 48), (54, 48)], BLACK, 1.7)
    return primary, secondary, extra


def blue_head():
    primary, secondary, extra = layer(), layer(), layer()
    p, s, e = map(ImageDraw.Draw, (primary, secondary, extra))
    rounded(p, (8, 12, 55, 51), 15, BLACK)
    rounded(p, (12, 16, 51, 47), 12, WHITE)
    rounded(p, (8, 29, 19, 44), 4, BLACK)
    polygon(p, [(17, 25), (48, 24), (47, 41), (22, 41)], CLEAR)
    polygon(s, [(17, 25), (48, 24), (47, 41), (22, 41)], WHITE)
    polygon(e, [(21, 28), (43, 28), (38, 33), (35, 33), (35, 39), (29, 39), (29, 33), (25, 33)], BLACK)
    rounded(e, (18, 14, 40, 18), 2, WHITE, BLACK, 1)
    return primary, secondary, extra


def orange_head():
    primary, secondary, extra = layer(), layer(), layer()
    p, s, e = map(ImageDraw.Draw, (primary, secondary, extra))
    outer = [(8, 25), (13, 13), (31, 7), (49, 14), (57, 26), (53, 50), (15, 50)]
    inner = [(13, 26), (17, 17), (31, 12), (45, 18), (52, 28), (48, 45), (18, 45)]
    polygon(p, outer, BLACK)
    polygon(p, inner, WHITE)
    visor = [(14, 27), (50, 27), (46, 37), (18, 37)]
    polygon(p, visor, CLEAR)
    polygon(s, visor, WHITE)
    line(p, [(13, 26), (52, 26)], BLACK, 2.0)
    ellipse(e, (47, 34, 59, 46), BLACK)
    ellipse(e, (51, 38, 56, 43), WHITE)
    line(e, [(22, 17), (31, 12), (43, 18)], BLACK, 1.4)
    return primary, secondary, extra


def compact_body(armored=False):
    primary, secondary, extra = layer(), layer(), layer()
    p, s = ImageDraw.Draw(primary), ImageDraw.Draw(secondary)
    rounded(p, (18, 16, 46, 49), 8, BLACK)
    rounded(p, (23, 20, 41, 44), 5, WHITE)
    chest = [(23, 27), (41, 27), (39, 40), (25, 40)]
    polygon(p, chest, CLEAR)
    polygon(s, chest, WHITE)
    for x1, x2 in ((7, 20), (44, 57)):
        rounded(p, (x1, 22, x2, 40), 5, BLACK)
        inset = (x1 + 3, 25, x2 - 3, 37)
        rounded(p, inset, 3, WHITE if not armored else CLEAR)
        if armored:
            rounded(s, inset, 3, WHITE)
    polygon(p, [(22, 42), (31, 41), (32, 55), (21, 55)], BLACK)
    polygon(p, [(33, 41), (42, 43), (43, 55), (32, 55)], BLACK)
    return primary, secondary, extra


def build_parts(slug: str):
    if slug == "sneaker-bot":
        return [sneaker_head(), sneaker_body(), high_top_shoe(), high_top_shoe(mirror=True)]
    if slug == "blue-runner":
        return [blue_head(), compact_body(), high_top_shoe(compact=True), high_top_shoe(mirror=True, compact=True)]
    if slug == "orange-guard":
        return [orange_head(), compact_body(armored=True), high_top_shoe(compact=True), high_top_shoe(mirror=True, compact=True)]
    raise ValueError(slug)


def frame_sequence(slug: str):
    parts = build_parts(slug)
    frames = []
    prefix = slug.replace("-", "_")
    for index, (primary, secondary, extra) in enumerate(parts, start=1):
        frames.append((f"{prefix}_{index:02}_001.png", primary))
        frames.append((f"{prefix}_{index:02}_2_001.png", secondary))
        frames.append((f"{prefix}_{index:02}_glow_001.png", make_glow(primary, secondary, extra)))
        if index == 1:
            frames.append((f"{prefix}_{index:02}_extra_001.png", extra))
    return [(name, downsample(image)) for name, image in frames]


def write_atlas(slug: str):
    frames = frame_sequence(slug)
    atlas = Image.new("RGBA", (TILE * 4, TILE * 4), CLEAR)
    plist_frames = {}
    for index, (name, image) in enumerate(frames):
        x = (index % 4) * TILE
        y = (index // 4) * TILE
        atlas.alpha_composite(image, (x, y))
        plist_frames[name] = {
            "frame": f"{{{{{x},{y}}},{{{TILE},{TILE}}}}}",
            "offset": "{0,0}",
            "rotated": False,
            "sourceSize": f"{{{TILE},{TILE}}}",
        }

    OUTPUT.mkdir(parents=True, exist_ok=True)
    atlas.save(OUTPUT / f"{slug}.png", optimize=True)
    payload = {
        "frames": plist_frames,
        "metadata": {
            "format": 2,
            "realTextureFileName": f"{slug}.png",
            "size": f"{{{atlas.width},{atlas.height}}}",
            "smartupdate": "$TexturePacker:SmartUpdate:GeoBR$",
            "textureFileName": f"{slug}.png",
        },
    }
    with (OUTPUT / f"{slug}.plist").open("wb") as stream:
        plistlib.dump(payload, stream, sort_keys=False)


def main():
    for slug in ("sneaker-bot", "blue-runner", "orange-guard"):
        write_atlas(slug)
    print(f"Generated 3 robot atlases in {OUTPUT}")


if __name__ == "__main__":
    main()
