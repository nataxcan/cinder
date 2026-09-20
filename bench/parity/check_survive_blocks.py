#!/usr/bin/env python3
"""Every block named by a would_survive predicate must be in the palette.

The predicate resolves the named state through block_id_for_name; an unknown
name makes the predicate fail, which silently rejects every placement that
uses it (that is how pf:oak's would_survive filter rejected every tree).
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EMBED = ROOT / "src" / "vanilla" / "embed.h"
COMMON = ROOT / "src" / "vanilla_common.h"
GEN = ROOT / "src" / "vanilla_gen.c"


def palette() -> set[str]:
    names = set()
    text = GEN.read_text(encoding="utf-8", errors="replace")
    start = text.index("block_name(int")
    body = text[start : text.index("}", start)]
    for name in re.findall(r'"(minecraft:[a-z0-9_]+)"', body):
        names.add(name)
    return names


def would_survive_states() -> set[str]:
    text = EMBED.read_text(encoding="utf-8", errors="replace")
    out = set()
    for chunk in re.finditer(r'\\"type\\": \\"minecraft:would_survive\\"(.{0,400}?)Name\\": \\"([a-z0-9_:]+)\\"', text):
        out.add(chunk.group(2))
    return out


def main() -> int:
    have = palette()
    want = would_survive_states()
    missing = sorted(n for n in want if n not in have)
    print(f"would_survive states: {len(want)}, palette: {len(have)}")
    print(f"palette already has: {len(want) - len(missing)}")
    print(f"missing: {len(missing)}")
    for name in missing:
        print(f"  {name}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
