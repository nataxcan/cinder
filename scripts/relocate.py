#!/usr/bin/env python3
"""Point the worldgen's absolute sibling includes at this checkout.

Bend inlines `src/vanilla_gen.c` into a temporary directory, so the C modules
include each other by absolute path (see AGENTS.md). That path is the original
author's checkout, so a fresh clone has to be repointed once:

    python3 scripts/relocate.py            # rewrite to this directory
    python3 scripts/relocate.py --check    # report only

It rewrites the prefix inside the CINDER_INC_* macros and the embed include.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
PATTERN = re.compile(r'"/mnt/c/[^"]*/cinder/src/([A-Za-z0-9_./]+)"')
WINDOWS = re.compile(r'"[A-Za-z]:[\\/][^"]*?/cinder/src/([A-Za-z0-9_./]+)"')


def main() -> int:
    check = "--check" in sys.argv
    prefix = str(SRC).replace("\\", "/")
    changed = []
    for path in sorted(SRC.glob("*.c")) + sorted(SRC.glob("*.h")):
        text = path.read_text(encoding="utf-8", errors="replace")
        new = PATTERN.sub(lambda m: f'"{prefix}/{m.group(1)}"', text)
        new = WINDOWS.sub(lambda m: f'"{prefix}/{m.group(1)}"', new)
        if new != text:
            changed.append(path)
            if not check:
                path.write_text(new)
    if not changed:
        print(f"all includes already point at {prefix}")
        return 0
    for path in changed:
        print(f"{'would rewrite' if check else 'rewrote'} {path.relative_to(ROOT)}")
    print(f"{len(changed)} file(s) -> {prefix}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
