#!/usr/bin/env python3
"""Compare the *carved* volume of one chunk: cinder-carved vs vanilla-carved.

    python3 bench/parity/carve_diff.py --cx 20 --cz 11

Cinder side  : dump with carvers on (--dump-on) minus dump with carvers off (--dump-off)
Vanilla side : ref26.2 (carvers on) minus ref-carvers-off26.2 (carvers off), both features-off
Prints set sizes, the intersection and the Y profile of the symmetric difference,
which shows whether the tunnels are shaped or placed differently.
"""
from __future__ import annotations

import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import diff as diffmod  # noqa: E402
import regions  # noqa: E402

SOLID = {"minecraft:air"}  # anything not air is "solid" for this purpose


def carved_from_dumps(on_path: str, off_path: str, cx: int, cz: int):
    on = diffmod.Dump(on_path)
    off = diffmod.Dump(off_path)
    cb_on, _ = on.chunk(cx, cz)
    cb_off, _ = off.chunk(cx, cz)
    names = on.block_names
    carved = set()
    for i in range(len(cb_on)):
        if names[cb_on[i]] == "minecraft:air" and names[cb_off[i]] != "minecraft:air":
            carved.add(i)
    return carved, names


def carved_from_regions(on_dir: str, off_dir: str, cx: int, cz: int):
    on = regions.read_chunk(on_dir, cx, cz)
    off = regions.read_chunk(off_dir, cx, cz)
    carved = set()
    for i in range(len(on.blocks)):
        if on.block_names[on.blocks[i]] == "minecraft:air" and off.block_names[off.blocks[i]] != "minecraft:air":
            carved.add(i)
    return carved


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cx", type=int, required=True)
    ap.add_argument("--cz", type=int, required=True)
    ap.add_argument("--dump-on", default="/tmp/d32/dump.bin")
    ap.add_argument("--dump-off", default="/tmp/dnc/dump.bin")
    ap.add_argument("--ref-on", default="/home/nataxcan/dev/ref26.2/world/region")
    ap.add_argument("--ref-off", default="/home/nataxcan/dev/ref-carvers-off26.2/world/region")
    args = ap.parse_args()

    ci, _ = carved_from_dumps(args.dump_on, args.dump_off, args.cx, args.cz)
    va = carved_from_regions(args.ref_on, args.ref_off, args.cx, args.cz)
    print(f"chunk ({args.cx},{args.cz}): cinder carved={len(ci)} vanilla carved={len(va)}")
    print(f"  intersection={len(ci & va)}  cinder-only={len(ci - va)}  vanilla-only={len(va - ci)}")

    def y_of(i: int) -> int:
        return i // 256 - 64

    prof_c = collections.Counter(y_of(i) for i in (ci - va))
    prof_v = collections.Counter(y_of(i) for i in (va - ci))
    print("  Y profile of the symmetric difference (y: cinder-only / vanilla-only):")
    for y in range(64, -65, -1):
        a, b = prof_c.get(y, 0), prof_v.get(y, 0)
        if a or b:
            print(f"    y={y:5d}  {a:5d} / {b:5d}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
