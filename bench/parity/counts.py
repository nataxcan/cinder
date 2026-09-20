#!/usr/bin/env python3
"""Compare per-block-name totals between a Cinder dump and the vanilla reference.

Position-scatter vs missing/extra blocks: if the totals agree but the
positions do not, the feature RNG stream differs; if the totals differ, the
feature logic itself differs.
"""
from __future__ import annotations

import importlib.util
import os
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("diffmod", HERE / "diff.py")
diffmod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diffmod)

DUMP = Path(os.environ.get("PARITY_DUMP", "/home/nataxcan/.cache/cinder-parity/dump.bin"))
REGION = Path(
    os.environ.get("PARITY_REGION", "/home/nataxcan/dev/ref-canonical26.2/world/region")
)
GRID = int(os.environ.get("PARITY_GRID", "8"))
OX = int(os.environ.get("PARITY_OX", "16"))
OZ = int(os.environ.get("PARITY_OZ", "8"))
TOP = int(os.environ.get("PARITY_TOP", "24"))

NAMES = [
    "minecraft:stone",
    "minecraft:deepslate",
    "minecraft:tuff",
    "minecraft:granite",
    "minecraft:diorite",
    "minecraft:andesite",
    "minecraft:dirt",
    "minecraft:gravel",
    "minecraft:coal_ore",
    "minecraft:deepslate_coal_ore",
    "minecraft:iron_ore",
    "minecraft:deepslate_iron_ore",
    "minecraft:copper_ore",
    "minecraft:deepslate_copper_ore",
    "minecraft:diamond_ore",
    "minecraft:deepslate_diamond_ore",
    "minecraft:oak_log",
    "minecraft:oak_leaves",
    "minecraft:short_grass",
    "minecraft:leaf_litter",
    "minecraft:glow_lichen",
    "minecraft:air",
]


def main() -> int:
    dump = diffmod.Dump(DUMP)
    mine: Counter = Counter()
    theirs: Counter = Counter()
    for cz in range(OZ, OZ + GRID):
        for cx in range(OX, OX + GRID):
            dblocks, _ = dump.chunk(cx, cz)
            names = dump.block_names
            for i in dblocks:
                mine[names[i]] += 1
            chunk = diffmod.regions.read_chunk(REGION, cx, cz)
            for i in chunk.blocks:
                theirs[chunk.block_names[i]] += 1
    print(f"{'block':38s} {'cinder':>10s} {'vanilla':>10s} {'delta':>10s}")
    for name in NAMES:
        a, b = mine[name], theirs[name]
        print(f"{name:38s} {a:>10d} {b:>10d} {a - b:>+10d}")
    print()
    print("largest absolute totals delta (all names):")
    keys = set(mine) | set(theirs)
    for name, d in sorted(((k, mine[k] - theirs[k]) for k in keys), key=lambda kv: -abs(kv[1]))[:TOP]:
        print(f"  {d:>+9d}  {name}  (cinder {mine[name]}, vanilla {theirs[name]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
