#!/usr/bin/env python3
"""GeoBR mod icon (logo.png): the territory of Brazil in GD style.

Geometry: Natural Earth 1:50m admin-0 (public domain), via the npm package
world-atlas@2.0.2 (TopoJSON). Step 1 extracts Brazil once into
art-source/brazil-outline.json; step 2 renders the icon from that file.

  python3 tools/generate_logo.py --extract <world-atlas>/countries-50m.json
  python3 tools/generate_logo.py
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUTLINE = ROOT / "art-source" / "brazil-outline.json"
SIZE = 336
SS = 4


def extract(topo_path: Path) -> None:
    topo = json.loads(topo_path.read_text())
    sx, sy = topo["transform"]["scale"]
    tx, ty = topo["transform"]["translate"]
    arcs = []
    for arc in topo["arcs"]:
        x = y = 0
        pts = []
        for dx, dy in arc:
            x += dx
            y += dy
            pts.append((x * sx + tx, y * sy + ty))
        arcs.append(pts)

    def ring(indices):
        out = []
        for i in indices:
            pts = arcs[i] if i >= 0 else list(reversed(arcs[~i]))
            out.extend(pts if not out else pts[1:])
        return out

    geom = next(g for g in topo["objects"]["countries"]["geometries"] if g.get("id") == "076")
    polys = geom["arcs"] if geom["type"] == "MultiPolygon" else [geom["arcs"]]
    rings = [ring(poly[0]) for poly in polys]  # outer rings only (no holes in Brazil)
    rings.sort(key=len, reverse=True)
    OUTLINE.parent.mkdir(exist_ok=True)
    OUTLINE.write_text(json.dumps({
        "source": "Natural Earth 1:50m admin-0 (public domain) via world-atlas@2.0.2, id 076",
        "rings": [[[round(x, 3), round(y, 3)] for x, y in r] for r in rings],
    }))
    print(f"extracted {len(rings)} rings, {sum(len(r) for r in rings)} points -> {OUTLINE.relative_to(ROOT)}")


def chamfered(x0, y0, x1, y1, c):
    return [(x0 + c, y0), (x1 - c, y0), (x1, y0 + c), (x1, y1 - c), (x1 - c, y1), (x0 + c, y1), (x0, y1 - c), (x0, y0 + c)]


def render() -> None:
    data = json.loads(OUTLINE.read_text())
    rings = data["rings"]
    # Equirectangular with cos(latitude) correction around Brazil's centre.
    lat0 = math.radians(-14.0)
    proj = [[(lon * math.cos(lat0), lat) for lon, lat in r] for r in rings]
    xs = [p[0] for r in proj for p in r]
    ys = [p[1] for r in proj for p in r]
    minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
    W = SIZE * SS
    box = W * 0.66
    scale = box / max(maxx - minx, maxy - miny)
    ox = (W - (maxx - minx) * scale) / 2
    oy = (W - (maxy - miny) * scale) / 2 + W * 0.01
    shapes = [[(ox + (x - minx) * scale, W - (oy + (y - miny) * scale)) for x, y in r] for r in proj]

    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    # Tile: offset shadow, black contour, thin white border, blue gradient.
    m = int(W * 0.035)
    shadow = Image.new("L", (W, W), 0)
    ImageDraw.Draw(shadow).polygon(chamfered(m + W * 0.025, m + W * 0.03, W - m + W * 0.025 - 1, W - m + W * 0.03 - 1, W * 0.06), fill=150)
    img.paste(Image.new("RGBA", (W, W), (0, 0, 0, 255)), (0, 0), shadow.filter(ImageFilter.GaussianBlur(W * 0.006)))
    tile = Image.new("L", (W, W), 0)
    ImageDraw.Draw(tile).polygon(chamfered(m, m, W - m, W - m, W * 0.06), fill=255)
    img.paste(Image.new("RGBA", (W, W), (0, 0, 0, 255)), (0, 0), tile)
    inner = Image.new("L", (W, W), 0)
    b = m + W * 0.028
    ImageDraw.Draw(inner).polygon(chamfered(b, b, W - b, W - b, W * 0.05), fill=255)
    img.paste(Image.new("RGBA", (W, W), (255, 255, 255, 255)), (0, 0), inner)
    fill = Image.new("L", (W, W), 0)
    f = b + W * 0.012
    ImageDraw.Draw(fill).polygon(chamfered(f, f, W - f, W - f, W * 0.045), fill=255)
    grad = Image.linear_gradient("L").resize((W, W))
    top, bottom = (36, 98, 196), (10, 30, 78)
    blue = Image.composite(Image.new("RGBA", (W, W), bottom + (255,)), Image.new("RGBA", (W, W), top + (255,)), grad)
    img.paste(blue, (0, 0), fill)
    # Soft light rays behind the map, GD menu style.
    rays = Image.new("L", (W, W), 0)
    rd = ImageDraw.Draw(rays)
    cx, cy = W / 2, W * 0.46
    for k in range(12):
        a0 = math.radians(k * 30)
        a1 = a0 + math.radians(12)
        rd.polygon([(cx, cy), (cx + math.cos(a0) * W, cy + math.sin(a0) * W), (cx + math.cos(a1) * W, cy + math.sin(a1) * W)], fill=28)
    img.paste(Image.new("RGBA", (W, W), (255, 255, 255, 255)), (0, 0), ImageChops.multiply(rays, fill))

    # Brazil: shadow, black contour, white edge, green land, yellow rim light.
    land = Image.new("L", (W, W), 0)
    ld = ImageDraw.Draw(land)
    for s in shapes:
        ld.polygon(s, fill=255)
    land_shadow = land.filter(ImageFilter.MaxFilter(25)).transform((W, W), Image.AFFINE, (1, 0, -W * 0.018, 0, 1, -W * 0.022))
    img.paste(Image.new("RGBA", (W, W), (0, 0, 0, 255)), (0, 0), ImageChops.multiply(land_shadow, fill).point(lambda v: v * 0.55))
    contour = land.filter(ImageFilter.MaxFilter(33))
    img.paste(Image.new("RGBA", (W, W), (0, 0, 0, 255)), (0, 0), contour)
    edge = land.filter(ImageFilter.MaxFilter(13))
    img.paste(Image.new("RGBA", (W, W), (255, 255, 255, 255)), (0, 0), edge)
    green = Image.composite(Image.new("RGBA", (W, W), (18, 138, 62, 255)), Image.new("RGBA", (W, W), (62, 214, 96, 255)), grad)
    img.paste(green, (0, 0), land)
    # Yellow rim light on the upper-left edges (GD-style highlight).
    rim = ImageChops.subtract(land, land.transform((W, W), Image.AFFINE, (1, 0, -W * 0.012, 0, 1, -W * 0.012)))
    img.paste(Image.new("RGBA", (W, W), (255, 214, 40, 255)), (0, 0), rim)
    # Small yellow diamond + blue globe accent, a nod to the flag, centred on the land.
    lx = sum(p[0] for p in shapes[0]) / len(shapes[0])
    ly = sum(p[1] for p in shapes[0]) / len(shapes[0])
    dw, dh = W * 0.15, W * 0.10
    diamond = Image.new("L", (W, W), 0)
    ImageDraw.Draw(diamond).polygon([(lx - dw, ly), (lx, ly - dh), (lx + dw, ly), (lx, ly + dh)], fill=255)
    img.paste(Image.new("RGBA", (W, W), (0, 0, 0, 255)), (0, 0), diamond.filter(ImageFilter.MaxFilter(13)))
    img.paste(Image.new("RGBA", (W, W), (255, 214, 40, 255)), (0, 0), diamond)
    r = W * 0.055
    globe = Image.new("L", (W, W), 0)
    ImageDraw.Draw(globe).ellipse([lx - r, ly - r, lx + r, ly + r], fill=255)
    img.paste(Image.new("RGBA", (W, W), (0, 0, 0, 255)), (0, 0), globe.filter(ImageFilter.MaxFilter(9)))
    img.paste(Image.new("RGBA", (W, W), (30, 70, 170, 255)), (0, 0), globe)

    out = img.resize((SIZE, SIZE), Image.LANCZOS)
    out.save(ROOT / "logo.png", optimize=True)
    print("wrote logo.png", out.size)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--extract", type=Path)
    args = parser.parse_args()
    if args.extract:
        extract(args.extract)
    render()
