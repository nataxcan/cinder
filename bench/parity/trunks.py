#!/usr/bin/env python3
"""Are the tree trunks where vanilla puts them?

Compares the *lowest* log position of every trunk column (a stand-in for the
placement position) between a Cinder dump and the reference world, per chunk.
If placements match but leaves do not, the bug is in the tree shape; if
placements differ, it is in the placement chain.
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

DUMP = Path(os.environ.get("PARITY_DUMP", "/home/nataxcan/.cache/can8/dump.bin"))
REGION = Path(
    os.environ.get("PARITY_REGION", "/home/nataxcan/dev/ref-canonical26.2/world/region")
)
GRID = int(os.environ.get("PARITY_GRID", "8"))
OX = int(os.environ.get("PARITY_OX", "16"))
OZ = int(os.environ.get("PARITY_OZ", "8"))
LOGS = os.environ.get(
    "PARITY_LOGS", "oak_log,birch_log,spruce_log,jungle_log,acacia_log,dark_oak_log,cherry_log"
).split(",")
MIN_Y = -64


def trunk_columns(ids, names, wanted):
    """(x, z) -> lowest log y, for every column that has a log of one of `wanted`."""
    out: dict[tuple[int, int, str], int] = {}
    for i, v in enumerate(ids):
        name = names[v].split(":")[-1]
        if name not in wanted:
            continue
        y = MIN_Y + i // 256
        z = (i % 256) // 16
        x = i % 16
        key = (x, z, name)
        if key not in out or y < out[key]:
            out[key] = y
    return out


def main() -> int:
    dump = diffmod.Dump(DUMP)
    want = set(LOGS)
    per_chunk = []
    tot_mine = tot_theirs = tot_same = 0
    for cz in range(OZ, OZ + GRID):
        for cx in range(OX, OX + GRID):
            dblocks, _ = dump.chunk(cx, cz)
            chunk = diffmod.regions.read_chunk(REGION, cx, cz)
            mine = trunk_columns(dblocks, dump.block_names, want)
            theirs = trunk_columns(chunk.blocks, chunk.block_names, want)
            same = len(set(mine) & set(theirs))
            tot_mine += len(mine)
            tot_theirs += len(theirs)
            tot_same += same
            if mine != theirs:
                per_chunk.append((cx, cz, len(mine), len(theirs), same))

    # horizontal agreement matters more than y: a tree one block up is a
    # different tree, but the interesting question is whether we picked the same
    # column at all.
    def columns_only(d):
        return {(x, z, k) for (x, z, k) in d}

    tot_col_same = 0
    for cz in range(OZ, OZ + GRID):
        for cx in range(OX, OX + GRID):
            dblocks, _ = dump.chunk(cx, cz)
            chunk = diffmod.regions.read_chunk(REGION, cx, cz)
            a = columns_only(trunk_columns(dblocks, dump.block_names, want))
            b = columns_only(trunk_columns(chunk.blocks, chunk.block_names, want))
            tot_col_same += len(a & b)

    print(f"trunk columns (lowest log per column) over {GRID}x{GRID} chunks")
    print(f"  cinder : {tot_mine}")
    print(f"  vanilla: {tot_theirs}")
    print(f"  same column, same lowest y: {tot_same}")
    print(f"  same column (ignoring y)  : {tot_col_same}")
    print(f"  cinder-only: {tot_mine - tot_col_same}, vanilla-only: {tot_theirs - tot_col_same}")
    print()
    if per_chunk:
        print(f"chunks with a different trunk set: {len(per_chunk)}")
        for cx, cz, m, v, s in sorted(per_chunk, key=lambda t: abs(t[2] - t[3]), reverse=True)[:10]:
            print(f"  ({cx},{cz}) cinder={m:3d} vanilla={v:3d} same={s:3d}")
    else:
        print("every chunk has the same trunk columns")
    return 0


if __name__ == "__main__":
    sys.exit(main())
