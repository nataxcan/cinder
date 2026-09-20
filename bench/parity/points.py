#!/usr/bin/env python3
"""Deterministic point sets for the vanilla-vs-Cinder density probes.

Points are aligned to the density cell grid (x,z multiples of 4, y multiples of
8) so the comparison is meaningful for nodes vanilla evaluates through
NoiseChunk's flat caches and interpolators, which quantize horizontally.
"""
from __future__ import annotations

import random
import sys


def df_points(count: int = 200, seed: int = 7, spread: int = 4096) -> list[tuple[int, int, int]]:
    rng = random.Random(seed)
    pts = []
    while len(pts) < count:
        x = rng.randint(-spread, spread) // 4 * 4
        z = rng.randint(-spread, spread) // 4 * 4
        y = rng.randint(-64, 320) // 8 * 8
        pts.append((x, y, z))
    return pts


def biome_points(count: int = 120, seed: int = 11, spread: int = 512) -> list[tuple[int, int, int]]:
    """Quart coordinates (what Climate.Sampler takes)."""
    rng = random.Random(seed)
    pts = []
    for _ in range(count):
        pts.append((rng.randint(-spread, spread), rng.randint(-16, 76), rng.randint(-spread, spread)))
    return pts


def main() -> int:
    kind = sys.argv[1] if len(sys.argv) > 1 else "df"
    pts = biome_points() if kind == "biome" else df_points()
    for x, y, z in pts:
        print(x, y, z)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
