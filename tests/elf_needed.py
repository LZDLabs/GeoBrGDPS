#!/usr/bin/env python3
"""DT_NEEDED of a 64-bit little-endian ELF (the libraries the loader must find).

Used by the package checks: a mod's .so may only need system libraries,
libcocos2dcpp/libfmod, Geode itself and the .so of mods declared in its own
mod.json "dependencies". Anything else fails at dlopen on the device
("library ... not found: needed by ...").
"""
import struct

SYSTEM_LIBS = {
    "libc.so", "libm.so", "libdl.so", "liblog.so", "libz.so", "libandroid.so",
    "libGLESv2.so", "libEGL.so", "libOpenSLES.so", "libc++_shared.so",
    "libcocos2dcpp.so", "libfmod.so", "Geode.android64.so",
}


def needed(elf: bytes) -> list[str]:
    assert elf[:4] == b"\x7fELF" and elf[4] == 2 and elf[5] == 1, "64-bit little-endian ELF"
    phoff = struct.unpack_from("<Q", elf, 0x20)[0]
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    loads, dynamic = [], None
    for i in range(phnum):
        p_type, _flags, p_offset, p_vaddr, _paddr, p_filesz = struct.unpack_from("<IIQQQQ", elf, phoff + i * phentsize)
        if p_type == 1:
            loads.append((p_vaddr, p_offset, p_filesz))
        elif p_type == 2:
            dynamic = (p_offset, p_filesz)
    assert dynamic, "no PT_DYNAMIC"

    def file_offset(vaddr: int) -> int:
        for va, off, size in loads:
            if va <= vaddr < va + size:
                return off + (vaddr - va)
        raise ValueError(f"address {vaddr:#x} outside PT_LOAD")

    names, strtab = [], None
    off, end = dynamic[0], dynamic[0] + dynamic[1]
    while off + 16 <= end:
        tag, val = struct.unpack_from("<qQ", elf, off)
        off += 16
        if tag == 0:
            break
        if tag == 1:
            names.append(val)
        elif tag == 5:
            strtab = file_offset(val)
    assert strtab is not None, "no DT_STRTAB"
    out = []
    for n in names:
        start = strtab + n
        out.append(elf[start:elf.index(b"\0", start)].decode())
    return out


def undeclared_mod_libs(elf: bytes, dependencies) -> list[str]:
    """Mod libraries (<id>.android64.so) the .so needs without declaring the mod."""
    declared = set(dependencies or {})
    bad = []
    for lib in needed(elf):
        if lib in SYSTEM_LIBS:
            continue
        mod_id = lib[: -len(".android64.so")] if lib.endswith(".android64.so") else None
        if mod_id is None or mod_id not in declared:
            bad.append(lib)
    return bad


if __name__ == "__main__":
    import sys
    print("\n".join(needed(open(sys.argv[1], "rb").read())))
