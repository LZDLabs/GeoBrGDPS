#!/usr/bin/env python3
"""Import libraries for Windows builds made on Linux.

The Geode SDK ships no Geode.lib (it comes with the Windows loader binaries),
and the dependency hjfod.gmd-api has no Windows .lib staged here. An import
library only maps names to a DLL, so it can be generated from the exact
(mangled) symbols the mod imports: the link of each build dir is re-run with
/demangle:no /errorlimit:0, every undefined __imp_ symbol is collected and
assigned to its DLL (gmd namespace -> hjfod.gmd-api.dll, everything else ->
Geode.dll, which exports all GEODE_DLL declarations), and llvm-dlltool writes
the .lib files. Undefined symbols that are not imports are reported as errors.

Usage: win_import_libs.py <geode.lib out> <gmd-api.lib out> <build dir>...
"""
import re, shlex, subprocess, sys
from pathlib import Path

LLVM = "/usr/lib/llvm-19/bin"
geode_out, gmd_out, builds = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3:]
state = Path("/opt/tc/win/imports.txt")
def is_data(sym: str) -> bool:
    if sym.startswith("??_7"):  # vftable
        return True
    m = re.search(r"@@([0-9A-Z])", sym)
    return bool(m) and m.group(1) in "23"

imports = set(state.read_text().split()) if state.exists() else set()
problems = set()

for build in builds:
    commands = subprocess.run(["ninja", "-C", build, "-t", "commands"], capture_output=True, text=True).stdout.splitlines()
    link = next((c for c in reversed(commands) if "vs_link_dll" in c and ".dll" in c), None)
    if not link:
        sys.exit(f"no DLL link command in {build}")
    link = link.split("&&", 1)[1].rsplit("&&", 1)[0].strip()
    args = shlex.split(link)
    args = args[args.index("--") + 1:] + ["/demangle:no", "/errorlimit:0"]
    out = subprocess.run(args, cwd=build, capture_output=True, text=True)
    for name in re.findall(r"undefined symbol: (\S+)", out.stdout + out.stderr):
        if name.startswith("__imp_"):
            imports.add(name[len("__imp_"):])
        elif re.search(r"@(geode|matjson|gmd)@@", name) and not is_data(name):
            # Called directly (e.g. from a vtable): the import library's
            # call stub with the plain name resolves it into the DLL.
            imports.add(name)
        else:
            problems.add(name)

state.write_text("\n".join(sorted(imports)) + "\n")

def write(lib: Path, dll: str, syms):
    deff = lib.with_suffix(".def")
    lines = [f"LIBRARY {dll}", "EXPORTS"] + [f"  {s}{' DATA' if is_data(s) else ''}" for s in sorted(syms)]
    deff.write_text("\n".join(lines) + "\n")
    subprocess.run([f"{LLVM}/llvm-dlltool", "-m", "i386:x86-64", "-d", str(deff), "-l", str(lib)], check=True)
    print(f"{lib.name}: {len(syms)} symbols -> {dll}")

gmd = {s for s in imports if "@gmd@@" in s}
write(gmd_out, "hjfod.gmd-api.dll", gmd or {"geobr_placeholder"})
write(geode_out, "Geode.dll", (imports - gmd) or {"geobr_placeholder"})
if problems:
    print("NOT imports (must be fixed in the build):", *sorted(problems)[:40], sep="\n  ")
    sys.exit(2)
