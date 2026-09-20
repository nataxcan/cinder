#!/usr/bin/env python3
"""Read a Cinder dump (see bench/parity/dump_format docs in diff.py) and print stats."""
from __future__ import annotations

import collections
import struct
import sys
from pathlib import Path


def read_dump(path: Path):
    data = path.read_bytes()
    off = 0
    magic, version, seed, grid, cell = struct.unpack_from("<IIIII", data, off)
    off += 20
    assert magic == 0x52444E43, hex(magic)
    (nblocks,) = struct.unpack_from("<I", data, off)
    off += 4
    block_names = []
    for _ in range(nblocks):
        (ln,) = struct.unpack_from("<H", data, off)
        off += 2
        block_names.append(data[off:off + ln].decode())
        off += ln
    (nbiomes,) = struct.unpack_from("<I", data, off)
    off += 4
    biome_names = []
    for _ in range(nbiomes):
        (ln,) = struct.unpack_from("<H", data, off)
        off += 2
        biome_names.append(data[off:off + ln].decode())
        off += ln
    per_chunk = 8 + 16 * 384 * 16 * 2 + 24 * 64
    chunks = {}
    while off + per_chunk <= len(data):
        cx, cz = struct.unpack_from("<ii", data, off)
        off += 8
        blocks = struct.unpack_from("<%dH" % (16 * 384 * 16), data, off)
        off += 16 * 384 * 16 * 2
        biomes = data[off:off + 24 * 64]
        off += 24 * 64
        chunks[(cx, cz)] = (blocks, biomes)
    return dict(version=version, seed=seed, grid=grid, block_names=block_names,
                biome_names=biome_names, chunks=chunks)


def main() -> int:
    dump = read_dump(Path(sys.argv[1]))
    counts: collections.Counter = collections.Counter()
    bcounts: collections.Counter = collections.Counter()
    for (cx, cz), (blocks, biomes) in dump["chunks"].items():
        for b in blocks:
            counts[dump["block_names"][b]] += 1
        for b in biomes:
            bcounts[dump["biome_names"][b]] += 1
    print(f"chunks={len(dump['chunks'])} grid={dump['grid']} seed={dump['seed']}")
    total = sum(counts.values())
    print("blocks:")
    for name, n in counts.most_common(30):
        print(f"  {name:34s} {n:10d}  {100.0 * n / total:5.2f}%")
    print("biomes:")
    for name, n in bcounts.most_common(20):
        print(f"  {name:34s} {n:8d}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
