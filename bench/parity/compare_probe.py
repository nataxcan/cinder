#!/usr/bin/env python3
"""Compare probe outputs from the vanilla jar and from Cinder.

Vanilla lines: "x y z <decimal> [<hex float>]"
Cinder lines : "x y z <decimal>"
Values are compared as exact doubles (the hex column, when present, wins), so
formatting differences cannot mask or create mismatches. Biome lines compare as
strings.
"""
from __future__ import annotations

import sys


def load(path: str) -> dict[tuple[int, int, int], str]:
    out: dict[tuple[int, int, int], str] = {}
    with open(path) as fh:
        for line in fh:
            parts = line.split()
            if len(parts) < 4:
                continue
            key = (int(parts[0]), int(parts[1]), int(parts[2]))
            out[key] = " ".join(parts[3:])
    return out


def main() -> int:
    vanilla = load(sys.argv[1])
    cinder = load(sys.argv[2])
    label = sys.argv[3] if len(sys.argv) > 3 else "values"
    keys = sorted(set(vanilla) | set(cinder))
    missing_v = [k for k in keys if k not in vanilla]
    missing_c = [k for k in keys if k not in cinder]
    if missing_v or missing_c:
        print(f"   FAIL: {len(missing_c)} points missing from cinder, {len(missing_v)} from vanilla")
        return 1

    bad = []
    for k in keys:
        v = vanilla[k]
        c = cinder[k]
        vv = v.split()
        cc = c.split()
        if len(vv) > 1:  # vanilla prints decimal + hex; the hex is authoritative
            try:
                vnum = float.fromhex(vv[1])
            except ValueError:
                vnum = float(vv[0])
        else:
            vnum = float(vv[0])
        try:
            cnum = float(cc[0])
        except ValueError:
            if v != c:
                bad.append((k, v, c))
            continue
        if vnum != cnum:
            bad.append((k, v, c))

    if not bad:
        print(f"   PASS ({len(keys)} {label} identical)")
        return 0
    print(f"   FAIL: {len(bad)}/{len(keys)} {label} differ")
    for k, v, c in bad[:10]:
        print(f"     {k}: vanilla={v} cinder={c}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
