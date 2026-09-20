#!/usr/bin/env python3
"""Render bench/PUMPKIN.md from bench/world_results.json.

The Cinder-vs-Pumpkin rows come from `scripts/world_bench.py`; this reads the
JSON that run wrote, so the table can be regenerated without re-running the bench.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RESULTS = ROOT / "bench" / "world_results.json"
OUT = ROOT / "bench" / "PUMPKIN.md"


def main() -> int:
    d = json.loads(RESULTS.read_text())
    cinder = d["cinder"]
    pumpkin = d["pumpkin"]
    threads = [t for t in ("1", "4", "8", "16") if t in cinder and t in pumpkin]

    md = [
        "# World generation - Cinder vs Pumpkin",
        "",
        "Regenerate with `python3 scripts/world_bench.py`; the full write-up is in "
        "[`WORLD.md`](WORLD.md).",
        "",
        "Same box, same grid: **32x32 = 1024 chunks** around origin.",
        "",
        "- **Cinder**: `./cinder --threads N` printing `world ready` - the whole vanilla 26.2 "
        "stage list (biomes, density, aquifers, ore veins, surface rules, carvers, vanilla "
        "features, lighting) into memory, then the listener comes up.",
        "- **Pumpkin**: `pumpkin-gen 32 N`, `generate_single_chunk(..., Full)` - its own feature "
        "engine, in memory.",
        "",
        "| Threads | Cinder median | Cinder best | Pumpkin median | Pumpkin best | Pumpkin time / Cinder time (best; lower = Pumpkin faster) |",
        "| ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for t in threads:
        c = cinder[t]
        p = pumpkin[t]
        md.append(
            f"| {t} | {c['median_ms'] / 1000:.2f} s | {c['best_ms'] / 1000:.2f} s | "
            f"{p['median_ms'] / 1000:.2f} s | {p['best_ms'] / 1000:.2f} s | "
            f"{p['best_ms'] / c['best_ms']:.2f}x |"
        )
    md.append("")
    md.append("## The GPU row")
    md.append("")
    md.append("Cinder's generator is C behind one IO effect, so it is not a GPU target - Bend only")
    md.append("hands `!` (parallel call) to the device and nothing in the pipeline uses it. The")
    md.append("Bend-side heightmap microbench (`src/world.bend`) does run there, on a different")
    md.append("workload, so it is listed separately and left out of the ratios above:")
    md.append("")
    md.append("| Heightmap microbench | CPU 8 threads | GPU `--gpu 4GB` | speedup |")
    md.append("| --- | ---: | ---: | ---: |")
    md.append("| 32x32 chunks | 19 ms | ~2.4 s | 0.008x (device fixed cost dominates) |")
    md.append("| 256x256 = 65 536 chunks | 113.6 s | **59.2 s** | **1.92x** |")
    md.append("")
    md.append("Both modes produce identical checksums (16647010 and 1065330270). See")
    md.append("`bench/WORLD.md` for the toolchain and the WSL2 caveat.")
    md.append("")
    if "8" in cinder and "8" in pumpkin:
        c8, p8 = cinder["8"]["median_ms"], pumpkin["8"]["median_ms"]
        md.append(
            f"At the thread count both scale to (8): Cinder {c8 / 1000:.2f} s vs Pumpkin "
            f"{p8 / 1000:.2f} s, i.e. Pumpkin is **{c8 / p8:.2f}x faster**."
        )
    md.append("")
    OUT.write_text("\n".join(md) + "\n")
    print(f"wrote {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
