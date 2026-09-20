#!/usr/bin/env python3
"""Inventory the worldgen data embedded in src/vanilla/embed.h (keys + JSON types)."""
from __future__ import annotations

import collections
import json
import re
import sys
from pathlib import Path

EMBED = Path(__file__).resolve().parents[2] / "src" / "vanilla" / "embed.h"


def main() -> int:
    text = EMBED.read_text(encoding="utf-8", errors="replace")
    entries = re.findall(r'^  \{"([^"]+)", "(.*)"\},$', text, re.M)
    print(f"embedded entries: {len(entries)}")

    prefixes = collections.Counter()
    for key, _ in entries:
        prefixes[key.split(":")[0]] += 1
    print("by namespace:", dict(prefixes))

    types = collections.Counter()
    for key, raw in entries:
        body = json.loads(f'"{raw}"')  # C string literal escapes are JSON-compatible here
        if key.startswith("settings:"):
            continue
        try:
            obj = json.loads(body)
        except json.JSONDecodeError:
            types["<unparseable>"] += 1
            continue
        t = obj.get("type") if isinstance(obj, dict) else None
        types[str(t)] += 1
    print("\ntypes (non-settings):")
    for t, c in types.most_common():
        print(f"  {c:5d}  {t}")

    if len(sys.argv) > 1 and sys.argv[1] == "--dfs":
        for key, raw in entries:
            if not key.startswith("minecraft:"):
                continue
            body = json.loads(f'"{raw}"')
            obj = json.loads(body)
            t = obj.get("type") if isinstance(obj, dict) else None
            extra = ""
            if t is None and isinstance(obj, dict):
                extra = " keys=" + ",".join(sorted(obj.keys()))
            print(f"{key:60s} {t}{extra}")
        return 0

    if len(sys.argv) > 1:
        want = sys.argv[1]
        for key, raw in entries:
            if key == want:
                print(json.loads(f'"{raw}"'))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
