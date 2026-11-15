#!/usr/bin/env python3
"""Windows half of the packages: each .geode carries <id>.dll (PE32+ x64)
next to the Android .so, and the DLL imports only Windows/MSVC runtime DLLs,
GD's own DLLs, Geode.dll and the DLLs of mods declared in its mod.json.
Usage: package_win.py [package.geode ...]"""
import json, re, struct, subprocess, sys, tempfile, zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
READOBJ = "/usr/lib/llvm-19/bin/llvm-readobj"
SYSTEM = re.compile(r"^(kernel32|user32|gdi32|shell32|ole32|advapi32|ws2_32|opengl32|vcruntime140(_1)?|msvcp140(_1|_2|_atomic_wait)?|api-ms-win-[a-z0-9-]+)\.dll$", re.I)
GD = {"libcocos2d.dll", "libextensions.dll", "glew32.dll", "fmod.dll", "geode.dll"}
packages = [Path(p) for p in sys.argv[1:]] or [root / "dist/geobr.geobrmod.geode", root / "dist/geobr.gdps.geode"]

for f in packages:
    with zipfile.ZipFile(f) as z:
        m = json.loads(z.read("mod.json"))
        assert m["gd"].get("win") == "2.2081", f"{f.name}: mod.json declares Windows"
        dll = f'{m["id"]}.dll'
        assert dll in z.namelist() and f'{m["id"]}.android64.so' in z.namelist(), f"{f.name}: Android and Windows binaries"
        data = z.read(dll)
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[:2] == b"MZ" and data[pe:pe + 4] == b"PE\0\0" and struct.unpack_from("<H", data, pe + 4)[0] == 0x8664, f"{dll}: PE x64"
    with tempfile.NamedTemporaryFile(suffix=".dll") as tmp:
        tmp.write(data); tmp.flush()
        out = subprocess.run([READOBJ, "--coff-imports", tmp.name], capture_output=True, text=True, check=True).stdout
    imported = sorted(set(re.findall(r"Name: (\S+\.dll)", out, re.I)))
    declared = {f"{d}.dll".lower() for d in (m.get("dependencies") or {})}
    bad = [d for d in imported if not SYSTEM.match(d) and d.lower() not in GD and d.lower() not in declared]
    assert not bad, f"{dll} imports DLLs of undeclared mods: {bad}"
    print(f"PASS: {f.name}: {dll} PE x64, imports {', '.join(d for d in imported if not d.lower().startswith('api-ms-win'))}")
