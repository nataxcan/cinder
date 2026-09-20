#!/usr/bin/env python3
"""Inventory the providers/predicates used by the overworld's placed features.

Collects every `type` inside placement modifiers (int providers, height providers,
block predicates) so the port only implements what the overworld actually uses.
"""
from __future__ import annotations

import collections
import json
import re
from pathlib import Path

EMBED = Path(__file__).resolve().parents[2] / "src" / "vanilla" / "embed.h"


def load_embed() -> dict[str, object]:
    text = EMBED.read_text(encoding="utf-8", errors="replace")
    out: dict[str, object] = {}
    for key, raw in re.findall(r'^  \{"([^"]+)", "(.*)"\},$', text, re.M):
        if key.startswith(("biome:", "cf:", "pf:")):
            try:
                out[key] = json.loads(json.loads(f'"{raw}"'))
            except json.JSONDecodeError:
                pass
    return out


def walk(node, counter: collections.Counter) -> None:
    if isinstance(node, dict):
        t = node.get("type")
        if isinstance(t, str):
            counter[t] += 1
        for v in node.values():
            walk(v, counter)
    elif isinstance(node, list):
        for v in node:
            walk(v, counter)


def main() -> int:
    embed = load_embed()
    biomes = [k for k in embed if k.startswith("biome:") and ":" in k]
    placed = set()
    for key in biomes:
        body = embed[key]
        for step in body.get("features", []):
            for ref in step:
                placed.add("pf:" + ref.split(":", 1)[-1])

    ints: collections.Counter = collections.Counter()
    heights: collections.Counter = collections.Counter()
    preds: collections.Counter = collections.Counter()
    modifiers: collections.Counter = collections.Counter()
    for key in sorted(placed):
        pf = embed.get(key)
        if not isinstance(pf, dict):
            continue
        for mod in pf.get("placement", []):
            if not isinstance(mod, dict):
                continue
            t = mod.get("type", "?")
            modifiers[t] += 1
            if t == "minecraft:count" or t == "minecraft:random_offset":
                walk(mod.get("count") or mod.get("xz_spread"), ints)
                walk(mod.get("y_spread"), ints)
            elif t == "minecraft:height_range":
                walk(mod.get("height"), heights)
                walk(mod.get("height"), ints)
            elif t == "minecraft:block_predicate_filter":
                walk(mod.get("predicate"), preds)
            elif t in ("minecraft:noise_threshold_count", "minecraft:noise_based_count"):
                walk(mod, ints)

    print("int provider types:")
    for t, c in ints.most_common():
        print(f"  {c:5d}  {t}")
    print("\nheight provider types:")
    for t, c in heights.most_common():
        print(f"  {c:5d}  {t}")
    print("\nblock predicate types:")
    for t, c in preds.most_common():
        print(f"  {c:5d}  {t}")
    print("\nplacement modifier types:")
    for t, c in modifiers.most_common():
        print(f"  {c:5d}  {t}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
