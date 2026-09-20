#!/usr/bin/env python3
"""Print one embedded JSON entry (the embed.h strings are C-escaped).

    python3 bench/parity/embed_show.py cf:amethyst_geode [--path config.blocks]
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

EMBED = Path(__file__).resolve().parents[2] / "src" / "vanilla" / "embed.h"


def load(key: str):
    text = EMBED.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'^  \{"' + re.escape(key) + r'", "(.*)"\},$', text, re.M)
    if not m:
        raise SystemExit(f"{key}: not in the embed")
    raw = m.group(1)
    # the C string literal uses \" for quotes and \n for newlines
    lit = '"' + raw + '"'
    return json.loads(json.loads(lit))


def main() -> int:
    key = sys.argv[1]
    obj = load(key)
    path = None
    if len(sys.argv) > 3 and sys.argv[2] == "--path":
        path = sys.argv[3]
    if path:
        cur = obj
        for part in path.split("."):
            cur = cur[int(part)] if part.isdigit() else cur[part]
        obj = cur
    print(json.dumps(obj, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
