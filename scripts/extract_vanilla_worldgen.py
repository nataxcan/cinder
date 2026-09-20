#!/usr/bin/env python3
"""Pack vanilla 26.2 worldgen JSON into src/vanilla/embed.h for the C generator."""
from __future__ import annotations

import json
import zipfile
from pathlib import Path

JAR = Path("/home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar")
OUT = Path("/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla/embed.h")

PREFIXES = (
    "data/minecraft/worldgen/noise_settings/overworld.json",
    "data/minecraft/worldgen/density_function/",
    "data/minecraft/worldgen/noise/",
    "data/minecraft/worldgen/configured_carver/",
    "data/minecraft/worldgen/biome/",
    "data/minecraft/worldgen/configured_feature/",
    "data/minecraft/worldgen/placed_feature/",
)


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\r", "")


def key_for(name: str) -> str:
    rest = name[len("data/minecraft/") :]
    if rest.endswith(".json"):
        rest = rest[:-5]
    # DF + noise keep vanilla IDs (minecraft:overworld/offset, minecraft:cave_cheese).
    # Everything else is prefixed so biomes/features don't collide.
    mapping = (
        ("worldgen/density_function/", "minecraft:"),
        ("worldgen/noise/", "minecraft:"),
        ("worldgen/configured_carver/", "carver:"),
        ("worldgen/biome/", "biome:"),
        ("worldgen/configured_feature/", "cf:"),
        ("worldgen/placed_feature/", "pf:"),
        ("worldgen/noise_settings/", "settings:"),
    )
    for token, prefix in mapping:
        if rest.startswith(token):
            return prefix + rest[len(token) :]
    return "minecraft:" + rest


def main() -> int:
    if not JAR.exists():
        raise SystemExit(f"missing {JAR}")
    entries: list[tuple[str, str, str]] = []
    with zipfile.ZipFile(JAR) as z:
        for name in z.namelist():
            if not name.endswith(".json"):
                continue
            if not name.startswith("data/minecraft/worldgen/"):
                continue
            keep = False
            for p in PREFIXES:
                if name.startswith(p) or name == p:
                    keep = True
                    break
            if not keep:
                continue
            raw = z.read(name).decode("utf-8")
            json.loads(raw)  # validate
            entries.append((key_for(name), name, raw))
    entries.sort()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "/* Auto-generated from vanilla 26.2 server jar. Do not edit. */",
        "#pragma once",
        "#include <stddef.h>",
        "typedef struct { const char *key; const char *json; } CinderEmbed;",
        f"#define CINDER_EMBED_COUNT {len(entries)}",
        "static const CinderEmbed CINDER_EMBED[] = {",
    ]
    for key, _name, raw in entries:
        lines.append(f'  {{"{key}", "{c_escape(raw)}"}},')
    lines.append("};")
    lines.append(
        "const char *cinder_embed_get(const char *key) {\n"
        "  for (size_t i = 0; i < CINDER_EMBED_COUNT; i++) {\n"
        "    const char *k = CINDER_EMBED[i].key;\n"
        "    const char *a = k, *b = key;\n"
        "    while (*a && *a == *b) { a++; b++; }\n"
        "    if (*a == 0 && *b == 0) return CINDER_EMBED[i].json;\n"
        "  }\n"
        "  return 0;\n"
        "}\n"
    )
    OUT.write_text("\n".join(lines) + "\n")
    print(f"wrote {OUT} entries={len(entries)} bytes={OUT.stat().st_size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
