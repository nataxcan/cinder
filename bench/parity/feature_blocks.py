#!/usr/bin/env python3
"""List the block names the overworld's configured features reference.

Any name that is not in Cinder's B_* palette must be added before a vanilla
feature port can place it.
"""
from __future__ import annotations

import collections
import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
EMBED = ROOT / "src" / "vanilla" / "embed.h"
HEADER = ROOT / "src" / "vanilla_common.h"


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


def palette() -> set[str]:
    text = HEADER.read_text()
    body = text[text.index("B_AIR = 0") : text.index("B_COUNT")]
    return set(re.findall(r"\bB_[A-Z0-9_]+\b", body))


def main() -> int:
    embed = load_embed()
    # only the biomes the overworld can actually produce (BIOME_NAMES in vanilla_biomes.h)
    bio_h = (ROOT / "src" / "vanilla_biomes.h").read_text()
    overworld = set(re.findall(r'\[ (BIO_[A-Z_]+) \] = "minecraft:([a-z_]+)"', bio_h))
    paths = {name for _, name in overworld}
    biomes = [k for k in embed if k.startswith("biome:") and k.split(":", 1)[1] in paths]
    cf_used = set()
    for key in biomes:
        for step in embed[key].get("features", []):
            for ref in step:
                pf = embed.get("pf:" + ref.split(":", 1)[-1])
                if isinstance(pf, dict):
                    inner = pf.get("feature")
                    if isinstance(inner, str):
                        cf_used.add("cf:" + inner.split(":", 1)[-1])

    names: collections.Counter = collections.Counter()

    def walk(node) -> None:
        if isinstance(node, dict):
            if "Name" in node and isinstance(node["Name"], str):
                names[node["Name"]] += 1
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    for key in sorted(cf_used):
        if key in embed:
            walk(embed[key])

    known = palette()
    # names present in the palette (B_* ids map to minecraft:<name> in vanilla_gen.c)
    gen = (ROOT / "src" / "vanilla_gen.c").read_text()
    table = gen[gen.index("static const char *names[B_COUNT]") :]
    table = table[: table.index("};")]
    mapped = set(re.findall(r'"(minecraft:[a-z_0-9]+)"', table))
    missing = sorted(n for n in names if n not in mapped)
    print(f"distinct block names referenced by overworld configured features: {len(names)}")
    print(f"already in the palette: {len(names) - len(missing)}")
    print(f"missing ({len(missing)}):")
    for n in missing:
        print(f"  {names[n]:5d}  {n}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
