#!/usr/bin/env python3
"""Where in Y do the mismatches live, per block pair?

Ore-blob scatter sits in the ore features' bands; vein-system differences sit in
the deepslate band (y < 0). Prints a Y histogram per mismatch pair so the
remaining feature gap can be attributed to a subsystem.
"""
from __future__ import annotations

import importlib.util
import os
import sys
from collections import Counter, defaultdict
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
MIN_Y = -64
WANT = os.environ.get("PARITY_BLOCKS", "andesite,diorite,granite,tuff,deepslate").split(",")


def main() -> int:
    dump = diffmod.Dump(DUMP)
    bands: dict[str, Counter] = defaultdict(Counter)
    totals: Counter = Counter()
    for cz in range(OZ, OZ + GRID):
        for cx in range(OX, OX + GRID):
            dblocks, _ = dump.chunk(cx, cz)
            chunk = diffmod.regions.read_chunk(REGION, cx, cz)
            tab = diffmod.NameTable()
            dump_ids = tab.table_for(dump.block_names)
            ref_ids = tab.table_for(chunk.block_names)
            names = tab.names
            for i, (a, b) in enumerate(zip([dump_ids[i] for i in dblocks],
                                           [ref_ids[i] for i in chunk.blocks])):
                if a == b:
                    continue
                an, bn = names[a], names[b]
                if not any(w in an or w in bn for w in WANT):
                    continue
                y = MIN_Y + i // 256
                band = f"{y // 16 * 16:+5d}"
                bands[f"{an} -> {bn}"][band] += 1
                totals[f"{an} -> {bn}"] += 1
    for pair, _ in totals.most_common(10):
        c = bands[pair]
        print(f"{pair}  total={totals[pair]}")
        print("   " + "  ".join(f"{b}:{c[b]}" for b in sorted(c, key=lambda s: -c[s])[:8]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
