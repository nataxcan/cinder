#!/usr/bin/env python3
"""List block names the overworld surface rules reference, vs the Cinder palette."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
EMBED = ROOT / "src" / "vanilla" / "embed.h"


def main() -> int:
    text = EMBED.read_text(encoding="utf-8", errors="replace")
    settings = None
    for key, raw in re.findall(r'^  \{"([^"]+)", "(.*)"\},$', text, re.M):
        if key == "settings:overworld":
            settings = json.loads(json.loads(f'"{raw}"'))
    names: set[str] = set()

    def walk(node) -> None:
        if isinstance(node, dict):
            if isinstance(node.get("Name"), str):
                names.add(node["Name"])
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(settings.get("surface_rule"))
    walk(settings.get("default_block"))

    gen = (ROOT / "src" / "vanilla_gen.c").read_text()
    table = gen[gen.index("static const char *names[B_COUNT]") :]
    table = table[: table.index("};")]
    mapped = set(re.findall(r'"(minecraft:[a-z_0-9]+)"', table))
    missing = sorted(n for n in names if n not in mapped)
    print(f"surface rule block names: {len(names)}")
    print(f"missing from the palette ({len(missing)}): {missing}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
