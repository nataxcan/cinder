# Cinder ↔ vanilla chunk dump format (`CNDR`, version 1)

Two programs have to agree on this file:

* the **writer** — Cinder's C worldgen (`src/vanilla_gen.c`), emitting one dump for a
  square grid of chunks;
* the **readers** — `bench/parity/diff.py` (comparison) and `bench/parity/regions.py`
  (reference side), both pure Python.

Everything is **little-endian**. All offsets below are byte offsets from the start of
the file.

## File layout

```
offset  size            field
0       4               magic "CNDR"
4       4               u32 version = 1
8       4               u32 world seed
12      4               u32 grid (chunks per side)
16      4               u32 cell = 16
20      4               u32 nblocks
24      ...             nblocks entries: u16 name_len + name bytes (UTF-8, e.g. "minecraft:stone")
        ...             4 bytes: u32 nbiomes
        ...             nbiomes entries: u16 name_len + name bytes (e.g. "minecraft:plains")
then grid*grid chunk records in order cz = 0..grid-1, cx = 0..grid-1:
  i32 cx, i32 cz
  u16 blocks[98304]     index = ((y+64)*16 + z)*16 + x, y in -64..319
  u8  biomes[1536]      index = sec*64 + yi*16 + zi*4 + xi, sec = 0..23 (y section = sec-4), yi/zi/xi in 0..3
```

Derived sizes (checked by `diff.py`, which refuses anything else):

```
block table bytes = 98304 * 2 = 196608
biome table bytes = 1536  * 1 = 1536
record           = 8 + 196608 + 1536 = 198152
data_start       = 24 + sum(2 + len(name))
file size        = data_start + grid*grid*198152
```

## Semantics

* `name` strings are **resource locations with namespace**, exactly as they appear in
  Minecraft (`minecraft:stone`, `minecraft:water`, …). No trailing NUL, no `\0`.
* The two name tables are **global for the whole dump**: every block/biome that appears
  anywhere in the grid has one entry, and the `u16`/`u8` arrays index into them. Nothing
  is chunk-local, so both sides must emit identical spellings.
* All `grid*grid` records must be present, in `cz`-major order; there is no "empty slot"
  encoding. A chunk that could not be generated should not be written at all — the
  reader keys records by the explicit `cx`/`cz` fields, so records may also be sparse in
  principle, but the file-size check requires exactly `grid*grid` of them.
* `u16 blocks` covers the full overworld column: `y = index/256 - 64` (0 → y=-64,
  98303 → y=319), `z = (index/16) % 16`, `x = index % 16`. Air is a normal palette
  entry — there is no "absent" value; a position that is air must have the palette index
  of `minecraft:air`.
* `u8 biomes` covers the 24 biome sections (`sec = 0..23`), each a 4×4×4 cell volume:
  `sec = index/64`, and within the section `x` is the fastest axis, then `z`, then `y`:
  `yi = (index/16) % 4`, `zi = (index/4) % 4`, `xi = index % 4`. Section `sec` is the
  16-block y-slice `sec - 4` (so `sec 0` is y ∈ [-64,-49], `sec 23` is y ∈ [304,319]).
* The biome cells are **4×4×4 block-resolution**, matching vanilla's biome container
  (one biome per 4×4×4 blocks), not per block.

### `u16` index ≠ properties

The dump stores block *names* only, so a palette holds at most one entry per block
state spelled as `minecraft:xxx`. Properties (`axis`, `waterlogged`, …) are **not**
represented; the comparison done by `diff.py` is name-level. If Cinder ever needs
state-level parity, a new dump version must add properties to the palette entry.

## Reference implementation in Cinder

The natural C layout is a per-column loop that appends to a growable name table and then
serializes; the python side (`regions.py`) mirrors it with these mappings:

| dump concept | vanilla Anvil source |
| --- | --- |
| section `sec` | section `Y = sec - 4` (overworld `yPos = -4`, 24 sections) |
| block index | section-local index `(y_local*16 + z)*16 + x`, section at `y = sec*16 - 64` |
| biome index | section-local index `(y_local*4 + z)*4 + x` in the 4×4×4 biome container |
| name table | `Name` values of `block_states.palette` / `biomes.palette` entries |

Vanilla chunk NBT (26.2, 1.18+ layout, see `SerializableChunkData`):

```
root: { DataVersion, xPos, yPos(minSectionY), zPos, Status, sections: [
          { Y: byte, block_states: { palette: [{Name, Properties?}], data: long[]? },
                     biomes:      { palette: ["minecraft:plains", ...], data: long[]? },
            BlockLight: byte[]?, SkyLight: byte[]? } ] }
```

* `data` is omitted when the palette has exactly one entry (that value fills the whole
  container).
* NBT `TAG_Byte` is **signed**: `Y` is `-4..19` on the overworld, so a reader that treats
  the byte as unsigned sees sections 252..255 instead of -4..-1 (a real bug this tooling
  hit and fixed).
* A `TAG_List` of compounds stores each element as a *bare* compound payload (its
  `TAG_End` terminated item list) — there is **no** `0x0A 0x00 0x00` id/name prefix per
  element. Lists of strings likewise hold raw `u16 len + bytes` entries.
* Bit width on disk: `bits = max(min_bits, ceil_log2(palette_size))` with
  `min_bits = 4` for block states and `min_bits = 1` for biomes; a palette of size 1 has
  no data at all. `bits` is *not* stored in the NBT — it is derived from the palette.
* Packing is vanilla's `SimpleBitStorage`: `64 // bits` values per `long`, values packed
  low-to-high inside the long with **no** cross-long spanning; the final long is padded
  with unused high bits. `regions.py:unpack_long_array` implements exactly this.
* Sections with neither blocks nor light are absent from `sections`; `regions.py` fills
  those positions with `minecraft:air` blocks and with the factory default biome
  (`minecraft:plains`) so both sides always compare 98304 / 1536 positions.

## Statistics reported by `diff.py`

* **block agreement** = matching positions / compared positions (98304 per chunk,
  aggregate over all compared chunks); no partial credit for properties.
* **top mismatches** = `(cinder_name, vanilla_name) -> count` over mismatching positions.
* **expected-but-unknown** = vanilla names that occur at a mismatching position and are
  **not present in the dump's global block palette at all** — these are the blocks Cinder
  never learned about; a non-empty list means Cinder's palette is incomplete, not that a
  single block mapping is wrong.
* **biome agreement** = matching cells / compared cells (1536 per chunk) with the same
  pair table for mismatches.

## Reference world conventions (what the vanilla side contains)

The reference world is generated by a vanilla 26.2 server, seed 1, **features and
structures disabled** via a datapack (`bench/parity/gen_ref.py`, `ref_config.md`), then
forceloaded to `Status=minecraft:full` over chunks x=0..47, z=0..47. So the expected block
set is: air, stone, deepslate, tuff, granite/diorite/andesite, dirt/grass_block/podzol/
mycelium/rooted_dirt/mud, gravel, sand/red_sand/sandstone/clay, water, lava, bedrock,
snow/snow_block/ice/packed_ice/powder_snow, calcite/smooth_basalt/amethyst, dripstone,
moss, terracotta, plus cave-carver equivalents (cave_air, …). Any tree/ore/vegetation
block or structure block in the comparison output means the datapack did not take effect.

Observed block set of the actual reference world (chunks 0..47 × 0..47, 226 492 416 block
positions): air, deepslate, stone, water, bedrock, dirt, grass_block, gravel, sand, lava,
sandstone, tuff, granite, deepslate_iron_ore, copper_ore, raw_iron_block,
raw_copper_block, obsidian. `deepslate_iron_ore`/`raw_iron_block`/`copper_ore`/
`raw_copper_block` and `obsidian` are *expected*: ore veins are applied by
`OreVeinifier` inside the noise stage (not by a placed feature), so disabling features
does not remove them, and obsidian comes from lava/water contact during carving.

## Verifying a dump / a world by hand

In 26.2 a vanilla server writes the overworld to
`<world>/dimensions/minecraft/overworld/region/r.X.Z.mca` (there is no
`<world>/region/`); pass that directory as `--region-dir`. The reference world under
`/home/nataxcan/dev/ref26.2/world/` carries a `region -> dimensions/minecraft/overworld/region`
symlink, so both paths work there.

```sh
# decode one reference chunk and print its block/biome histograms
python3 bench/parity/regions.py --region-dir /home/nataxcan/dev/ref26.2/world/region --cx 0 --cz 0

# status histogram of a whole region directory (used to wait for generation)
python3 bench/parity/regions.py --region-dir /home/nataxcan/dev/ref26.2/world/region --scan

# full comparison
python3 bench/parity/diff.py --dump cinder.bin \
    --region-dir /home/nataxcan/dev/ref26.2/world/region --cx0 0 --cz0 0 --nx 48 --nz 48

# raw NBT tree of one chunk (debugging palettes / status)
python3 bench/parity/regions.py --region-dir DIR --cx 0 --cz 0 --nbt
```

## Known caveats

* Compression type 127 (external `.mcc` chunk streams) is rejected; the reference server
  uses `region-file-compression=deflate` (type 2), which is the well-tested path.
  Types 1/2/3 are native Python; type 4 (LZ4) shells out to `lz4-java` from
  `~/dev/c2me-run/libraries` (override with `CINDER_LZ4_JAR`).
* `Properties` values are normalised to strings and only used for palette identity on the
  reference side; the dump has no property channel (see above).
* The comparison is a straight positional comparison over the whole column, including
  air; it does not skip "uninteresting" y ranges, so a systematic off-by-one in either
  index formula shows up as a huge agreement drop rather than a subtle difference.
