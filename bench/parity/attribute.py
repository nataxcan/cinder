#!/usr/bin/env python3
"""Attribute per-stage block mismatches to the stage that introduced them.

Runs the generator once per stage set (terrain / +surface / +carvers /
+features) against the vanilla reference and prints, for each stage, the
mismatch pairs that stage changed. That points the biggest remaining gap at a
specific stage instead of at the pipeline as a whole.
"""
from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
CINDER_GEN = os.environ.get("CINDER_GEN", "/home/nataxcan/.cache/cinder-core/cinder-gen")

spec = importlib.util.spec_from_file_location("diffmod", HERE / "diff.py")
diffmod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diffmod)

GRID = int(os.environ.get("PARITY_GRID", "8"))
OX = int(os.environ.get("PARITY_OX", "16"))
OZ = int(os.environ.get("PARITY_OZ", "8"))
REGION = Path(
    os.environ.get("PARITY_REGION", "/home/nataxcan/dev/ref-canonical26.2/world/region")
)

STAGES = [
    ("terrain", "surface,carvers,features"),
    ("surface", "carvers,features"),
    ("carvers", "features"),
    ("features", ""),
]


def run(skip: str) -> Path:
    dump = Path(os.environ.get("PARITY_WORK", "/home/nataxcan/.cache/cinder-parity"))
    subprocess.run(["rm", "-rf", str(dump)], check=True)
    dump.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env.update(
        {
            "CINDER_GRID": str(GRID),
            "CINDER_ORIGIN_X": str(OX),
            "CINDER_ORIGIN_Z": str(OZ),
            "CINDER_THREADS": os.environ.get("PARITY_THREADS", "8"),
            "CINDER_DUMP": str(dump),
            "CINDER_SKIP_STAGES": skip,
        }
    )
    gen = subprocess.run([CINDER_GEN], env=env, capture_output=True, text=True)
    if gen.returncode != 0:
        raise SystemExit("generator failed: " + gen.stderr[-2000:])
    return dump / "dump.bin"


def pairs(dump_path: Path) -> Counter:
    dump = diffmod.Dump(dump_path)
    counter: Counter = Counter()
    for cz in range(OZ, OZ + GRID):
        for cx in range(OX, OX + GRID):
            dblocks, _ = dump.chunk(cx, cz)
            chunk = diffmod.regions.read_chunk(REGION, cx, cz)
            # both sides into one shared name table, then compare ids
            tab = diffmod.NameTable()
            dump_ids = tab.table_for(dump.block_names)
            ref_ids = tab.table_for(chunk.block_names)
            a = [dump_ids[i] for i in dblocks]
            b = [ref_ids[i] for i in chunk.blocks]
            names = tab.names
            for x, y in zip(a, b):
                if x != y:
                    counter[(names[x], names[y])] += 1
    return counter


def main() -> int:
    prev: Counter = Counter()
    for label, skip in STAGES:
        cur = pairs(run(skip))
        delta = cur - prev
        introduced = sum(c for c in delta.values() if c > 0)
        fixed = -sum(c for c in delta.values() if c < 0)
        print(f"== {label}: {sum(cur.values()):>9d} mismatching (net {introduced - fixed:+d})")
        for (a, b), c in delta.most_common(6):
            if c > 0:
                print(f"   {c:>8d}  {a} -> {b}")
        prev = cur
    return 0


if __name__ == "__main__":
    sys.exit(main())
