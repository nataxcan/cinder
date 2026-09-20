#!/usr/bin/env python3
"""Inventory the feature graph the overworld actually uses.

Walks every overworld biome's `features` steps (placed features), then each
placed feature's placement modifiers and its configured feature type, and prints
the distinct sets - the exact work list for a vanilla feature port.
"""
from __future__ import annotations

import collections
import json
import re
import sys
from pathlib import Path

EMBED = Path(__file__).resolve().parents[2] / "src" / "vanilla" / "embed.h"


def load_embed() -> dict[str, object]:
    text = EMBED.read_text(encoding="utf-8", errors="replace")
    out: dict[str, object] = {}
    for key, raw in re.findall(r'^  \{"([^"]+)", "(.*)"\},$', text, re.M):
        if key.startswith(("biome:", "cf:", "pf:", "carver:", "minecraft:", "settings:")):
            try:
                out[key] = json.loads(json.loads(f'"{raw}"'))
            except json.JSONDecodeError:
                pass
    return out


def embed_key(prefix: str, ref: str) -> str:
    """embed.h keys drop the namespace: minecraft:plains_trees -> pf:plains_trees"""
    return prefix + ref.split(":", 1)[-1]


def main() -> int:
    embed = load_embed()
    biomes = {k: v for k, v in embed.items() if k.startswith("biome:") and ":" in k}
    overworld = []
    for key, body in biomes.items():
        path = key.split(":", 1)[1]
        if path.startswith(("end_", "the_end", "the_void", "nether", "basalt", "crimson", "warped", "soul_sand", "cave_air")):
            continue
        overworld.append((key, body))

    placed_used: collections.Counter = collections.Counter()
    modifiers: collections.Counter = collections.Counter()
    configured_of: dict[str, str] = {}
    for key, body in overworld:
        for step in body.get("features", []):
            for ref in step:
                if isinstance(ref, str):
                    placed_used[ref] += 1
                    pf = embed.get(embed_key("pf:", ref))
                    if isinstance(pf, dict):
                        inner = pf.get("feature")
                        if isinstance(inner, str):
                            configured_of[ref] = inner
                        elif isinstance(inner, dict):
                            configured_of[ref] = inner.get("type", "?")
                        for mod in pf.get("placement", []):
                            if isinstance(mod, dict):
                                modifiers[mod.get("type", "?")] += 1

    cf_types: collections.Counter = collections.Counter()
    cf_used = set()
    for pf_ref, cf_ref in configured_of.items():
        cf_used.add(cf_ref)
        cf = embed.get(embed_key("cf:", cf_ref)) if isinstance(cf_ref, str) else None
        if isinstance(cf, dict):
            cf_types[cf.get("type", "?")] += 1
        elif isinstance(cf_ref, str) and not cf_ref.startswith("cf:"):
            cf_types["<inline/noop>"] += 1

    print(f"overworld biomes: {len(overworld)}")
    print(f"distinct placed features referenced: {len(placed_used)}")
    print(f"distinct configured features referenced: {len(cf_used)}")
    print("\nplacement modifier types:")
    for t, c in modifiers.most_common():
        print(f"  {c:5d}  {t}")
    print("\nconfigured feature types:")
    for t, c in cf_types.most_common():
        print(f"  {c:5d}  {t}")
    missing = [r for r in cf_used if isinstance(r, str) and embed_key("cf:", r) not in embed]
    if missing:
        print(f"\nconfigured features referenced but NOT embedded ({len(missing)}):")
        for r in sorted(missing)[:20]:
            print("   ", r)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
