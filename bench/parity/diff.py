#!/usr/bin/env python3
"""Compare a Cinder chunk dump (see dump_format.md) against a vanilla reference
world stored as Anvil region files.

Usage
-----
    python3 diff.py --dump DUMP.bin --region-dir DIR \
                    --cx0 A --cz0 B --nx N --nz M [--json] [--top 20]

Reports, per compared chunk and in aggregate:
  * block-name agreement,
  * the top mismatching (cinder -> vanilla) block-name pairs,
  * the top vanilla block names that never appear in Cinder's palette,
  * biome-name agreement and the top biome mismatch pairs.

Both sides are re-indexed into one shared per-run name table before comparison,
so a position-wise comparison is a plain u16 array compare (C speed when equal).
"""

from __future__ import annotations

import argparse
import json
import operator
import os
import struct
import sys
from array import array
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import regions  # noqa: E402  (same directory)

MAGIC = b"CNDR"
DUMP_VERSION = 1
BLOCK_BYTES = regions.CHUNK_BLOCKS * 2
BIOME_BYTES = regions.CHUNK_BIOMES
RECORD_SIZE = 8 + BLOCK_BYTES + BIOME_BYTES
MAX_NAMES = 0xFFFE


class DumpFormatError(Exception):
    pass


# --------------------------------------------------------------------------
# Cinder dump reader
# --------------------------------------------------------------------------

class Dump:
    """Reader for Cinder's CNDR dump file (little-endian, see dump_format.md)."""

    def __init__(self, path):
        self.path = path
        self.fh = open(path, "rb")
        self.size = self.fh.seek(0, os.SEEK_END)
        self.fh.seek(0)
        head = self.fh.read(24)
        if len(head) < 24:
            raise DumpFormatError("file shorter than a 24-byte header")
        (magic, self.version, self.seed, self.grid, self.cell,
         self.nblocks) = struct.unpack("<4sIIIII", head)
        if magic != MAGIC:
            raise DumpFormatError("bad magic %r (expected %r)" % (magic, MAGIC))
        if self.version != DUMP_VERSION:
            raise DumpFormatError("unsupported dump version %d" % self.version)
        if self.cell != 16:
            raise DumpFormatError("cell=%d, expected 16" % self.cell)
        if self.nblocks == 0:
            raise DumpFormatError("nblocks=0")

        self.block_names, self.biome_names, self.data_start = self._read_name_tables()
        self.nbiomes = len(self.biome_names)
        self.block_ids = set(self.block_names)
        self.biome_ids = set(self.biome_names)

        expected = self.data_start + self.grid * self.grid * RECORD_SIZE
        if self.size != expected:
            raise DumpFormatError(
                "file size %d, expected %d (%d x %d records); trailing or truncated data"
                % (self.size, expected, self.grid, self.grid))

        # absolute file offset of every chunk record, keyed by (cx, cz)
        self.records = {}
        for i in range(self.grid * self.grid):
            off = self.data_start + i * RECORD_SIZE
            self.fh.seek(off)
            cx, cz = struct.unpack("<ii", self.fh.read(8))
            if (cx, cz) in self.records:
                raise DumpFormatError("duplicate chunk record (%d,%d)" % (cx, cz))
            self.records[(cx, cz)] = off

    def _read_name_tables(self):
        """Return (block_names, biome_names, absolute offset of first record)."""
        window = 1 << 16
        while True:
            end = min(self.size, 24 + window)
            self.fh.seek(24)
            blob = self.fh.read(end - 24)
            try:
                blocks, biomes, off = self._parse_names(blob)
            except (struct.error, IndexError, UnicodeDecodeError):
                if end >= self.size:
                    raise DumpFormatError("truncated name tables")
                window *= 4
                continue
            return blocks, biomes, 24 + off

    def _parse_names(self, blob):
        off = 0
        blocks = []
        for _ in range(self.nblocks):
            (n,) = struct.unpack_from("<H", blob, off)
            off += 2
            blocks.append(blob[off:off + n].decode("utf-8"))
            off += n
        (nbiomes,) = struct.unpack_from("<I", blob, off)
        off += 4
        if nbiomes > 1 << 20:
            raise DumpFormatError("absurd nbiomes=%d" % nbiomes)
        biomes = []
        for _ in range(nbiomes):
            (n,) = struct.unpack_from("<H", blob, off)
            off += 2
            biomes.append(blob[off:off + n].decode("utf-8"))
            off += n
        return blocks, biomes, off

    def has(self, cx, cz):
        return (cx, cz) in self.records

    def chunk(self, cx, cz):
        """Return (blocks array('H') x98304, biomes bytes x1536)."""
        try:
            off = self.records[(cx, cz)]
        except KeyError:
            raise KeyError("chunk (%d,%d) is not in the dump" % (cx, cz))
        self.fh.seek(off + 8)
        raw = self.fh.read(RECORD_SIZE - 8)
        if len(raw) != RECORD_SIZE - 8:
            raise DumpFormatError("truncated record (%d,%d)" % (cx, cz))
        blocks = array("H")
        blocks.frombytes(raw[:BLOCK_BYTES])
        if sys.byteorder != "little":
            blocks.byteswap()
        return blocks, raw[BLOCK_BYTES:]

    def close(self):
        self.fh.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


# --------------------------------------------------------------------------
# Comparison
# --------------------------------------------------------------------------

class NameTable:
    """Shared name -> id table; every id fits in u16."""

    __slots__ = ("names", "_ids")

    def __init__(self):
        self.names = []
        self._ids = {}

    def table_for(self, names):
        ids = self._ids
        out = self.names
        for name in names:
            if name not in ids:
                if len(out) >= MAX_NAMES:
                    raise DumpFormatError("more than %d distinct names" % MAX_NAMES)
                ids[name] = len(out)
                out.append(name)
        return [ids[name] for name in names]


def _reindex(table, values):
    return array("H", map(table.__getitem__, values))


def compare(dump, region_dir, cx0, cz0, nx, nz, top=20):
    blocks_tab = NameTable()
    biomes_tab = NameTable()
    total = matched = 0
    per_chunk = []
    mismatch_pairs = Counter()
    expected_but_unknown = Counter()

    bio_total = bio_matched = 0
    bio_per_chunk = []
    bio_pairs = Counter()

    missing_dump = []
    missing_ref = []
    dump_blocks_tab = None

    for cz in range(cz0, cz0 + nz):
        for cx in range(cx0, cx0 + nx):
            in_dump = dump.has(cx, cz)
            in_ref = regions.chunk_exists(region_dir, cx, cz)
            if not in_dump:
                missing_dump.append((cx, cz))
            if not in_ref:
                missing_ref.append((cx, cz))
            if not (in_dump and in_ref):
                continue

            dblocks, dbiomes = dump.chunk(cx, cz)
            chunk = regions.read_chunk(region_dir, cx, cz)

            # --- blocks -------------------------------------------------
            # A = cinder, B = vanilla, both re-indexed into the shared table
            dump_blocks_tab = blocks_tab.table_for(dump.block_names)
            ref_blocks_tab = blocks_tab.table_for(chunk.block_names)
            a = _reindex(dump_blocks_tab, dblocks)
            b = _reindex(ref_blocks_tab, chunk.blocks)
            n = len(b)
            total += n
            if a == b:
                cmatched = n
            else:
                cmatched = n - sum(map(operator.ne, a, b))
                names = blocks_tab.names
                for x, y in zip(a, b):
                    if x != y:
                        dump_name = names[x]
                        ref_name = names[y]
                        mismatch_pairs[(dump_name, ref_name)] += 1
                        if ref_name not in dump.block_ids:
                            expected_but_unknown[ref_name] += 1
            matched += cmatched
            per_chunk.append({"cx": cx, "cz": cz, "status": chunk.status,
                              "matched": cmatched, "compared": n,
                              "agreement": (100.0 * cmatched / n) if n else 100.0})

            # --- biomes -------------------------------------------------
            dump_biomes_tab = biomes_tab.table_for(dump.biome_names)
            ref_biomes_tab = biomes_tab.table_for(chunk.biome_palette)
            ba = _reindex(dump_biomes_tab, dbiomes)
            bb = _reindex(ref_biomes_tab, chunk.biomes)
            bn = len(bb)
            bio_total += bn
            if ba == bb:
                bmatched = bn
            else:
                bmatched = bn - sum(map(operator.ne, ba, bb))
                bnames = biomes_tab.names
                for x, y in zip(ba, bb):
                    if x != y:
                        bio_pairs[(bnames[x], bnames[y])] += 1
            bio_matched += bmatched
            bio_per_chunk.append({"cx": cx, "cz": cz, "matched": bmatched,
                                  "compared": bn,
                                  "agreement": (100.0 * bmatched / bn) if bn else 100.0})

    return {
        "chunks": {
            "compared": len(per_chunk),
            "missing_in_dump": len(missing_dump),
            "missing_in_ref": len(missing_ref),
            "missing_in_dump_list": missing_dump[:100],
            "missing_in_ref_list": missing_ref[:100],
        },
        "blocks": {
            "compared": total,
            "matched": matched,
            "agreement": (100.0 * matched / total) if total else 100.0,
            "per_chunk": per_chunk,
            "mismatch_pairs": [[d, v, c] for (d, v), c in mismatch_pairs.most_common(top)],
            "distinct_mismatch_pairs": len(mismatch_pairs),
            "expected_but_unknown": [[n, c] for n, c in expected_but_unknown.most_common(top)],
            "distinct_expected_but_unknown": len(expected_but_unknown),
            "cinder_palette": dump.block_names,
        },
        "biomes": {
            "compared": bio_total,
            "matched": bio_matched,
            "agreement": (100.0 * bio_matched / bio_total) if bio_total else 100.0,
            "per_chunk": bio_per_chunk,
            "mismatch_pairs": [[d, v, c] for (d, v), c in bio_pairs.most_common(top)],
            "distinct_mismatch_pairs": len(bio_pairs),
            "cinder_palette": dump.biome_names,
        },
    }


# --------------------------------------------------------------------------
# Reporting
# --------------------------------------------------------------------------

def report_text(res, dump, region_dir, per_chunk_lines=True, worst=10):
    out = []
    w = out.append
    w("dump:        %s" % dump.path)
    w("region dir:  %s" % region_dir)
    w("dump header: seed=%d grid=%d cell=%d nblocks=%d nbiomes=%d"
      % (dump.seed, dump.grid, dump.cell, dump.nblocks, dump.nbiomes))
    c = res["chunks"]
    w("chunks:      compared=%d missing_in_dump=%d missing_in_ref=%d"
      % (c["compared"], c["missing_in_dump"], c["missing_in_ref"]))
    if c["missing_in_dump_list"]:
        w("  missing in dump (first 10): %s" % (c["missing_in_dump_list"][:10],))
    if c["missing_in_ref_list"]:
        w("  missing in ref  (first 10): %s" % (c["missing_in_ref_list"][:10],))

    b = res["blocks"]
    w("")
    w("block agreement: %.6f%%  (%d/%d positions)"
      % (b["agreement"], b["matched"], b["compared"]))
    if per_chunk_lines:
        w("per-chunk block agreement:")
        for row in b["per_chunk"]:
            w("  cx=%4d cz=%4d  %12.6f%%  (%8d/%8d)  %s"
              % (row["cx"], row["cz"], row["agreement"],
                 row["matched"], row["compared"], row["status"]))
    if b["per_chunk"]:
        rows = sorted(b["per_chunk"], key=lambda r: r["agreement"])
        w("worst %d chunks (block agreement): %s" % (worst, ", ".join(
            "(%d,%d)=%.4f%%" % (r["cx"], r["cz"], r["agreement"]) for r in rows[:worst])))
        agree = sorted(r["agreement"] for r in b["per_chunk"])
        w("per-chunk block agreement min=%.6f%% median=%.6f%% max=%.6f%%"
          % (agree[0], agree[len(agree) // 2], agree[-1]))

    w("")
    w("top block mismatches (cinder -> vanilla), %d distinct pairs:"
      % b["distinct_mismatch_pairs"])
    if not b["mismatch_pairs"]:
        w("  (none)")
    for cinder, vanilla, count in b["mismatch_pairs"]:
        w("  %10d  %s -> %s" % (count, cinder, vanilla))

    w("")
    w("expected-but-unknown: vanilla block names absent from Cinder's palette, "
      "%d distinct names:" % b["distinct_expected_but_unknown"])
    if not b["expected_but_unknown"]:
        w("  (none)")
    for name, count in b["expected_but_unknown"]:
        w("  %10d  %s" % (count, name))

    bi = res["biomes"]
    w("")
    w("biome agreement: %.6f%%  (%d/%d cells)"
      % (bi["agreement"], bi["matched"], bi["compared"]))
    if per_chunk_lines:
        w("per-chunk biome agreement:")
        for row in bi["per_chunk"]:
            w("  cx=%4d cz=%4d  %12.6f%%" % (row["cx"], row["cz"], row["agreement"]))
    w("top biome mismatches (cinder -> vanilla), %d distinct pairs:"
      % bi["distinct_mismatch_pairs"])
    if not bi["mismatch_pairs"]:
        w("  (none)")
    for cinder, vanilla, count in bi["mismatch_pairs"]:
        w("  %10d  %s -> %s" % (count, cinder, vanilla))
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Compare a Cinder dump against a vanilla world.")
    ap.add_argument("--dump", required=True, help="Cinder CNDR dump file")
    ap.add_argument("--region-dir", required=True, help="vanilla r.X.Z.mca directory")
    ap.add_argument("--cx0", type=int, default=0)
    ap.add_argument("--cz0", type=int, default=0)
    ap.add_argument("--nx", type=int, default=None, help="chunks in x (default: dump grid)")
    ap.add_argument("--nz", type=int, default=None, help="chunks in z (default: dump grid)")
    ap.add_argument("--top", type=int, default=20, help="rows per top-N table")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--no-per-chunk", action="store_true",
                    help="omit the per-chunk line list in text output")
    args = ap.parse_args(argv)

    if not os.path.isdir(args.region_dir):
        print("error: not a directory: %s" % args.region_dir, file=sys.stderr)
        return 2

    try:
        with Dump(args.dump) as dump:
            nx = dump.grid if args.nx is None else args.nx
            nz = dump.grid if args.nz is None else args.nz
            res = compare(dump, args.region_dir, args.cx0, args.cz0, nx, nz, top=args.top)
            if args.json:
                payload = dict(res)
                payload.update({"dump": args.dump, "region_dir": args.region_dir,
                                "seed": dump.seed, "grid": dump.grid, "cell": dump.cell,
                                "nblocks": dump.nblocks, "nbiomes": dump.nbiomes})
                print(json.dumps(payload, indent=2))
            else:
                print(report_text(res, dump, args.region_dir,
                                  per_chunk_lines=not args.no_per_chunk))
    except (DumpFormatError, regions.ChunkMissing, regions.NBTError, OSError) as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
