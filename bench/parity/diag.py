#!/usr/bin/env python3
"""Diagnose where a Cinder dump and the vanilla reference disagree.

    python3 bench/parity/diag.py --dump D.bin --region-dir DIR --cx 20 --cz 11

Prints the mismatch profile by Y level, by block pair, and the columns with the
most mismatches, so a localized carver/aquifer/surface difference is visible.
"""
from __future__ import annotations

import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import diff as diffmod  # noqa: E402
import regions  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", required=True)
    ap.add_argument("--region-dir", required=True)
    ap.add_argument("--cx", type=int, required=True)
    ap.add_argument("--cz", type=int, required=True)
    ap.add_argument("--top", type=int, default=12)
    args = ap.parse_args()

    dump = diffmod.Dump(args.dump)
    ref = regions.read_chunk(args.region_dir, args.cx, args.cz)

    cinder_blocks, cinder_biomes = dump.chunk(args.cx, args.cz)
    ref_blocks = ref.blocks  # 16x384x16 name indices
    names = dump.block_names
    ref_names = ref.block_names

    by_y = collections.Counter()
    pairs = collections.Counter()
    by_column = collections.Counter()
    for y in range(-64, 320):
        base = (y + 64) * 256
        for z in range(16):
            for x in range(16):
                c = names[cinder_blocks[base + z * 16 + x]]
                v = ref_names[ref_blocks[base + z * 16 + x]]
                if c != v:
                    by_y[y] += 1
                    pairs[(c, v)] += 1
                    by_column[(x, z)] += 1

    total = sum(by_y.values())
    print(f"chunk ({args.cx},{args.cz}): {total} mismatching positions")
    print("mismatches by Y:")
    for y in range(320, -65, -1):
        if by_y.get(y):
            print(f"  y={y:5d}  {by_y[y]:6d}  {'#' * min(60, by_y[y] // 20 + 1)}")
    print("top pairs:")
    for (c, v), n in pairs.most_common(args.top):
        print(f"  {n:7d}  cinder={c:30s} vanilla={v}")
    print("worst columns (x,z): count")
    for (x, z), n in by_column.most_common(10):
        print(f"  ({x:2d},{z:2d}) {n}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
