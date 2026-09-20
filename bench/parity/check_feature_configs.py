#!/usr/bin/env python3
"""Find configured-feature fields the port reads but the JSON never defines.

Several 26.2 configured-feature JSONs omit optional fields and rely on the
codec's defaults (the amethyst geode ships `"layers": {}` and no
`distribution_points` at all). A port that reads such a field gets its own
fallback instead - which is usually fine only if the fallback happens to equal
the codec default, and catastrophic when it does not (the geode sampled 0 points
and never placed anything).

This maps each `cf:<name>` to the C function implementing its `type` (through the
`{"type", fn}` lookup tables), finds every `jget(config, "key")` that the JSON
does not provide, and compares the C fallback with the codec default recorded
below (extracted with javap from the jar - see docs in
`bench/parity/codec_defaults.sh`).

    python3 bench/parity/check_feature_configs.py [--verbose]
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SRC = ROOT / "src"

# (feature type, field) -> codec default, for fields the 26.2 JSONs omit.
CODEC_DEFAULTS = {
    ("geode", "distribution_points"): "uniform(3,4)",
    ("geode", "point_offset"): "uniform(1,2)",
    ("geode", "min_gen_offset"): "-16",
    ("geode", "max_gen_offset"): "16",
    ("geode", "noise_multiplier"): "0.05",
    ("geode", "placements_require_layer0_alternate"): "true",
    ("geode", "use_potential_placements_chance"): "0.35",
    ("spring_feature", "requires_block_below"): "true",
    ("spring_feature", "rock_count"): "4",
    ("spring_feature", "hole_count"): "1",
    ("multiface_growth", "can_place_on_floor"): "false",
    ("multiface_growth", "search_range"): "10",
    ("multiface_growth", "chance_of_spreading"): "0.5",
    ("large_dripstone", "floor_to_ceiling_search_range"): "30",
}


def embed_entries() -> dict[str, dict]:
    text = (SRC / "vanilla" / "embed.h").read_text(encoding="utf-8", errors="replace")
    out: dict[str, dict] = {}
    for m in re.finditer(r'^  \{"([^"]+)", "(.*)"\},$', text, re.M):
        key, raw = m.group(1), m.group(2)
        if not key.startswith("cf:"):
            continue
        try:
            body = json.loads(json.loads('"' + raw + '"'))
        except Exception:
            continue
        if isinstance(body, dict):
            out[key] = body
    return out


def lookup_tables() -> dict[str, str]:
    table: dict[str, str] = {}
    for path in SRC.glob("*.c"):
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r'\{"([a-z_0-9]+)",\s*(feature_[a-z_0-9]+)\}', text):
            table.setdefault(m.group(1), m.group(2))
    return table


def function_bodies() -> dict[str, str]:
    bodies: dict[str, str] = {}
    for path in SRC.glob("*.c"):
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r'^static\s+int\s+(feature_[a-z_0-9]+)\s*\([^)]*\)\s*\{', text, re.M):
            name = m.group(1)
            start = m.end()
            depth, i = 1, start
            while i < len(text) and depth:
                if text[i] == "{":
                    depth += 1
                elif text[i] == "}":
                    depth -= 1
                i += 1
            bodies[name] = text[start:i]
    return bodies


def fallback_for(body: str, key: str) -> str:
    """The C fallback expression used for a missing field, if it is a literal."""
    patterns = [
        r'jnum\(\s*jget\(\s*config\s*,\s*"' + key + r'"\s*\)\s*,\s*([^)]+)\)',
        r'jbool\(\s*jget\(\s*config\s*,\s*"' + key + r'"\s*\)\s*,\s*([^)]+)\)',
        r'\(int\)jnum\(\s*jget\(\s*config\s*,\s*"' + key + r'"\s*\)\s*,\s*([^)]+)\)',
    ]
    for pat in patterns:
        m = re.search(pat, body)
        if m:
            return m.group(1).strip()
    # A missing provider written out as the codec's default uniform:
    #   field ? feat_int_provider(field, r) : A + legacy_next_bound(r, B)  == uniform(A, A+B-1)
    # the value may be read inline or hoisted into a local of the same name
    m = re.search(
        r'(?:jget\(\s*config\s*,\s*"' + key + r'"\s*\)|\b' + key + r'\b)'
        r'\s*\?[^;]*?:\s*(-?[0-9]+)\s*\+\s*legacy_next_bound\(r,\s*([0-9]+)\)',
        body,
    )
    if m:
        lo, span = int(m.group(1)), int(m.group(2))
        return f"uniform({lo},{lo + span - 1})"
    if re.search(r'jget\(\s*config\s*,\s*"' + key + r'"\s*\)', body):
        return "(read, no literal fallback)"
    return "(unread)"


def normalise(value: str) -> str:
    """Compare `1`/`0` with `true`/`false`, and treat an explicit ternary as a
    match when the code spells the codec default out."""
    v = value.strip().strip('()')
    v = {"1": "true", "0": "false", "1.0": "1", "0.5": "0.5"}.get(v, v)
    v = re.sub(r"\s+", "", v)
    return v


def main() -> int:
    verbose = "--verbose" in sys.argv
    entries = embed_entries()
    table = lookup_tables()
    bodies = function_bodies()

    mismatches: list[str] = []
    unknown: list[str] = []
    ok = 0
    for key, body in sorted(entries.items()):
        ftype = body.get("type")
        if not ftype:
            continue
        short = ftype.split(":")[-1]
        fn = table.get(short)
        if not fn or fn not in bodies:
            continue
        cfg = body.get("config") or {}
        if not isinstance(cfg, dict):
            continue
        read = set(re.findall(r'jget\(\s*(?:config|cfg)\s*,\s*"([a-z_0-9]+)"\s*\)', bodies[fn]))
        for field in sorted(read - set(cfg)):
            fallback = fallback_for(bodies[fn], field)
            want = CODEC_DEFAULTS.get((short, field))
            line = f"{key:40s} {short:22s} {field:34s} C={fallback:18s} codec={want or '?'}"
            if want is None:
                unknown.append(line)
            elif normalise(fallback) != normalise(want):
                mismatches.append(line)
            else:
                ok += 1
                if verbose:
                    print("ok   " + line)

    for line in mismatches:
        print("MISMATCH " + line)
    for line in unknown:
        print("UNKNOWN  " + line)
    print()
    print(f"fields with a matching fallback: {ok}")
    print(f"fallback != codec default: {len(mismatches)}")
    print(f"no recorded codec default: {len(unknown)}")
    return 1 if mismatches or unknown else 0


if __name__ == "__main__":
    sys.exit(main())
