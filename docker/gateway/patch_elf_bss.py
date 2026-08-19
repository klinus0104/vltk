#!/usr/bin/env python3
import struct
import sys

PT_LOAD = 1
PF_W = 2


def patch(path: str) -> None:
    with open(path, "r+b") as f:
        data = bytearray(f.read())
        if data[:4] != b"\x7fELF":
            raise SystemExit(f"{path}: not an ELF file")
        if data[4] != 1 or data[5] != 1:
            raise SystemExit(f"{path}: expected ELF32 little-endian")

        e_phoff = struct.unpack_from("<I", data, 28)[0]
        e_phentsize = struct.unpack_from("<H", data, 42)[0]
        e_phnum = struct.unpack_from("<H", data, 44)[0]
        changed = 0

        for i in range(e_phnum):
            off = e_phoff + i * e_phentsize
            p_type, _, _, _, p_filesz, p_memsz, p_flags, _ = struct.unpack_from("<IIIIIIII", data, off)
            if p_type == PT_LOAD and p_memsz > p_filesz and (p_flags & PF_W) == 0:
                struct.pack_into("<I", data, off + 24, p_flags | PF_W)
                changed += 1

        if changed:
            f.seek(0)
            f.write(data)
            f.truncate()
        print(f"[patch-elf-bss] {path}: patched {changed} PT_LOAD segment(s)")


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit("usage: patch-elf-bss FILE [FILE...]")
    for path in sys.argv[1:]:
        patch(path)


if __name__ == "__main__":
    main()
