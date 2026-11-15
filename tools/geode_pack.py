#!/usr/bin/env python3
"""Offline subset of `geode package new` (Geode CLI 3.9.0 semantics).

Used only when the official CLI is unavailable. It reproduces the layout the
CLI writes for mods that declare plain `resources.files` entries:

  mod.json, <id>.android64.so, logo.png/about.md/changelog.md/support.md,
  resources/<id>/<file name>   (files are flattened, as the CLI does)

Spritesheets, fonts and sprite scaling are rejected instead of being faked.
"""
import argparse
import json
import sys
import zipfile
from pathlib import Path

BINARY_SUFFIXES = (".android64.so", ".android32.so", ".ios.dylib", ".dylib", ".dll", ".so")


def build(root: Path, binaries: list[Path], output: Path) -> None:
    meta = json.loads((root / "mod.json").read_text(encoding="utf-8"))
    mod_id = meta["id"]
    resources = meta.get("resources", {})
    unsupported = [k for k in ("spritesheets", "fonts", "sprites", "folders", "libraries") if resources.get(k)]
    if unsupported:
        sys.exit(f"geode_pack: unsupported resource kinds {unsupported}; use the official Geode CLI")

    entries: dict[str, bytes] = {"mod.json": (root / "mod.json").read_bytes()}
    for name in ("logo.png", "about.md", "changelog.md", "support.md"):
        if (root / name).is_file():
            entries[name] = (root / name).read_bytes()
    for rel in resources.get("files", []):
        src = root / rel
        if not src.is_file():
            sys.exit(f"geode_pack: missing resource {rel}")
        target = f"resources/{mod_id}/{src.name}"
        if target in entries:
            sys.exit(f"geode_pack: duplicate flattened resource name {src.name}")
        entries[target] = src.read_bytes()
    for binary in binaries:
        suffix = next((s for s in BINARY_SUFFIXES if binary.name.endswith(s)), None)
        if suffix is None:
            sys.exit(f"geode_pack: unknown binary type {binary}")
        entries[mod_id + suffix] = binary.read_bytes()

    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(entries):
            archive.writestr(name, entries[name])
    print(f"geode_pack: wrote {output} ({len(entries)} entries)")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--binary", type=Path, nargs="*", default=[])
    parser.add_argument("--output", type=Path, required=True)
    args, _ = parser.parse_known_args()
    build(args.root, args.binary, args.output)


if __name__ == "__main__":
    main()
