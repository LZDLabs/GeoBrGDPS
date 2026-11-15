#!/usr/bin/env python3
"""The GeoBR campaign levels (original designs, tools/design_campaign.py).
Decodes the .gmd files like GD (plist -> k4 -> url-safe base64 -> gzip) and
checks: official song, start state, Arrow portals, ground at y=0, every block
on the 30-unit grid with RobTop's outline piece for its neighbours, and that
rebuilding them (which re-runs every section's physics validation) gives the
same bytes. Usage: campaign_levels.py"""
import base64, gzip, re, sys, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
import design_campaign as D  # noqa: E402

LEVELS = {'teoria-tropical': (11, 2), 'bloqueio-total': (19, 3)}  # official song, Arrow portals
TILE = {'1111': '5', '0111': '2', '1101': '2', '1011': '2', '1110': '2', '0110': '3', '0011': '3', '1100': '3',
        '1001': '3', '1010': '7', '0101': '7', '0010': '6', '1000': '6', '0001': '6', '0100': '6', '0000': '1'}

def decode(path):
    xml = path.read_text()
    assert xml.startswith('<?xml version="1.0"?><plist version="1.0" gjver="2.0"><dict>')
    fields = dict(re.findall(r'<k>(\w+)</k><[is]>([^<]*)</[is]>', xml))
    raw = gzip.decompress(base64.urlsafe_b64decode(fields['k4'])).decode()
    header, *objs = raw.rstrip(';').split(';')
    h = header.split(',')
    objects = [dict(zip(o.split(',')[0::2], o.split(',')[1::2])) for o in objs]
    return fields, dict(zip(h[0::2], h[1::2])), objects

with tempfile.TemporaryDirectory() as tmp:
    import contextlib, io
    log = io.StringIO()
    with contextlib.redirect_stdout(log):
        sys.argv = ['design_campaign.py', tmp]
        D.main()
    for slug in LEVELS:
        assert (Path(tmp) / f'geobr-{slug}.gmd').read_bytes() == (root / f'campaign/geobr-{slug}.gmd').read_bytes(), f'{slug}: rebuild differs'
    sections = [l for l in log.getvalue().splitlines() if 'window' in l]

results = []
for slug, (song, arrows_expected) in LEVELS.items():
    fields, header, objects = decode(root / f'campaign/geobr-{slug}.gmd')
    assert int(fields['k8']) == song and int(fields['k48']) == len(objects)
    assert header['kA2'] == '0' and header['kA4'] == '0' and header['kA11'] == '0' and header['kA13'] == '0'
    assert '_6_1000_' in header['kS38'] and '_6_1004_' in header['kS38']
    assert not [o for o in objects if o['1'] == '1934'], 'no song triggers (official track)'
    arrows = [o for o in objects if o['1'] == '660' and o.get('135') == '1' and o.get('25') == '97']
    assert len(arrows) == arrows_expected and not [o for o in objects if o['1'] == '660' and o.get('135') != '1'], 'Arrow portals'
    assert all(float(o['2']) >= 0 and float(o['3']) >= 0 for o in objects), 'ground is y=0'
    blocks = {(round(float(o['2'])), round(float(o['3']))): o for o in objects if o['1'] in TILE.values()}
    for (x, y), o in blocks.items():
        assert x % 30 == 15 and y % 30 == 15, f'off-grid block at {x},{y}'
        mask = ''.join('1' if (x + dx, y + dy) in blocks else '0' for dx, dy in ((0, 30), (30, 0), (0, -30), (-30, 0)))
        assert o['1'] == TILE[mask], f'outline piece {o["1"]} for neighbours {mask} at {x},{y}'
    results.append(f'{slug}: {len(objects)} objects, {len(blocks)} tiled blocks, {len(arrows)} Arrow portal(s), song #{song}')
print('PASS: ' + ' | '.join(results) + f' | rebuild identical, {len(sections)} sections re-validated')
