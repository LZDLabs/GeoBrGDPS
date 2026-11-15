#!/usr/bin/env python3
"""Companion mod package check: geobr.gdps (separate GDPS save + redirect)."""
import json, struct, sys, zipfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tests'))
from elf_needed import needed, undeclared_mod_libs
f = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'dist/geobr.gdps.geode'
with zipfile.ZipFile(f) as z:
    assert z.testzip() is None
    m = json.loads(z.read('mod.json'))
    assert m['id'] == 'geobr.gdps' and m['early-load'] is True and not m.get('dependencies'), 'early-load, no dependencies'
    assert m['settings']['server']['default'] == 'Oficial', 'starts on the official server'
    so = [n for n in z.namelist() if n.endswith('.so')]
    assert so == ['geobr.gdps.android64.so'], so
    elf = z.read(so[0])
    assert elf[:4] == b'\x7fELF' and struct.unpack('<H', elf[18:20])[0] == 183, 'AArch64 ELF'
    assert b'_ZN8GManager5setupEv' in elf and b'geobr-gdps/' in elf, 'save-file hook (GManager::setup) present'
    libs = needed(elf)
    bad = undeclared_mod_libs(elf, m.get('dependencies'))
    assert not bad, f'.so needs libraries of mods that are not dependencies (dlopen fails on the device): {bad}'
print(f'PASS: {f.name}: geobr.gdps {m["version"]}, early-load, no dependencies, AArch64, save-folder hook, NEEDED = {libs}')
