#!/usr/bin/env python3
"""Structural inventory of the vanilla overworld noise settings embedded in embed.h.

Prints every JSON "type" used inside the density-function graph and inside the
surface rule tree, with counts, so a C port can check coverage.
"""
from __future__ import annotations

import collections
import json
import re
import sys
from pathlib import Path

EMBED = Path(__file__).resolve().parents[2] / "src" / "vanilla" / "embed.h"


def load_settings() -> dict:
    text = EMBED.read_text(encoding="utf-8", errors="replace")
    for key, raw in re.findall(r'^  \{"([^"]+)", "(.*)"\},$', text, re.M):
        if key == "settings:overworld":
            return json.loads(json.loads(f'"{raw}"'))
    raise SystemExit("settings:overworld not found")


def collect_types(node: object, out: collections.Counter, path: str = "") -> None:
    if isinstance(node, dict):
        t = node.get("type")
        if isinstance(t, str):
            out[f"{path}{t}"] += 1
        for k, v in node.items():
            collect_types(v, out, path)
    elif isinstance(node, list):
        for v in node:
            collect_types(v, out, path)


def main() -> int:
    settings = load_settings()
    print("top-level keys:", sorted(settings.keys()))
    print("sea_level:", settings.get("sea_level"), "aquifers:", settings.get("aquifers_enabled"),
          "ore_veins:", settings.get("ore_veins_enabled"), "legacy_random:", settings.get("legacy_random_source"))
    print("default_block:", settings.get("default_block"), "default_fluid:", settings.get("default_fluid"))
    print("noise keys:", sorted(settings["noise"].keys()) if "noise" in settings else None)

    router = settings["noise_router"]
    print("\nnoise_router entries:")
    for k, v in router.items():
        kind = v.get("type") if isinstance(v, dict) else type(v).__name__
        print(f"  {k:32s} {kind if kind else str(v)[:40]}")

    counts: collections.Counter = collections.Counter()
    collect_types(router, counts)
    print("\ndensity function types (router, recursive):")
    for t, c in counts.most_common():
        print(f"  {c:5d}  {t}")

    sr = settings.get("surface_rule")
    sr_counts: collections.Counter = collections.Counter()
    collect_types(sr, sr_counts)
    print("\nsurface rule types (recursive):")
    for t, c in sr_counts.most_common():
        print(f"  {c:5d}  {t}")
    if len(sys.argv) > 1 and sys.argv[1] == "--surface":
        print(json.dumps(sr, indent=1)[:20000])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
