#!/usr/bin/env python3
"""Per-stage block agreement against the vanilla reference.

Generates the canonical 8x8 grid with one stage set enabled at a time and
reports block agreement for each, so a regression can be attributed to the
stage that caused it.
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CINDER_GEN = os.environ.get("CINDER_GEN", "/home/nataxcan/.cache/cinder-core/cinder-gen")
ROOT = HERE.parents[1]

STAGES = [
    ("terrain (no surface, no carvers, no features)", "surface,carvers,features"),
    ("+ surface", "carvers,features"),
    ("+ carvers", "features"),
    ("+ features", ""),
]

GRID = os.environ.get("PARITY_GRID", "8")
OX = os.environ.get("PARITY_OX", "16")
OZ = os.environ.get("PARITY_OZ", "8")
REGION = os.environ.get(
    "PARITY_REGION", "/home/nataxcan/dev/ref-canonical26.2/world/region"
)


def run_stage(skip: str) -> tuple[str, str]:
    dump = Path(os.environ.get("PARITY_WORK", "/home/nataxcan/.cache/cinder-parity"))
    subprocess.run(["rm", "-rf", str(dump)], check=True)
    dump.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env.update(
        {
            "CINDER_GRID": GRID,
            "CINDER_ORIGIN_X": OX,
            "CINDER_ORIGIN_Z": OZ,
            "CINDER_THREADS": os.environ.get("PARITY_THREADS", "8"),
            "CINDER_DUMP": str(dump),
            "CINDER_SKIP_STAGES": skip,
        }
    )
    gen = subprocess.run([CINDER_GEN], env=env, capture_output=True, text=True)
    if gen.returncode != 0:
        raise SystemExit(f"generator failed: {gen.stderr[-2000:]}")
    checksum = next(
        (l.split("=", 1)[1] for l in gen.stdout.splitlines() if l.startswith("checksum=")),
        "?",
    )
    diff = subprocess.run(
        [
            sys.executable,
            str(HERE / "diff.py"),
            "--dump",
            str(dump / "dump.bin"),
            "--region-dir",
            REGION,
            "--cx0",
            OX,
            "--cz0",
            OZ,
            "--nx",
            GRID,
            "--nz",
            GRID,
            "--no-per-chunk",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    agree = next(
        (l for l in diff.stdout.splitlines() if l.startswith("block agreement")), "?"
    )
    return checksum, agree


def main() -> int:
    print(f"grid={GRID} origin=({OX},{OZ}) region={REGION}")
    for label, skip in STAGES:
        checksum, agree = run_stage(skip)
        print(f"{label:48s} checksum={checksum:>12s}  {agree}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
