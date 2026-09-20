#!/usr/bin/env python3
"""Resolve the overworld density graph transitively and report every type used.

Walks settings:overworld's noise_router, following string references into the
named density-function entries (and noise entries), so the C port can be checked
for coverage up front instead of aborting one type at a time.
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
        if key.startswith(("minecraft:", "biome:", "carver:", "settings:")):
            try:
                out[key] = json.loads(json.loads(f'"{raw}"'))
            except json.JSONDecodeError:
                pass
    return out


def walk(node: object, embed: dict[str, object], types: collections.Counter,
         refs: set[str], depth: int = 0) -> None:
    if depth > 40:
        return
    if isinstance(node, str):
        if node.startswith("minecraft:"):
            refs.add(node)
            target = embed.get(node)
            if isinstance(target, dict):
                walk(target, embed, types, refs, depth + 1)
        return
    if isinstance(node, dict):
        t = node.get("type")
        if isinstance(t, str):
            types[t] += 1
        for v in node.values():
            walk(v, embed, types, refs, depth + 1)
        return
    if isinstance(node, list):
        for v in node:
            walk(v, embed, types, refs, depth + 1)


def main() -> int:
    embed = load_embed()
    settings = embed["settings:overworld"]
    router = settings["noise_router"]
    types: collections.Counter = collections.Counter()
    refs: set[str] = set()
    walk(router, embed, types, refs)
    print(f"density function types reachable from the overworld router ({len(refs)} named refs):")
    for t, c in types.most_common():
        print(f"  {c:5d}  {t}")
    unresolved = sorted(r for r in refs if not isinstance(embed.get(r), dict))
    if unresolved:
        print("\nrefs that are not density functions (noise parameters etc.):")
        for r in unresolved:
            print("  ", r)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
