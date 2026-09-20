#!/usr/bin/env python3
"""Cluster one block name into blobs in a chunk and compare centres.

If the two sides place the same number of blobs at scattered centres, the
feature RNG stream differs. If the counts differ, the feature logic differs.
"""
from __future__ import annotations

import importlib.util
import os
import sys
from collections import deque
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("diffmod", HERE / "diff.py")
diffmod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diffmod)

DUMP = Path(os.environ.get("PARITY_DUMP", "/home/nataxcan/.cache/cinder-parity/dump.bin"))
REGION = Path(
    os.environ.get("PARITY_REGION", "/home/nataxcan/dev/ref-canonical26.2/world/region")
)
CX = int(os.environ.get("PARITY_CX", "16"))
CZ = int(os.environ.get("PARITY_CZ", "8"))
BLOCK = os.environ.get("PARITY_BLOCK", "minecraft:andesite")
MIN_Y = -64
HEIGHT = 384


def blobs(ids, names, want):
    cells = set()
    for i, v in enumerate(ids):
        if names[v] == want:
            y = MIN_Y + i // 256
            z = (i % 256) // 16
            x = i % 16
            cells.add((x, y, z))
    seen = set()
    out = []
    for c in cells:
        if c in seen:
            continue
        q = deque([c])
        seen.add(c)
        n = 0
        sx = sy = sz = 0
        while q:
            x, y, z = q.popleft()
            n += 1
            sx += x
            sy += y
            sz += z
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for dz in (-1, 0, 1):
                        nc = (x + dx, y + dy, z + dz)
                        if nc in cells and nc not in seen:
                            seen.add(nc)
                            q.append(nc)
        out.append((n, sx / n, sy / n, sz / n))
    out.sort(key=lambda t: -t[0])
    return out


def main() -> int:
    dump = diffmod.Dump(DUMP)
    dblocks, _ = dump.chunk(CX, CZ)
    chunk = diffmod.regions.read_chunk(REGION, CX, CZ)
    mine = blobs(dblocks, dump.block_names, BLOCK)
    theirs = blobs(chunk.blocks, chunk.block_names, BLOCK)
    print(f"{BLOCK} in chunk ({CX},{CZ})")
    print(f"  cinder: {len(mine)} blobs, {sum(b[0] for b in mine)} blocks")
    for n, x, y, z in mine[:12]:
        print(f"    size={n:4d} centre=({x:6.1f},{y:7.1f},{z:6.1f})")
    print(f"  vanilla: {len(theirs)} blobs, {sum(b[0] for b in theirs)} blocks")
    for n, x, y, z in theirs[:12]:
        print(f"    size={n:4d} centre=({x:6.1f},{y:7.1f},{z:6.1f})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
