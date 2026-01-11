#!/usr/bin/env python3
import os
import sys


def unpack_padded_elf(in_path: str, out_path: str) -> int:
    with open(in_path, "rb") as f:
        data = f.read()

    magic = b"\x7fELF"
    if data.startswith(magic):
        # Already a normal ELF; just copy.
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        with open(out_path, "wb") as out:
            out.write(data)
        return 0

    off = data.find(magic)
    if off < 0:
        print(f"ERROR: no ELF header found in {in_path}", file=sys.stderr)
        return 2

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as out:
        out.write(data[off:])

    return 0


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("Usage: unpack_padded_elf.py <in.so> <out.so>", file=sys.stderr)
        return 2

    in_path, out_path = argv[1], argv[2]
    if not os.path.isfile(in_path):
        print(f"ERROR: input not found: {in_path}", file=sys.stderr)
        return 2

    return unpack_padded_elf(in_path, out_path)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
