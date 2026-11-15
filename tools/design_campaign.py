#!/usr/bin/env python3
"""GeoBR campaign: two ORIGINAL levels, designed by this script.

  * "Teoria Tropical"  - song: Theory of Everything (official GD track 11)
  * "Bloqueio Total"   - song: Deadlocked (official GD track 19)

Nothing here copies RobTop's layouts. RobTop Travel 2 was only used as a
reference for three things, all measured with the analysis notes in RC8.md:
  1. coordinates: in a level string the ground is y=0 and the first row of
     blocks is centred at y=15 (RC7's levels assumed 90 and floated);
  2. the classic block set: RobTop draws outlines only on the outside of a
     structure by choosing 1 of 7 pieces (ids 1-7, with rotation/flip) from
     the 4 neighbours of each cell - reproduced by `tile()`;
  3. musical structure: the moments where the song changes section (where
     RobTop switches gamemode) and, for Deadlocked, the beat accents (pulse
     times). Section boundaries and pulses here sit on those moments.

Every section is checked by a simulator for its gamemode (cube, ball, UFO,
ship, Arrow) over a range of physics constants, from several entry heights,
and must leave a minimum timing window for each input. A section that fails
is regenerated with the next variant; the build fails if none passes.

Usage: design_campaign.py <out dir> [--preview]
"""
from __future__ import annotations

import base64
import gzip
import math
import random
import sys
from dataclasses import dataclass, field
from pathlib import Path

SPEED = {0.5: 251.16, 1: 311.58, 2: 387.42, 3: 468.0}
SPEED_PORTAL = {0.5: 200, 1: 201, 2: 202, 3: 203}
SPEED_HEADER = {0.5: 1, 1: 0, 2: 2, 3: 3}
MODE_PORTAL = {'cube': 12, 'ship': 13, 'ball': 47, 'ufo': 111, 'arrow': 660}
PORTAL_Y = 60            # every mode portal: spans ~17..103, reachable from the ground
FPS = 60.0

# --------------------------------------------------------------- geometry
@dataclass
class Box:
    x0: float
    y0: float
    x1: float
    y1: float

    def hits(self, x: float, y: float, hw: float, hh: float) -> bool:
        return x + hw > self.x0 and x - hw < self.x1 and y + hh > self.y0 and y - hh < self.y1


def cx(col: int) -> float: return 30 * col + 15
def cy(row: int) -> float: return 30 * row + 15


@dataclass
class Level:
    name: str
    song: int
    start_speed: float
    solids: set = field(default_factory=set)               # (col, row)
    spikes: dict = field(default_factory=dict)             # (col, row) -> flipped
    pads: dict = field(default_factory=dict)               # (col, row) -> flipped
    orbs: set = field(default_factory=set)                 # (col, row)
    deco: list = field(default_factory=list)               # raw object strings
    triggers: list = field(default_factory=list)
    portals: list = field(default_factory=list)
    report: list = field(default_factory=list)

    # ---- building
    def block(self, col, row): self.solids.add((col, row))
    def column(self, col, r0, r1):
        for r in range(r0, r1): self.solids.add((col, r))
    def spike(self, col, row, down=False): self.spikes[(col, row)] = down
    def pad(self, col, row, down=False): self.pads[(col, row)] = down
    def orb(self, col, row): self.orbs.add((col, row))

    def clear(self, c0, c1):
        for store in (self.solids, self.orbs):
            for k in [k for k in store if c0 <= k[0] < c1]: store.discard(k)
        for store in (self.spikes, self.pads):
            for k in [k for k in store if c0 <= k[0] < c1]: del store[k]

    def clear_low(self, c0, c1, below=7):
        """Undo a gameplay pattern without touching the scenery above it."""
        for store in (self.solids, self.orbs):
            for k in [k for k in store if c0 <= k[0] < c1 and k[1] < below]: store.discard(k)
        for store in (self.spikes, self.pads):
            for k in [k for k in store if c0 <= k[0] < c1 and k[1] < below - 1]: del store[k]

    def portal(self, oid, x, y=PORTAL_Y, extra=''):
        self.portals.append(f'1,{oid},2,{x:g},3,{y:g}' + extra)

    def color(self, x, channel, rgb, duration=0.5, y=None):
        r, g, b = rgb
        self.triggers.append(f'1,899,2,{x:g},3,{y if y is not None else 345 + 30 * (len(self.triggers) % 6)},36,1,7,{r},8,{g},9,{b},10,{duration:g},35,1,23,{channel}')

    def pulse(self, x, channel, rgb, hold=0.05, fade=0.35):
        r, g, b = rgb
        self.triggers.append(f'1,1006,2,{x:g},3,{525 + 30 * (len(self.triggers) % 4)},36,1,51,{channel},45,0,46,{hold:g},47,{fade:g},7,{r},8,{g},9,{b}')

    # ---- collision view for the simulators
    def solid_boxes(self, x0, x1):
        c0, c1 = int(x0 // 30) - 2, int(x1 // 30) + 2
        return [Box(30 * c, 30 * r, 30 * c + 30, 30 * r + 30) for (c, r) in self.solids if c0 <= c <= c1]

    def hazard_boxes(self, x0, x1):
        c0, c1 = int(x0 // 30) - 2, int(x1 // 30) + 2
        out = []
        for (c, r), down in self.spikes.items():
            if not c0 <= c <= c1: continue
            # GD's spike hazard is a small box low in the sprite (about 6x12); 8x14 here.
            if down: out.append(Box(cx(c) - 4, 30 * r + 14, cx(c) + 4, 30 * r + 28))
            else: out.append(Box(cx(c) - 4, 30 * r + 2, cx(c) + 4, 30 * r + 16))
        return out

    # ---- export
    def tile(self):
        """RobTop's classic auto-tiling (learned from his levels): piece by
        neighbour mask N,E,S,W -> (id, rotation, flip x, flip y)."""
        table = {
            '1111': (5, 0, 0, 0), '0111': (2, 0, 0, 0), '1101': (2, 0, 0, 1), '1011': (2, 90, 0, 0),
            '1110': (2, -90, 0, 0), '0110': (3, 0, 0, 0), '0011': (3, 0, 1, 0), '1100': (3, 0, 0, 1),
            '1001': (3, 0, 1, 1), '1010': (7, 0, 0, 0), '0101': (7, 90, 0, 0), '0010': (6, 0, 0, 0),
            '1000': (6, 0, 0, 1), '0001': (6, 90, 0, 0), '0100': (6, -90, 0, 0), '0000': (1, 0, 0, 0),
        }
        out = []
        for (c, r) in sorted(self.solids):
            mask = ''.join('1' if (c + dc, r + dr) in self.solids else '0' for dc, dr in ((0, 1), (1, 0), (0, -1), (-1, 0)))
            oid, rot, fx, fy = table[mask]
            s = f'1,{oid},2,{cx(c):g},3,{cy(r):g}'
            if rot: s += f',6,{rot}'
            if fx: s += ',4,1'
            if fy: s += ',5,1'
            out.append(s)
        return out

    def objects(self):
        objs = self.tile()
        for (c, r), down in sorted(self.spikes.items()):
            objs.append(f'1,8,2,{cx(c):g},3,{cy(r):g}' + (',5,1' if down else ''))
        for (c, r), down in sorted(self.pads.items()):
            objs.append(f'1,35,2,{cx(c):g},3,{30 * r + (28 if down else 2):g}' + (',5,1' if down else ''))
        for (c, r) in sorted(self.orbs):
            objs.append(f'1,36,2,{cx(c):g},3,{cy(r):g}')
        return objs + self.portals + self.deco + self.triggers


# --------------------------------------------------------------- simulators
@dataclass
class SimResult:
    ok: bool
    window: float            # tightest input window, in frames
    note: str = ''
    path: list = field(default_factory=list)


def _q(v, step): return round(v / step)


def run_search(start_states, step_fn, frames, finish_ok, branch_fn, key_fn):
    """Generic forward search with merging, then backward win marking.
    Returns (all_entries_win, tightest window in frames, canonical path)."""
    layers = [dict() for _ in range(frames + 1)]
    for s in start_states:
        layers[0][key_fn(s)] = (s, [])
    for i in range(frames):
        nxt = layers[i + 1]
        for k, (s, children) in layers[i].items():
            for choice in branch_fn(s, i):
                ns = step_fn(s, choice, i)
                if ns is None: continue
                nk = key_fn(ns)
                if nk not in nxt: nxt[nk] = (ns, [])
                children.append((choice, nk))
    win = [dict() for _ in range(frames + 1)]
    for k, (s, _) in layers[frames].items(): win[frames][k] = finish_ok(s)
    for i in range(frames - 1, -1, -1):
        for k, (s, children) in layers[i].items():
            win[i][k] = any(win[i + 1].get(nk, False) for _, nk in children)
    entries_ok = all(win[0][key_fn(s)] for s in start_states)
    # Route from the first entry, playing like a person: no input until one
    # is needed, then the input is made in the middle of the frames where it
    # works. The tightest such window (in frames) is the section's margin.
    if not win[0][key_fn(start_states[0])]: return entries_ok, 0, []
    k = key_fn(start_states[0]); tight = 999; hist = []; i = 0; last_input = -1
    while i < frames:
        s, children = layers[i][k]
        idle = [c for c in children if c[0] == 0 and win[i + 1].get(c[1], False)]
        if idle:
            hist.append((i, k)); k = idle[0][1]; i += 1
            continue
        moves = [c for c in children if c[0] != 0 and win[i + 1].get(c[1], False)]
        choice = moves[0][0]
        earliest = i
        for j, pk in reversed(hist):
            if j <= last_input: break
            if any(c[0] == choice and win[j + 1].get(c[1], False) for c in layers[j][pk][1]): earliest = j
            else: break
        tight = min(tight, i - earliest + 1)
        mid = (earliest + i) // 2
        while hist and hist[-1][0] >= mid: i, k = hist.pop()
        if i != mid: i, k = mid, k
        s, children = layers[i][k]
        nxt = next(c for c in children if c[0] == choice and win[i + 1].get(c[1], False))
        last_input = i
        hist.append((i, k)); k = nxt[1]; i += 1
    path = [layers[j][pk][0] for j, pk in hist]
    return entries_ok, tight, path


class Grid:
    """O(1) collision queries: solids are 30x30 cells, spikes are indexed by
    column (the simulators used to scan every box of the section)."""
    def __init__(self, lv: Level):
        self.cells = lv.solids
        self.haz = {}
        for b in lv.hazard_boxes(-1e9, 1e9):
            for c in range(int(b.x0 // 30), int(b.x1 // 30) + 1): self.haz.setdefault(c, []).append(b)

    def solids(self, x, y, hw, hh):
        out = []
        for c in range(math.floor((x - hw) / 30), math.floor((x + hw - 1e-6) / 30) + 1):
            for r in range(max(0, math.floor((y - hh) / 30)), math.floor((y + hh - 1e-6) / 30) + 1):
                if (c, r) in self.cells: out.append((30 * r, 30 * r + 30))
        return out

    def hazard(self, x, y, hw, hh):
        for c in range(math.floor((x - hw) / 30), math.floor((x + hw) / 30) + 1):
            for b in self.haz.get(c, ()):
                if b.hits(x, y, hw, hh): return True
        return False

    def resolve(self, x, y, ny, vy, hw, hh):
        """Move from y to ny: land on a top, stop under a bottom (when the
        previous frame was clear of it), die on anything else."""
        state = None
        if ny - hh <= 0: ny, vy, state = hh, 0.0, 'floor'
        for bottom, top in self.solids(x, ny, hw, hh):
            if vy <= 0 and y - hh >= top - 1.0: ny, vy, state = top + hh, 0.0, 'floor'
            elif vy >= 0 and y + hh <= bottom + 1.0: ny, vy, state = bottom - hh, 0.0, 'ceil'
            else: return None
        if self.solids(x, ny, hw - .01, hh - .01): return None
        return ny, vy, state

    def standing(self, x, y, hh):
        if y - hh <= 0.01: return True
        return bool(self.solids(x, y - hh - 1, 14.9, 0.5))


def sim_cube(lv: Level, x0, x1, speed, params, entries):
    vx = SPEED[speed] / FPS
    grid = Grid(lv)
    pads = [(cx(c), 30 * r) for (c, r), d in lv.pads.items() if not d and x0 - 30 <= cx(c) <= x1 + 30]
    orbs = [(cx(c), cy(r)) for (c, r) in lv.orbs if x0 - 30 <= cx(c) <= x1 + 30]
    g, J, Jpad, Jorb = params
    frames = int((x1 - x0) / vx)

    def orb_near(x, y):
        for o in orbs:
            if abs(o[0] - x) < 33 and abs(o[1] - y) < 33: return o
        return None

    def step(s, choice, i):
        x, y, vy, ground, used = s
        x = x0 + (i + 1) * vx
        if choice == 1: vy = J; ground = False
        elif choice == 2: vy = Jorb; ground = False; used = used | {orb_near(x, y)}
        if not ground: vy = max(vy - g, -15.0)
        for px, py in pads:
            if abs(px - x) < 18 and y - 15 < py + 6 and y + 15 > py: vy = Jpad; ground = False
        ny = y + vy if not ground else y
        r = grid.resolve(x, y, ny, vy, 15, 15)
        if r is None: return None
        ny, vy, state = r
        if state == 'ceil': return None                      # head hit
        if grid.hazard(x, ny, 15, 15): return None
        ground = state == 'floor' or (ground and grid.standing(x, ny, 15))
        if ground and not grid.standing(x, ny, 15): ground = False
        return (x, ny, vy, ground, used)

    def branch(s, i):
        x, y, vy, ground, used = s
        out = [0]
        if ground: out.append(1)
        o = orb_near(x, y)
        if o and o not in used and not ground: out.append(2)
        return out

    key = lambda s: (_q(s[1], .5), _q(s[2], .1), s[3], s[4])
    starts = [(x0, y, 0.0, y <= 15.01, frozenset()) for y in entries]
    ok, tight, path = run_search(starts, step, frames, lambda s: 0 < s[1] < 105, branch, key)
    return SimResult(ok, tight, path=path)


def sim_flyer(lv: Level, x0, x1, speed, params, entries, mode):
    """ship (hold), ufo (tap), ball (tap on a surface flips gravity)."""
    vx = SPEED[speed] / FPS
    grid = Grid(lv)
    frames = int((x1 - x0) / vx)
    every = 3 if mode != 'ball' else 1
    hw, hh = (15, 15) if mode == 'ball' else (14, 12)

    if mode in ('ship', 'ufo'):
        return pilot(lv, grid, x0, x1, speed, params, entries, mode)

    g, v0 = params
    def step(s, choice, i):
        x, y, vy, grav, surf = s
        x = x0 + (i + 1) * vx
        if choice == 1: grav = -grav; vy = v0 * grav
        vy = max(-15.0, min(15.0, vy - g * grav))
        r = grid.resolve(x, y, y + vy, vy, hw, hh)
        if r is None: return None
        ny, nvy, land = r
        if grid.hazard(x, ny, hw, hh): return None
        surf = land is not None and ((land == 'floor') == (grav > 0))
        return (x, ny, nvy, grav, surf)
    branch = lambda s, i: [0, 1] if s[4] else [0]
    key = lambda s: (_q(s[1], .5), _q(s[2], .1), s[3], s[4])
    starts = [(x0, y, 0.0, 1, y <= 15.01) for y in entries]
    ok, tight, path = run_search(starts, step, frames, lambda s: 0 < s[1] < 100, branch, key)
    return SimResult(ok, tight, path=path)


def corridor_at(lv, col):
    """Free vertical span (bottom, top) of a column for flying modes."""
    rows = [r for r in range(0, 10) if (col, r) not in lv.solids]
    if not rows: return 0, 0
    lo = rows[0]
    hi = lo
    while hi + 1 < 10 and (col, hi + 1) not in lv.solids: hi += 1
    bottom, top = 30 * lo, 30 * hi + 30
    for (c, r), down in lv.spikes.items():
        if c == col and lo <= r <= hi:
            if down: top = min(top, 30 * r + 14)
            else: bottom = max(bottom, 30 * r + 16)
    return bottom, top


def pilot(lv, grid, x0, x1, speed, params, entries, mode):
    last_col = int(x1 // 30) - 1
    """A simple human-like pilot (decisions every 3 frames) that steers to the
    middle of the corridor two columns ahead. If it gets through every
    physics variant from every entry height with clearance, a person can.
    `window` = smallest clearance (units) to any block or spike."""
    vx = SPEED[speed] / FPS
    frames = int((x1 - x0) / vx)
    # inflated hitboxes: the margin is built into the collision itself
    hw, hh = (15, 15) if mode == 'ufo' else (15, 13)
    worst = 999.0; where = None
    for y0 in entries:
        y, vy, hold, last_tap = y0, 0.0, 0, -99
        for i in range(frames):
            x = x0 + (i + 1) * vx
            # aim at the tightest opening in the next 4 columns
            col = int((x + 15) // 30)
            spans = [corridor_at(lv, min(k, last_col)) for k in range(col, col + (5 if mode == 'ship' else 8))]
            narrow = min(spans, key=lambda sp: sp[1] - sp[0])
            target = (narrow[0] + narrow[1]) / 2
            if mode == 'ufo' or i % 3 == 0:
                if mode == 'ship':
                    up, down, vmax = params
                    hold = 1 if (y + vy * 6) < target else 0
                else:
                    g, J, vmin = params
                    # tap at the bottom of the bob: the tap lifts ~J^2/2g
                    tap = y < target - 15 and vy <= 0 and i - last_tap >= 6
                    if tap: vy = J; last_tap = i
            if mode == 'ship':
                vy = min(vmax, vy + up) if hold else max(-vmax, vy - down)
            elif i != last_tap:
                vy = max(vmin, vy - g)
            r = grid.resolve(x, y, y + vy, vy, hw, hh)
            if r is None or grid.hazard(x, r[0], hw, hh):
                return SimResult(False, 0, note=f'pilot hit at x={x:.0f} y={y:.0f} entry {y0}')
            y, vy = r[0], r[1]
            # clearance to blocks/spikes around the ship
            for m in (4, 8, 12, 16, 20):
                if grid.solids(x, y, hw + m, hh + m) or grid.hazard(x, y, hw + m, hh + m):
                    if m - 4 < worst: worst, where = m - 4, (round(x), round(y))
                    break
        if not (0 < y < 105):
            return SimResult(False, 0, note=f'pilot ended at y={y:.0f}')
    return SimResult(True, worst, note=f'tightest at {where}' if worst < 999 else '')


def sim_arrow(lv: Level, x0, x1, speed, entries, half=6.0):
    """The Arrow (and GD's wave) moves at exactly 45 degrees; it slides along
    block tops/bottoms and the ground, dies on block sides and spikes."""
    vx = SPEED[speed] / FPS
    grid = Grid(lv)
    frames = int((x1 - x0) / vx)

    def step(s, choice, i):
        x, y, d = s
        x = x0 + (i + 1) * vx
        if choice == 1: d = -d
        r = grid.resolve(x, y, y + d * vx, d, half, half)
        if r is None: return None
        ny = r[0]
        if grid.hazard(x, ny, half, half): return None
        return (x, ny, d)

    branch = lambda s, i: [0, 1]
    key = lambda s: (_q(s[1], 1.0), s[2])
    starts = [(x0, y, d) for y in entries for d in (-1, 1)]
    ok, tight, path = run_search(starts, step, frames, lambda s: 0 < s[1] < 105, branch, key)
    return SimResult(ok, tight, path=path)


# physics ranges (per 1/60 s); every section must pass with all of them
CUBE_PARAMS = [(0.958, 11.18, 16.0, 11.18), (0.958, 10.6, 15.0, 10.4), (0.958, 11.7, 17.0, 11.7)]
SHIP_PARAMS = [(0.35, 0.45, 6.0), (0.5, 0.6, 8.0), (0.6, 0.8, 9.0)]
UFO_PARAMS = [(0.6, 7.0, -10.0), (0.75, 8.0, -12.0), (0.5, 6.5, -9.0)]
BALL_PARAMS = [(0.55, 3.0), (0.7, 4.5), (0.45, 2.5)]


# ----------------------------------------------------------------- sections
@dataclass
class Section:
    mode: str
    t0: float
    t1: float
    speed: float
    style: dict


def build_cube(lv, c0, c1, speed, rng, hard, check=None):
    """Ground run with spikes, steps, pillars over spike beds, yellow pads
    and orbs. Each pattern is kept only if the section so far still passes
    the simulator with the required margin (else another one is tried)."""
    c = c0 + 5
    tries = 0
    while c < c1 - 8:
        before = c
        kind = rng.choice(['spikes', 'step', 'platforms', 'pad', 'orbs', 'stairs', 'double', 'spikeblock'] if hard
                          else ['spikes', 'spikes', 'step', 'platforms', 'pad', 'stairs', 'double', 'spikeblock'])
        room = c1 - 8 - c
        if kind == 'spikes' and room >= 6:
            n = rng.choice([1, 2, 2] + ([3] if hard and speed <= 1 else []))
            for k in range(n): lv.spike(c + k, 0)
            c += n + rng.randint(3, 4)
        elif kind == 'step' and room >= 11:
            w = rng.randint(5, 7)
            for k in range(w): lv.block(c + k, 0)
            if w >= 6 and rng.random() < .6: lv.spike(c + w // 2, 1)
            if rng.random() < .5: lv.spike(c + w + 1, 0)
            c += w + rng.randint(3, 5)
        elif kind == 'platforms' and room >= 16:
            # pillars of height 1-2 with spike beds between them
            k = c
            for p in range(rng.randint(2, 3)):
                for g_ in range(2): lv.spike(k + g_, 0)
                k += 2
                h = rng.choice([1, 2]) if hard else 1
                w = rng.randint(2, 3)
                for q in range(w): lv.column(k + q, 0, h)
                k += w
            for g_ in range(2): lv.spike(k + g_, 0)
            c = k + 2 + rng.randint(3, 4)
        elif kind == 'pad' and room >= 14:
            lv.pad(c, 0)
            land = c + (5 if speed <= 1 else 6)
            for q in range(5): lv.column(land + q, 0, 3)
            for q in range(c + 1, land): lv.spike(q, 0)
            c = land + 5 + rng.randint(3, 4)
        elif kind == 'orbs' and room >= 11:
            for q in range(5): lv.spike(c + q, 0)
            lv.orb(c + 2, 2)
            c += 5 + rng.randint(3, 4)
        elif kind == 'stairs' and room >= 15:
            for q in range(3): lv.column(c + q, 0, 1)
            for q in range(3, 7): lv.column(c + q, 0, 2)
            if hard and rng.random() < .5: lv.spike(c + 5, 2)
            lv.spike(c + 8, 0)
            c += 9 + rng.randint(3, 4)
        elif kind == 'double' and room >= 12:
            for q in range(2): lv.column(c + q, 0, 1)
            for q in range(2, 5): lv.column(c + q, 0, 2)
            for q in range(5, 7): lv.column(c + q, 0, 1)
            lv.spike(c + 8, 0)
            c += 9 + rng.randint(3, 4)
        elif kind == 'spikeblock' and room >= 9:
            lv.spike(c, 0)
            for q in range(2, 4): lv.block(c + q, 0)
            if rng.random() < .5: lv.spike(c + 5, 0)
            c += 6 + rng.randint(3, 4)
        else:
            c += 2
        if check and c != before and c - before > 2:
            if not check(max(c0, before - 3), min(c + 2, c1)):
                lv.clear_low(before, c)
                tries += 1
                c = before + (0 if tries < 6 else 3)
                if tries >= 6: tries = 0
            else:
                tries = 0


def ceiling_mass(lv, col, row, top=9):
    lv.column(col, row, top + 1)


def write_flying(lv, c0, c1, floor, ceil, exit_ceil=4, entry_ceil=6, funnel=12):
    """Writes a flying corridor from per-column floor/ceiling rows: free
    entry (floor 0), the body, and an exit funnel whose ceiling comes down
    one row every two columns to `exit_ceil`, so the next portal is reached
    from any height. Returns the columns where obstacles may go."""
    for col in range(c0 + 1, c1):
        f, h = floor.get(col, 0), ceil.get(col, 9)
        if col <= c0 + 4: f, h = 0, max(h, entry_ceil)
        if col >= c1 - funnel:
            f = 0
            h = max(exit_ceil, min(h, exit_ceil + (c1 - 1 - col) // 2))
        if f > 0: lv.column(col, 0, f)
        ceiling_mass(lv, col, h)
    return range(c0 + 7, c1 - funnel - 1)


def build_ship(lv, c0, c1, speed, rng, hard):
    gap = 4 if hard else 5
    floor, ceil = {}, {}
    f = 0
    for col in range(c0 + 5, c1):
        if (col - c0) % 4 == 0 and rng.random() < .7:
            f = max(0, min(9 - gap - 1, f + rng.choice([-1, 1, 1, -1, 0])))
        floor[col], ceil[col] = f, f + gap
    body = write_flying(lv, c0, c1, floor, ceil)
    for col in body:
        if rng.random() < (.12 if hard else .07) and floor.get(col - 1) == floor.get(col) == floor.get(col + 1):
            if rng.random() < .5 and floor[col] > 0: lv.spike(col, floor[col])
            elif gap >= 5: lv.spike(col, ceil[col] - 1, True)


def build_ufo(lv, c0, c1, speed, rng, hard):
    top = 8
    body = write_flying(lv, c0, c1, {}, {col: top for col in range(c0, c1)})
    col = body.start
    prev = 1
    while col < body.stop:
        gap = 5
        low = max(0, min(top - gap, prev + rng.randint(-1, 1) * (2 if hard else 1)))
        prev = low
        lv.column(col, 0, low)
        lv.column(col, low + gap, top)
        # one spiked side per pillar (alternating), like a flappy gate
        if rng.random() < .5:
            if low > 0: lv.spike(col, low)
        elif low + gap < top: lv.spike(col, low + gap - 1, True)
        col += rng.randint(7, 9) if hard else rng.randint(8, 10)


def build_ball(lv, c0, c1, speed, rng, hard):
    ceil = 5
    body = write_flying(lv, c0, c1, {}, {col: ceil for col in range(c0, c1)}, exit_ceil=3, entry_ceil=5, funnel=8)
    col = body.start
    side = 0
    while col < body.stop:
        n = rng.choice([1, 2] if hard else [1, 1, 2])
        for k in range(n):
            if side == 0: lv.spike(col + k, 0)
            else: lv.spike(col + k, ceil - 1, True)
        side ^= 1
        col += n + rng.randint(5, 7)
    # gravity back to normal before leaving
    lv.portal(10, cx(c1 - 3), 45)


def arrow_profile(rng, c0, c1, gap, amplitude=3):
    """Zigzag tunnel: floor and ceiling follow the same 45-degree staircase,
    `gap` rows apart; the Arrow toggles at every corner."""
    floor, ceil = {}, {}
    f, up, run = 0, True, 0
    top = 9 - gap - 1
    for col in range(c0, c1):
        if run == 0:
            run = rng.randint(2, amplitude)
            up = True if f == 0 else False if f >= top else not up
        f = max(0, min(top, f + (1 if up else -1)))
        run -= 1
        floor[col], ceil[col] = f, f + gap
    return floor, ceil


def build_arrow(lv, c0, c1, speed, rng, hard):
    """First half: pillars that alternate floor/ceiling with spikes on the
    tips. Second half (long sections): a 45-degree zigzag tunnel."""
    floor, ceil = {}, {col: 8 for col in range(c0, c1)}
    zig_from = c0 + (c1 - c0) // 2 if c1 - c0 >= 40 else c1
    if zig_from < c1:
        zf, zc = arrow_profile(rng, zig_from, c1, 4 if hard else 5)
        floor.update(zf); ceil.update(zc)
    body = write_flying(lv, c0, c1, floor, ceil)
    col = body.start
    flip = 0
    while col < min(body.stop, zig_from - 4):
        if flip == 0:
            hgt = rng.randint(2, 3)
            lv.column(col, 0, hgt); lv.spike(col, hgt)
        else:
            dep = rng.randint(2, 3)
            lv.column(col, 8 - dep, 8); lv.spike(col, 8 - dep - 1, True)
        flip ^= 1
        col += rng.randint(5, 6) if hard else rng.randint(6, 7)


def cube_scenery(lv, c0, c1, rng):
    """Hanging block formations above cube sections (rows 7-9, out of reach
    of any jump, pad or orb here) with spikes pointing down, for the
    classic RobTop silhouette. The simulator still sees them."""
    col = c0 + rng.randint(3, 8)
    while col < c1 - 6:
        w = rng.randint(3, 7)
        depth = rng.choice([1, 1, 2])
        for q in range(w):
            ceiling_mass(lv, col + q, 9 - depth + (1 if q in (0, w - 1) and depth == 2 else 0))
        for q in range(1, w - 1):
            if rng.random() < .5: lv.spike(col + q, 9 - depth - (0 if q not in (0, w - 1) else 0) - 1 + (0), True)
        col += w + rng.randint(5, 10)


BUILDERS = {'cube': build_cube, 'ship': build_ship, 'ufo': build_ufo, 'ball': build_ball, 'arrow': build_arrow}


def validate(lv, mode, c0, c1, speed, hard, first):
    x0, x1 = cx(c0), cx(c1)
    entries = [15.0] if first else [15.0, 45.0, 75.0, 100.0]
    need = 5 if not hard else 4
    worst = 999
    if mode == 'cube':
        for p in CUBE_PARAMS:
            r = sim_cube(lv, x0, x1, speed, p, entries)
            if not r.ok: return False, 0, r
            worst = min(worst, r.window)
        return worst >= need, worst, r
    if mode == 'arrow':
        r = sim_arrow(lv, x0, x1, speed, [15.0, 45.0, 75.0, 95.0])
        return r.ok and r.window >= (6 if not hard else 5), r.window, r
    params = {'ship': SHIP_PARAMS, 'ufo': UFO_PARAMS, 'ball': BALL_PARAMS}[mode]
    for p in params:
        r = sim_flyer(lv, x0, x1, speed, p, entries, mode)
        if not r.ok: return False, 0, r
        worst = min(worst, r.window)
    if mode in ('ship', 'ufo'): return worst >= (0 if mode == 'ufo' else 4), worst, r   # clearance (units) along the pilot's path
    return worst >= need, worst, r


def design(name, song, sections, palettes, hard, seed, pulses=()):
    lv = Level(name, song, sections[0].speed)
    # time -> x
    x = 0.0
    bounds = []
    for s in sections:
        x0 = x
        x += (s.t1 - s.t0) * SPEED[s.speed]
        bounds.append((x0, x))
    cols = [(int(round((a - 15) / 30)), int(round((b - 15) / 30))) for a, b in bounds]
    for i, (s, (c0, c1)) in enumerate(zip(sections, cols)):
        ok = False
        for attempt in range(40):
            lv.clear(c0 + 1, c1)
            lv.portals = [p for p in lv.portals if not (cx(c0) - 1 <= float(p.split(',')[3]) <= cx(c1) - 1)]
            rng = random.Random(f'{seed}-{i}-{attempt}')
            if s.mode == 'cube':
                check = lambda a, b, s=s: validate(lv, 'cube', a, b, s.speed, hard, True)[0]
                cube_scenery(lv, c0, c1, rng)
                build_cube(lv, c0, c1, s.speed, rng, hard, check)
            else:
                BUILDERS[s.mode](lv, c0, c1, s.speed, rng, hard)
            ok, window, res = validate(lv, s.mode, c0, c1, s.speed, hard, i == 0)
            if ok: break
        if not ok:
            raise SystemExit(f'{name}: section {i} ({s.mode}) has no valid variant')
        extra = ',135,1,25,97' if s.mode == 'arrow' else ''
        if i > 0 or s.mode != 'cube':
            lv.portal(MODE_PORTAL[s.mode], cx(c0), PORTAL_Y, extra)
        if i > 0 and s.speed != sections[i - 1].speed:
            lv.portal(SPEED_PORTAL[s.speed], cx(c0) + 10, PORTAL_Y)
        pal = palettes[s.style.get('palette', s.mode)]
        lv.color(cx(c0), 1000, pal['bg'], .6)
        lv.color(cx(c0), 1001, pal['ground'], .6)
        lv.color(cx(c0), 1004, pal['obj'], .3)
        lv.report.append(f'{s.mode:5s} {s.t0:6.2f}-{s.t1:6.2f}s x{s.speed:<3g} cols {c0}-{c1} attempt {attempt} window {window} frames')
        # ground decoration on free surfaces
        for col in range(c0 + 2, c1 - 1):
            if rng.random() < .22:
                surf = 0
                while (col, surf) in lv.solids: surf += 1
                if (col, surf) in lv.spikes or (col, surf) in lv.pads or (col, surf + 1) in lv.solids: continue
                if s.mode != 'cube' and surf == 0: continue
                did = rng.choice([18, 19, 20, 21])
                off = {18: 19, 19: 19, 20: 13, 21: 7}[did]
                lv.deco.append(f'1,{did},2,{cx(col):g},3,{30 * surf + off - 15 + 15:g}')
    end_x = bounds[-1][1]
    for t in pulses:
        px = time_to_x(sections, t)
        if px < end_x - 60: lv.pulse(px, 1000, (90, 90, 120) if not hard else (120, 20, 30))
    lv.deco.append(f'1,18,2,{end_x - 45:g},3,19')
    return lv, cols


def time_to_x(sections, t):
    x = 0.0
    for s in sections:
        if t <= s.t1:
            return x + (t - s.t0) * SPEED[s.speed]
        x += (s.t1 - s.t0) * SPEED[s.speed]
    return x


def level_string(lv: Level, palettes, first_palette):
    p = palettes[first_palette]
    def ch(cid, rgb, blend=False):
        r, g, b = rgb
        return f'1_{r}_2_{g}_3_{b}_11_255_12_255_13_255_4_-1_6_{cid}_7_1_15_1_18_0_8_1' + ('_5_1' if blend else '')
    colors = '|'.join([ch(1000, p['bg']), ch(1001, p['ground']), ch(1009, p['ground2']), ch(1002, (255, 255, 255), True),
                       ch(1004, p['obj']), ch(1003, (255, 255, 255))]) + '|'
    header = (f'kS38,{colors},kA13,0,kA15,0,kA16,0,kA14,,kA6,0,kA7,0,kA25,0,kA17,0,kA18,0,kS39,0,'
              f'kA2,0,kA3,0,kA8,0,kA4,{SPEED_HEADER[lv.start_speed]},kA9,0,kA10,0,kA22,0,kA23,0,kA24,0,kA27,1,kA40,1,kA41,1,kA42,1,'
              'kA28,0,kA29,0,kA31,1,kA32,1,kA36,0,kA43,0,kA44,0,kA45,1,kA46,0,kA33,1,kA34,1,kA35,0,'
              'kA37,1,kA38,1,kA39,1,kA19,0,kA26,0,kA20,0,kA21,0,kA11,0')
    objs = lv.objects()
    return header + ';' + ';'.join(objs) + ';', len(objs)


def gmd(name, level, description, song, count):
    packed = base64.urlsafe_b64encode(gzip.compress(level.encode(), mtime=0)).decode()
    desc = base64.urlsafe_b64encode(description.encode()).decode()
    fields = [('kCEK', 'i', 4), ('k2', 's', name), ('k4', 's', packed), ('k3', 's', desc),
              ('k5', 's', 'Equipe GeoBR'), ('k13', 't', None), ('k21', 'i', 2), ('k16', 'i', 1),
              ('k8', 'i', song), ('k50', 'i', 45), ('k48', 'i', count)]
    body = ''.join(f'<k>{k}</k>' + ('<t />' if t == 't' else f'<{t}>{v}</{t}>') for k, t, v in fields)
    return f'<?xml version="1.0"?><plist version="1.0" gjver="2.0"><dict>{body}</dict></plist>'


# --------------------------------------------------------------- the levels
# Section boundaries = where the songs change section (seconds at the start
# of each phrase, measured from RobTop's own timing of the same songs).
TOE = [  # Theory of Everything, all at 1x
    ('cube', 0.00, 7.32), ('ship', 7.32, 13.77), ('cube', 13.77, 22.24), ('ufo', 22.24, 28.50),
    ('cube', 28.50, 36.41), ('ball', 36.41, 44.19), ('arrow', 44.19, 53.44), ('ship', 53.44, 60.30),
    ('cube', 60.30, 67.19), ('ufo', 67.19, 75.58), ('ball', 75.58, 79.34), ('arrow', 79.34, 81.83),
    ('cube', 81.83, 83.00),
]
DEADLOCKED = [  # Deadlocked
    ('cube', 0.00, 14.22, 1), ('arrow', 14.22, 17.90, 1), ('ship', 17.90, 21.38, 2), ('ball', 21.38, 24.95, 1),
    ('ufo', 24.95, 28.51, 1), ('cube', 28.51, 35.48, 1), ('ball', 35.48, 42.72, 1), ('cube', 42.72, 49.85, 2),
    ('arrow', 49.85, 56.89, 1), ('ship', 56.89, 63.82, 2), ('ufo', 63.82, 71.27, 1), ('arrow', 71.27, 74.59, 2),
    ('cube', 74.59, 78.35, 0.5), ('ship', 78.35, 85.42, 2), ('ball', 85.42, 88.97, 1), ('ufo', 88.97, 92.48, 1),
    ('cube', 92.48, 99.52, 1),
]
# Beat accents of Deadlocked (seconds), where RobTop pulses the screen.
DEADLOCKED_BEATS = [2.43, 2.62, 15.14, 16.04, 16.94, 24.18, 24.48, 30.14, 32.02, 33.86, 35.42, 37.17, 38.85, 39.93,
                    40.63, 41.17, 41.64, 42.02, 42.18, 42.32, 42.47, 44.45, 46.28, 48.02, 51.63, 53.48, 55.19, 73.86,
                    94.13, 95.81, 97.5]

TOE_PALETTES = {
    'cube': {'bg': (76, 18, 120), 'ground': (120, 30, 160), 'ground2': (60, 10, 90), 'obj': (255, 255, 255)},
    'ship': {'bg': (20, 40, 140), 'ground': (40, 70, 200), 'ground2': (20, 30, 100), 'obj': (200, 240, 255)},
    'ufo': {'bg': (140, 20, 90), 'ground': (190, 40, 120), 'ground2': (90, 10, 60), 'obj': (255, 220, 250)},
    'ball': {'bg': (20, 110, 120), 'ground': (30, 160, 150), 'ground2': (10, 70, 70), 'obj': (230, 255, 250)},
    'arrow': {'bg': (110, 55, 20), 'ground': (150, 80, 30), 'ground2': (80, 40, 15), 'obj': (255, 220, 170)},
}
DEAD_PALETTES = {
    'cube': {'bg': (20, 4, 8), 'ground': (60, 8, 16), 'ground2': (30, 4, 8), 'obj': (255, 60, 60)},
    'ship': {'bg': (8, 4, 24), 'ground': (30, 10, 70), 'ground2': (15, 5, 35), 'obj': (170, 110, 255)},
    'ufo': {'bg': (4, 20, 24), 'ground': (10, 60, 70), 'ground2': (5, 30, 35), 'obj': (90, 255, 230)},
    'ball': {'bg': (24, 16, 4), 'ground': (80, 50, 10), 'ground2': (40, 25, 5), 'obj': (255, 200, 60)},
    'arrow': {'bg': (40, 18, 6), 'ground': (110, 55, 18), 'ground2': (55, 27, 9), 'obj': (255, 170, 90)},
}


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    preview = '--preview' in sys.argv
    toe_sections = [Section(m, a, b, 1, {}) for m, a, b in TOE]
    dead_sections = [Section(m, a, b, v, {}) for m, a, b, v in DEADLOCKED]
    builds = [
        ('Teoria Tropical', 'teoria-tropical', 11, toe_sections, TOE_PALETTES, False, 'toe', (),
         'Fase original da GeoBR na musica Theory of Everything: cubo, nave, UFO, bola e dois trechos de Arrow.'),
        ('Bloqueio Total', 'bloqueio-total', 19, dead_sections, DEAD_PALETTES, True, 'dead', DEADLOCKED_BEATS,
         'Fase original da GeoBR na musica Deadlocked: tres trechos de Arrow, velocidades 0.5x a 2x.'),
    ]
    for name, slug, song, sections, pal, hard, seed, pulses, desc in builds:
        lv, cols = design(name, song, sections, pal, hard, seed, pulses)
        string, count = level_string(lv, pal, sections[0].mode)
        (out / f'geobr-{slug}.gmd').write_text(gmd(name, string, desc, song, count), encoding='utf-8')
        print(f'== {name}: {count} objects, {cols[-1][1]} columns')
        for line in lv.report: print('  ' + line)
        if preview: render(lv, out / f'preview-{slug}.png', cols)


def render(lv, path, cols):
    from PIL import Image, ImageDraw
    width = cols[-1][1] + 2
    rows_per_line = 12
    seg = 220
    lines = math.ceil(width / seg)
    S = 6
    img = Image.new('RGB', (seg * S + 20, lines * (rows_per_line * S + 14) + 10), (16, 16, 26))
    d = ImageDraw.Draw(img)
    for li in range(lines):
        oy = 10 + li * (rows_per_line * S + 14) + rows_per_line * S
        c_off = li * seg
        def at(c, r): return (10 + (c - c_off) * S, oy - (r + 1) * S)
        d.line([(10, oy), (10 + seg * S, oy)], fill=(200, 200, 200))
        for (c, r) in lv.solids:
            if c_off <= c < c_off + seg: x, y = at(c, r); d.rectangle([x, y, x + S - 1, y + S - 1], fill=(70, 120, 230))
        for (c, r), dn in lv.spikes.items():
            if c_off <= c < c_off + seg:
                x, y = at(c, r)
                d.polygon([(x, y + S), (x + S, y + S), (x + S / 2, y)] if not dn else [(x, y), (x + S, y), (x + S / 2, y + S)], fill=(240, 70, 70))
        for (c, r) in lv.pads:
            if c_off <= c < c_off + seg: x, y = at(c, r); d.rectangle([x, y + S - 2, x + S, y + S], fill=(255, 230, 60))
        for (c, r) in lv.orbs:
            if c_off <= c < c_off + seg: x, y = at(c, r); d.ellipse([x, y, x + S, y + S], fill=(255, 230, 60))
        for c0, c1 in cols:
            if c_off <= c0 < c_off + seg: x, _ = at(c0, 0); d.line([(x, oy - rows_per_line * S), (x, oy)], fill=(120, 255, 120))
    img.save(path)


if __name__ == '__main__':
    main()
