#!/usr/bin/env python3
"""How much of the residual disagreement is structure-caused?

Structures are placed during the FEATURES stage, so in vanilla the feature pass
sees structure blocks. Cinder has no structures, so any chunk containing one
differs twice over: the structure's own blocks, and every feature decision that
looked at them. This splits the mismatches of each chunk by whether the vanilla
chunk contains structure blocks at all (tuff bricks / waxed copper / trial
spawners are trial-chamber-only in the overworld).
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

STRUCTURE_MARKERS = (
    "tuff_bricks",
    "chiseled_tuff_bricks",
    "polished_tuff",
    "waxed_copper_block",
    "waxed_oxidized_copper",
    "trial_spawner",
    "vault",
)


def main() -> int:
    dump = diffmod.Dump(DUMP)
    inside = Counter()
    outside = Counter()
    chunks_with = 0
    chunks_without = 0
    for cz in range(OZ, OZ + GRID):
        for cx in range(OX, OX + GRID):
            dblocks, _ = dump.chunk(cx, cz)
            chunk = diffmod.regions.read_chunk(REGION, cx, cz)
            tab = diffmod.NameTable()
            a = [tab.table_for(dump.block_names)[i] for i in dblocks]
            b = [tab.table_for(chunk.block_names)[i] for i in chunk.blocks]
            names = tab.names
            has_structure = any(
                any(m in chunk.block_names[v] for m in STRUCTURE_MARKERS)
                for v in set(chunk.blocks)
            )
            if has_structure:
                chunks_with += 1
            else:
                chunks_without += 1
            target = inside if has_structure else outside
            for x, y in zip(a, b):
                if x != y:
                    target[(names[x], names[y])] += 1

    total = sum(inside.values()) + sum(outside.values())
    print(f"chunks with structure blocks: {chunks_with}, without: {chunks_without}")
    print(f"mismatches inside structure chunks: {sum(inside.values())}")
    print(f"mismatches outside structure chunks: {sum(outside.values())}")
    print(f"total: {total}")
    print()
    print("top mismatches OUTSIDE structure chunks (i.e. not explained by structures):")
    for (a, b), c in outside.most_common(12):
        print(f"  {c:>7d}  {a} -> {b}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
