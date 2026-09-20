#!/usr/bin/env python3
"""Every LegacyRng must have a defined kind.

The feature RNG is Xoroshiro-backed (LEGACY_RNG_XORO); every other RNG is the
plain LCG. A stack LegacyRng that is never zeroed would read an uninitialised
kind, so this lists each declaration with the lines that follow it.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    bad = 0
    for path in sorted((ROOT / "src").glob("*.c")):
        lines = path.read_text().splitlines()
        for i, line in enumerate(lines):
            if not re.search(r"\bLegacyRng\s+\w+\s*;", line):
                continue
            window = "\n".join(lines[i : i + 6])
            if "XORO" in window or "memset" in window or "= {0}" in window:
                continue
            print(f"{path.relative_to(ROOT)}:{i + 1}: LegacyRng without an explicit kind")
            bad += 1
    print(f"{bad} declaration(s) missing a kind")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
