#!/usr/bin/env python3
"""Pure-Python Anvil (.mca) region reader for Minecraft 26.2 (1.18+ chunk layout).

Reads a region directory, picks the region file holding a chunk, inflates the
chunk NBT and decodes the paletted block/biome containers into flat name-index
spaces that line up with Cinder's dump format (see dump_format.md):

    block index  (y - min_y) * 256 + z * 16 + x     y in -64..319 (overworld)
    biome index  sec * 64 + yi * 16 + zi * 4 + xi   sec 0..23 (y section -4..19)

No third-party dependencies.  Compression types 1 (gzip), 2 (deflate), 3
(none) are handled natively; type 4 (LZ4) shells out to lz4-java via java.

CLI
---
    python3 regions.py --region-dir DIR --cx N --cz N [--json]
    python3 regions.py --region-dir DIR --scan [--json]
    python3 regions.py --region-dir DIR --cx N --cz N --nbt      # raw NBT dump
"""

from __future__ import annotations

import argparse
import glob
import gzip
import json
import os
import struct
import subprocess
import sys
import zlib
from array import array
from collections import Counter

AIR = "minecraft:air"
DEFAULT_BIOME = "minecraft:plains"

SECTOR = 4096
BLOCK_ENTRIES = 4096          # 16x16x16
BIOME_ENTRIES = 64            # 4x4x4
SECTION_HEIGHT = 16
CHUNK_BLOCKS = 24 * BLOCK_ENTRIES   # 98304, overworld y -64..319
CHUNK_BIOMES = 24 * BIOME_ENTRIES   # 1536

MASK64 = (1 << 64) - 1


class NBTError(Exception):
    pass


class ChunkMissing(Exception):
    """No chunk record for the requested (cx, cz)."""


# --------------------------------------------------------------------------
# NBT
# --------------------------------------------------------------------------

TAG_END = 0
TAG_BYTE = 1
TAG_SHORT = 2
TAG_INT = 3
TAG_LONG = 4
TAG_FLOAT = 5
TAG_DOUBLE = 6
TAG_BYTE_ARRAY = 7
TAG_STRING = 8
TAG_LIST = 9
TAG_COMPOUND = 10
TAG_INT_ARRAY = 11
TAG_LONG_ARRAY = 12

_FMT = {TAG_SHORT: ">h", TAG_INT: ">i", TAG_LONG: ">q",
        TAG_FLOAT: ">f", TAG_DOUBLE: ">d"}
_SIZE = {TAG_SHORT: 2, TAG_INT: 4, TAG_LONG: 8, TAG_FLOAT: 4, TAG_DOUBLE: 8}


class _Reader:
    __slots__ = ("d", "i")

    def __init__(self, data, i=0):
        self.d = data
        self.i = i

    def _scalar(self, tag):
        fmt = _FMT[tag]
        n = _SIZE[tag]
        v = struct.unpack_from(fmt, self.d, self.i)[0]
        self.i += n
        return v

    def u1(self):
        v = self.d[self.i]
        self.i += 1
        return v

    def u2(self):
        v = struct.unpack_from(">H", self.d, self.i)[0]
        self.i += 2
        return v

    def i1(self):
        v = struct.unpack_from(">b", self.d, self.i)[0]
        self.i += 1
        return v

    def i4(self):
        v = struct.unpack_from(">i", self.d, self.i)[0]
        self.i += 4
        return v

    def raw(self, n):
        v = self.d[self.i:self.i + n]
        if len(v) != n:
            raise NBTError("truncated NBT payload")
        self.i += n
        return v

    def string(self):
        return self.raw(self.u2()).decode("utf-8", "replace")

    def payload(self, tag):
        if tag == TAG_BYTE:
            return self.i1()
        if tag in _FMT:
            return self._scalar(tag)
        if tag == TAG_BYTE_ARRAY:
            return self.raw(self.i4())
        if tag == TAG_STRING:
            return self.string()
        if tag == TAG_LIST:
            elem = self.u1()
            n = self.i4()
            if n <= 0:
                return []
            return [self.payload(elem) for _ in range(n)]
        if tag == TAG_COMPOUND:
            out = {}
            while True:
                t = self.u1()
                if t == TAG_END:
                    return out
                name = self.string()
                out[name] = self.payload(t)
        if tag == TAG_INT_ARRAY:
            n = self.i4()
            vals = struct.unpack_from(">%di" % n, self.d, self.i)
            self.i += 4 * n
            return list(vals)
        if tag == TAG_LONG_ARRAY:
            n = self.i4()
            vals = struct.unpack_from(">%dq" % n, self.d, self.i)
            self.i += 8 * n
            return list(vals)
        raise NBTError("unknown NBT tag id %d" % tag)

    def skip(self, tag):
        """Advance past one payload without materialising it."""
        if tag == TAG_BYTE:
            self.i += 1
        elif tag in _SIZE:
            self.i += _SIZE[tag]
        elif tag == TAG_BYTE_ARRAY:
            self.i += 4 + self.i4()
        elif tag == TAG_STRING:
            self.i += 2 + self.u2()
        elif tag == TAG_LIST:
            elem = self.u1()
            n = self.i4()
            if elem == TAG_END and n > 0:
                raise NBTError("non-empty list of TAG_End")
            for _ in range(n):
                self.skip(elem)
        elif tag == TAG_COMPOUND:
            while True:
                t = self.u1()
                if t == TAG_END:
                    return
                nlen = struct.unpack_from(">H", self.d, self.i)[0]
                self.i += 2 + nlen
                self.skip(t)
        elif tag == TAG_INT_ARRAY:
            self.i += 4 + 4 * self.i4()
        elif tag == TAG_LONG_ARRAY:
            self.i += 4 + 8 * self.i4()
        else:
            raise NBTError("unknown NBT tag id %d" % tag)


def parse_nbt(data):
    """Parse a root compound tag (named or not) and return its dict."""
    r = _Reader(data)
    tag = r.u1()
    if tag != TAG_COMPOUND:
        raise NBTError("root tag is %d, expected compound" % tag)
    r.string()
    return r.payload(TAG_COMPOUND)


def parse_chunk_status(data):
    """Extract only the root 'Status' string; skips everything else."""
    r = _Reader(data)
    tag = r.u1()
    if tag != TAG_COMPOUND:
        raise NBTError("root tag is %d, expected compound" % tag)
    r.string()
    while True:
        t = r.u1()
        if t == TAG_END:
            return None
        name = r.string()
        if t == TAG_STRING and name == "Status":
            return r.string()
        r.skip(t)


# --------------------------------------------------------------------------
# Bit storage (SimpleBitStorage: 64/bits values per long, no cross-long merge)
# --------------------------------------------------------------------------

def unpack_long_array(longs, bits, count):
    """Decode vanilla's SimpleBitStorage layout into an array('I') of `count`."""
    if bits <= 0:
        return array("I", [0]) * count
    per_long = 64 // bits
    mask = (1 << bits) - 1
    out = array("I")
    ext = out.extend
    if bits == 4:
        # dominant case: 16 nibbles per long
        for v in longs:
            v &= MASK64
            ext(((v & 15, (v >> 4) & 15, (v >> 8) & 15, (v >> 12) & 15,
                 (v >> 16) & 15, (v >> 20) & 15, (v >> 24) & 15, (v >> 28) & 15,
                 (v >> 32) & 15, (v >> 36) & 15, (v >> 40) & 15, (v >> 44) & 15,
                 (v >> 48) & 15, (v >> 52) & 15, (v >> 56) & 15, (v >> 60) & 15)))
    else:
        shifts = [i * bits for i in range(per_long)]
        for v in longs:
            v &= MASK64
            ext([(v >> s) & mask for s in shifts])
    if len(out) > count:
        del out[count:]
    if len(out) < count:
        raise NBTError("bit storage holds %d of %d values" % (len(out), count))
    return out


def ceil_log2(n):
    if n <= 1:
        return 0
    return (n - 1).bit_length()


# --------------------------------------------------------------------------
# Paletted containers
# --------------------------------------------------------------------------

def _block_entry(entry):
    """NBT block-state entry -> (name, properties dict)."""
    name = entry.get("Name", AIR)
    props = entry.get("Properties") or {}
    if props:
        props = {k: (v if isinstance(v, str) else str(v)) for k, v in props.items()}
    return name, props


def _state_key(name, props):
    if not props:
        return name
    return (name, tuple(sorted(props.items())))


def decode_block_states(tag):
    """{'palette': [...], 'data': [...]} -> (palette, array('I') x4096)."""
    if not tag:
        return [(AIR, {})], array("I", [0]) * BLOCK_ENTRIES
    palette = [_block_entry(e) for e in tag.get("palette") or []]
    if not palette:
        palette = [(AIR, {})]
    if len(palette) == 1:
        return palette, array("I", [0]) * BLOCK_ENTRIES
    data = tag.get("data")
    if data is None:
        raise NBTError("block_states palette of %d without data" % len(palette))
    bits = max(4, ceil_log2(len(palette)))
    return palette, unpack_long_array(data, bits, BLOCK_ENTRIES)


def decode_biomes(tag):
    """{'palette': [...], 'data': [...]} -> (palette, array('I') x64)."""
    if not tag:
        return [DEFAULT_BIOME], array("I", [0]) * BIOME_ENTRIES
    palette = list(tag.get("palette") or [])
    if not palette:
        palette = [DEFAULT_BIOME]
    if len(palette) == 1:
        return palette, array("I", [0]) * BIOME_ENTRIES
    data = tag.get("data")
    if data is None:
        raise NBTError("biomes palette of %d without data" % len(palette))
    bits = max(1, ceil_log2(len(palette)))
    return palette, unpack_long_array(data, bits, BIOME_ENTRIES)


# --------------------------------------------------------------------------
# Chunk model
# --------------------------------------------------------------------------

class Section:
    """One stored 16^3 section of a chunk."""

    __slots__ = ("y", "palette", "blocks", "biome_palette", "biomes")

    def __init__(self, y, palette, blocks, biome_palette, biomes):
        self.y = y
        self.palette = palette                  # [(name, props)]
        self.blocks = blocks                    # array('I') 4096, idx (y,z,x)
        self.biome_palette = biome_palette      # [str]
        self.biomes = biomes                    # array('I') 64, idx (y,z,x)*4

    @property
    def block_names(self):
        return [n for n, _ in self.palette]

    def __repr__(self):
        return "Section(y=%d, palette=%d, biomes=%d)" % (
            self.y, len(self.palette), len(self.biome_palette))


class Chunk:
    """A decoded chunk.

    palette/block_names      chunk-wide block palette (index 0 = air)
    blocks                   array('I') x 98304, index (y-min_y)*256 + z*16 + x
    biome_palette            chunk-wide biome list (index 0 = default biome)
    biomes                   array('I') x 1536, index sec*64 + yi*16 + zi*4 + xi
    """

    __slots__ = ("cx", "cz", "status", "data_version", "min_section_y",
                 "sections", "palette", "block_names", "blocks",
                 "biome_palette", "biomes", "_state_index", "_biome_index")

    def __init__(self, cx, cz, nbt):
        self.cx = cx
        self.cz = cz
        self.status = nbt.get("Status", "minecraft:empty")
        self.data_version = nbt.get("DataVersion")
        self.min_section_y = int(nbt.get("yPos", -4))

        sections = {}
        for stag in nbt.get("sections") or ():
            y = int(stag.get("Y", 0))
            palette, blocks = decode_block_states(stag.get("block_states"))
            bpal, bdat = decode_biomes(stag.get("biomes"))
            sections[y] = Section(y, palette, blocks, bpal, bdat)
        self.sections = sections

        # chunk-wide block palette / index space
        self.palette = [(AIR, {})]
        self.block_names = [AIR]
        state_index = {AIR: 0}
        blocks = array("I", bytes(4 * CHUNK_BLOCKS))
        for y in sorted(sections):
            sec = sections[y]
            base = (y * SECTION_HEIGHT - self.min_section_y * SECTION_HEIGHT) * 256
            if base < 0 or base + BLOCK_ENTRIES > CHUNK_BLOCKS:
                continue
            trans = []
            for name, props in sec.palette:
                key = _state_key(name, props)
                idx = state_index.get(key)
                if idx is None:
                    idx = len(self.palette)
                    state_index[key] = idx
                    self.palette.append((name, props))
                    self.block_names.append(name)
                trans.append(idx)
            if len(trans) == 1:
                blocks[base:base + BLOCK_ENTRIES] = array("I", [trans[0]]) * BLOCK_ENTRIES
            else:
                blocks[base:base + BLOCK_ENTRIES] = array("I", [trans[i] for i in sec.blocks])
        self.blocks = blocks
        self._state_index = state_index

        # chunk-wide biome palette / index space (24 sections, sec 0 = y -4)
        self.biome_palette = [DEFAULT_BIOME]
        biome_index = {DEFAULT_BIOME: 0}
        biomes = array("I", bytes(4 * CHUNK_BIOMES))
        for y in sorted(sections):
            sec = sections[y]
            sec_index = y - self.min_section_y
            if sec_index < 0 or sec_index >= CHUNK_BIOMES // BIOME_ENTRIES:
                continue
            trans = []
            for name in sec.biome_palette:
                idx = biome_index.get(name)
                if idx is None:
                    idx = len(self.biome_palette)
                    biome_index[name] = idx
                    self.biome_palette.append(name)
                trans.append(idx)
            if len(trans) == 1:
                biomes[sec_index * BIOME_ENTRIES:(sec_index + 1) * BIOME_ENTRIES] = (
                    array("I", [trans[0]]) * BIOME_ENTRIES)
            else:
                biomes[sec_index * BIOME_ENTRIES:(sec_index + 1) * BIOME_ENTRIES] = (
                    array("I", [trans[i] for i in sec.biomes]))
        self.biomes = biomes
        self._biome_index = biome_index

    # -- accessors ---------------------------------------------------------
    def block_name(self, y, z, x):
        return self.block_names[self.blocks[(y - self.min_section_y * 16) * 256 + z * 16 + x]]

    def biome_name(self, y_section, yi, zi, xi):
        sec = y_section - self.min_section_y
        return self.biome_palette[self.biomes[sec * 64 + yi * 16 + zi * 4 + xi]]

    def block_counts(self):
        return Counter(self.block_names[i] for i in self.blocks)

    def biome_counts(self):
        return Counter(self.biome_palette[i] for i in self.biomes)

    def as_json(self):
        return {
            "cx": self.cx,
            "cz": self.cz,
            "status": self.status,
            "data_version": self.data_version,
            "min_section_y": self.min_section_y,
            "sections": sorted(self.sections),
            "block_palette": self.block_names,
            "blocks": dict(self.block_counts().most_common()),
            "biome_palette": self.biome_palette,
            "biomes": dict(self.biome_counts().most_common()),
        }

    def __repr__(self):
        return "Chunk(cx=%d, cz=%d, status=%s, sections=%d)" % (
            self.cx, self.cz, self.status, len(self.sections))


# --------------------------------------------------------------------------
# Region files
# --------------------------------------------------------------------------

def region_path(region_dir, cx, cz):
    return os.path.join(region_dir, "r.%d.%d.mca" % (cx >> 5, cz >> 5))


class RegionFile:
    """Reader for one r.X.Z.mca file."""

    def __init__(self, path):
        self.path = path
        self.fh = open(path, "rb")
        self.header = self.fh.read(SECTOR)
        if len(self.header) != SECTOR:
            self.fh.close()
            raise NBTError("%s: short region header" % path)

    def close(self):
        self.fh.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def locations(self):
        """Yield (local_cx, local_cz, sector_offset, sector_count) for present chunks."""
        h = self.header
        for i in range(1024):
            b = h[i * 4:i * 4 + 4]
            off = (b[0] << 16) | (b[1] << 8) | b[2]
            cnt = b[3]
            if off or cnt:
                yield i & 31, i >> 5, off, cnt

    def has_chunk(self, cx, cz):
        i = ((cx & 31) + (cz & 31) * 32) * 4
        return self.header[i:i + 3] != b"\x00\x00\x00"

    def read_raw(self, cx, cz):
        """Return (compression_type, compressed_bytes) or None if absent."""
        i = ((cx & 31) + (cz & 31) * 32) * 4
        b = self.header[i:i + 4]
        off = (b[0] << 16) | (b[1] << 8) | b[2]
        cnt = b[3]
        if off == 0 or cnt == 0:
            return None
        self.fh.seek(off * SECTOR)
        hdr = self.fh.read(5)
        if len(hdr) != 5:
            raise NBTError("%s: truncated chunk header at sector %d" % (self.path, off))
        length = int.from_bytes(hdr[:4], "big")
        comp = hdr[4]
        if length < 1:
            raise NBTError("%s: zero-length chunk payload" % self.path)
        payload = self.fh.read(length - 1)
        if len(payload) != length - 1:
            raise NBTError("%s: truncated chunk payload" % self.path)
        return comp, payload


def decompress(comp, payload):
    if comp == 1:
        return gzip.decompress(payload)
    if comp == 2:
        return zlib.decompress(payload)
    if comp == 3:
        return payload
    if comp == 4:
        return lz4_decompress(payload)
    if comp == 127:
        raise NBTError("chunk uses compression 127 (external .mcc stream), unsupported")
    raise NBTError("unknown compression type %d" % comp)


# --- lz4 ---------------------------------------------------------------

LZ4_HELPER_SRC = """\
import java.io.*;
import net.jpountz.lz4.LZ4BlockInputStream;
public class Lz4Dec {
  public static void main(String[] args) throws Exception {
    LZ4BlockInputStream in = new LZ4BlockInputStream(new BufferedInputStream(System.in));
    byte[] buf = new byte[1 << 16];
    OutputStream out = new BufferedOutputStream(System.out);
    int n;
    while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
    out.flush();
  }
}
"""


def _java_bin(name):
    home = os.path.expanduser("~")
    cand = os.path.join(home, "jdk-25", "bin", name)
    if os.path.exists(cand):
        return cand
    return name


def _lz4_jar():
    env = os.environ.get("CINDER_LZ4_JAR")
    if env:
        return env
    pats = [
        os.path.expanduser("~/dev/c2me-run/libraries/at/yawk/lz4/lz4-java/*/lz4-java-*.jar"),
        os.path.expanduser("~/dev/c2me-run/libraries/**/lz4-java-*.jar"),
        "/home/nataxcan/dev/c2me-run/libraries/at/yawk/lz4/lz4-java/*/lz4-java-*.jar",
    ]
    for pat in pats:
        hits = sorted(glob.glob(pat, recursive=True))
        if hits:
            return hits[-1]
    raise NBTError("lz4 chunk found but no lz4-java jar; set CINDER_LZ4_JAR")


def lz4_decompress(payload):
    jar = _lz4_jar()
    cache = os.path.join(os.path.expanduser("~"), ".cache", "cinder-parity")
    os.makedirs(cache, exist_ok=True)
    cls = os.path.join(cache, "Lz4Dec.class")
    if not os.path.exists(cls):
        src = os.path.join(cache, "Lz4Dec.java")
        with open(src, "w") as fh:
            fh.write(LZ4_HELPER_SRC)
        proc = subprocess.run([_java_bin("javac"), "-nowarn", "-cp", jar, "-d", cache, src],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if proc.returncode != 0:
            raise NBTError("cannot compile lz4 helper (%s): %s"
                           % (jar, proc.stdout.decode("utf-8", "replace")[-500:]))
    proc = subprocess.run(
        [_java_bin("java"), "-cp", cache + os.pathsep + jar, "Lz4Dec"],
        input=payload, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if proc.returncode != 0:
        raise NBTError("lz4 decode failed: %s" % proc.stderr.decode("utf-8", "replace")[-500:])
    return proc.stdout


def open_region(region_dir, cx, cz):
    path = region_path(region_dir, cx, cz)
    if not os.path.exists(path):
        raise ChunkMissing("no region file %s" % path)
    return RegionFile(path)


def chunk_compressed(region_dir, cx, cz):
    """Raw (compression, bytes) for a chunk, or None if the slot is empty."""
    with open_region(region_dir, cx, cz) as rf:
        return rf.read_raw(cx, cz)


def chunk_nbt(region_dir, cx, cz):
    raw = chunk_compressed(region_dir, cx, cz)
    if raw is None:
        raise ChunkMissing("chunk (%d,%d) not stored" % (cx, cz))
    return parse_nbt(decompress(*raw))


def read_chunk(region_dir, cx, cz):
    """Fully decoded Chunk for (cx, cz)."""
    return Chunk(cx, cz, chunk_nbt(region_dir, cx, cz))


def chunk_exists(region_dir, cx, cz):
    try:
        with open_region(region_dir, cx, cz) as rf:
            return rf.has_chunk(cx, cz)
    except ChunkMissing:
        return False


def read_chunk_status(region_dir, cx, cz):
    raw = chunk_compressed(region_dir, cx, cz)
    if raw is None:
        return None
    return parse_chunk_status(decompress(*raw))


def scan_dir(region_dir):
    """Yield (cx, cz, status) for every stored chunk in every region file."""
    for path in sorted(glob.glob(os.path.join(region_dir, "r.*.*.mca"))):
        base = os.path.basename(path)[2:-4]
        try:
            rx, rz = (int(v) for v in base.split("."))
        except ValueError:
            continue
        with RegionFile(path) as rf:
            for cx, cz, off, cnt in rf.locations():
                if not off or not cnt:
                    continue
                try:
                    raw = rf.read_raw(cx, cz)
                    status = parse_chunk_status(decompress(*raw)) if raw else None
                except Exception as exc:              # noqa: BLE001 - report, keep scanning
                    status = "error:%s" % exc
                yield rx * 32 + cx, rz * 32 + cz, status


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def _print_counts(title, counts, limit):
    print("%s (%d distinct):" % (title, len(counts)))
    for name, count in counts.most_common(limit if limit else None):
        print("  %10d  %s" % (count, name))


def main(argv=None):
    ap = argparse.ArgumentParser(description="Read Minecraft Anvil region files.")
    ap.add_argument("--region-dir", required=True, help="directory containing r.X.Z.mca")
    ap.add_argument("--cx", type=int, help="chunk x")
    ap.add_argument("--cz", type=int, help="chunk z")
    ap.add_argument("--json", action="store_true", help="emit JSON")
    ap.add_argument("--scan", action="store_true",
                    help="walk every chunk in the directory and print a status histogram")
    ap.add_argument("--nbt", action="store_true", help="dump the raw NBT tree")
    ap.add_argument("--top", type=int, default=0, help="limit listing to N entries (0 = all)")
    args = ap.parse_args(argv)

    if not os.path.isdir(args.region_dir):
        print("error: not a directory: %s" % args.region_dir, file=sys.stderr)
        return 2

    if args.scan:
        hist = Counter()
        full = 0
        total = 0
        for _cx, _cz, status in scan_dir(args.region_dir):
            hist[status] += 1
            total += 1
            if status == "minecraft:full":
                full += 1
        if args.json:
            print(json.dumps({"total": total, "full": full,
                              "statuses": dict(hist.most_common())}, indent=2))
        else:
            for status, count in hist.most_common():
                print("%8d  %s" % (count, status))
            print("full=%d total=%d" % (full, total))
        return 0

    if args.cx is None or args.cz is None:
        ap.error("--cx/--cz required unless --scan is used")

    try:
        if args.nbt:
            nbt = chunk_nbt(args.region_dir, args.cx, args.cz)
            text = json.dumps(nbt, indent=2, default=lambda o: "<%d bytes>" % len(o)
                              if isinstance(o, bytes) else str(o))
            if len(text) > 200000:
                text = text[:200000] + "\n... [truncated]"
            print(text)
            return 0

        chunk = read_chunk(args.region_dir, args.cx, args.cz)
    except (ChunkMissing, NBTError, OSError) as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps(chunk.as_json(), indent=2))
    else:
        print("chunk (%d, %d) status=%s dataVersion=%s minSectionY=%d sections=%s"
              % (chunk.cx, chunk.cz, chunk.status, chunk.data_version,
                 chunk.min_section_y, sorted(chunk.sections)))
        _print_counts("blocks", chunk.block_counts(), args.top)
        _print_counts("biomes", chunk.biome_counts(), args.top)
    return 0


if __name__ == "__main__":
    sys.exit(main())
