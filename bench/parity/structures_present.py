#!/usr/bin/env python3
"""Which structures does the reference world actually contain?

Reads the `structures` tag of each chunk's NBT (Anvil stores every structure
start with its pieces) and reports the ids and piece counts, so the porting
effort for structure parity can be scoped from data instead of guesswork.
"""
from __future__ import annotations

import importlib.util
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("diffmod", HERE / "diff.py")
diffmod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diffmod)

REGION = Path("/home/nataxcan/dev/ref-canonical26.2/world/region")


def walk(node, path=""):
    """Yield (path, value) for every leaf of a parsed NBT structure."""
    if isinstance(node, dict):
        for k, v in node.items():
            yield from walk(v, f"{path}/{k}")
    elif isinstance(node, list):
        for i, v in enumerate(node):
            yield from walk(v, f"{path}[{i}]")
    else:
        yield path, node


def main() -> int:
    ids = Counter()
    chunks = 0
    for cz in range(8, 16):
        for cx in range(16, 24):
            try:
                nbt = diffmod.regions.chunk_nbt(REGION, cx, cz)
            except Exception:
                continue
            chunks += 1
            for path, value in walk(nbt):
                if path.endswith("/id") and isinstance(value, str) and ":" in value:
                    if "/structures/" in path:
                        ids[value] += 1
    print(f"chunks read: {chunks}")
    if not ids:
        print("no structure ids found (tag shape may differ)")
        return 0
    print("structure ids in the reference area:")
    for name, count in ids.most_common():
        print(f"  {count:>5d}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
