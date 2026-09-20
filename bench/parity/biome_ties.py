#!/usr/bin/env python3
"""Explain Cinder/vanilla biome mismatches: are they ties in Climate.ParameterList?

    python3 bench/parity/biome_ties.py --dump /tmp/d32/dump.bin \
        --region-dir /home/nataxcan/dev/ref26.2/world/region --nx 32 --nz 32 [--limit 12]

For each mismatching 4x4x4 cell it prints Cinder's biome, vanilla's biome and the
fitness of both parameter points for the same climate target (evaluated by
Cinder's density-function engine, which is verified bit-identical to the jar).
Equal fitness means a genuine tie: vanilla breaks ties with the RTree's
thread-local "last result" candidate, Cinder with a per-chunk one, so the two can
legitimately pick different biomes there.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import diff as diffmod  # noqa: E402
import regions  # noqa: E402

HERE = Path(__file__).resolve().parent
PARAMS = json.loads((HERE / "biome_params.json").read_text())["entries"]
KEYS = ("temperature", "humidity", "continentalness", "erosion", "depth", "weirdness")


def fitness(point: dict, target: list[int]) -> int:
    total = 0
    for key, value in zip(KEYS, target):
        lo, hi = point[key]
        above = value - hi
        below = lo - value
        dist = above if above > 0 else max(below, 0)
        total += dist * dist
    total += point["offset"] ** 2
    return total


def collect_mismatches(dump_path: str, region_dir: str, nx: int, nz: int, limit: int):
    dump = diffmod.Dump(dump_path)
    out = []
    for cz in range(nz):
        for cx in range(nx):
            try:
                ref = regions.read_chunk(region_dir, cx, cz)
            except regions.ChunkMissing:
                continue
            _blocks, cinder_biomes = dump.chunk(cx, cz)
            for sec in range(24):
                for yi in range(4):
                    for zi in range(4):
                        for xi in range(4):
                            idx = sec * 64 + yi * 16 + zi * 4 + xi
                            c = dump.biome_names[cinder_biomes[idx]]
                            v = ref.biome_palette[ref.biomes[idx]]
                            if c != v:
                                qx = cx * 4 + xi
                                qy = (sec - 4) * 4 + yi
                                qz = cz * 4 + zi
                                out.append((qx, qy, qz, c, v))
                                if len(out) >= limit:
                                    return out, dump
    return out, dump


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", required=True)
    ap.add_argument("--region-dir", required=True)
    ap.add_argument("--nx", type=int, default=32)
    ap.add_argument("--nz", type=int, default=32)
    ap.add_argument("--limit", type=int, default=12)
    ap.add_argument("--probe", default=os.environ.get("CINDER_PROBE", os.path.expanduser("~/.cache/cinder-core/cinder-probe")))
    args = ap.parse_args()

    mismatches, _dump = collect_mismatches(args.dump, args.region_dir, args.nx, args.nz, args.limit)
    if not mismatches:
        print("no biome mismatches")
        return 0
    pts = "\n".join(f"{qx} {qy} {qz}" for qx, qy, qz, _, _ in mismatches) + "\n"
    probe = subprocess.run([args.probe, "--climate"], input=pts, capture_output=True, text=True)
    if probe.returncode != 0:
        print("probe failed:", probe.stderr.strip()[:400])
        return 2
    climates = {}
    for line in probe.stdout.splitlines():
        parts = line.split()
        if len(parts) == 9:
            climates[(int(parts[0]), int(parts[1]), int(parts[2]))] = [int(v) for v in parts[3:]]

    ties = 0
    for qx, qy, qz, c, v in mismatches:
        target = climates.get((qx, qy, qz))
        if not target:
            continue
        scored = sorted(((fitness(p, target), p["biome"]) for p in PARAMS))
        best = scored[0][0]
        fittest = {name for fit, name in scored if fit == best}
        c_fit = min(fit for fit, name in scored if name == c)
        v_fit = min(fit for fit, name in scored if name == v)
        tied = c_fit == v_fit
        ties += tied
        print(f"  quart=({qx},{qy},{qz}) cinder={c} (fit {c_fit}) vanilla={v} (fit {v_fit})"
              f" {'TIE' if tied else 'not a tie'}; fittest set size {len(fittest)}")
    print(f"{ties}/{len(mismatches)} mismatches are exact fitness ties")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
