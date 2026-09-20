# Vanilla 26.2 reference world (worldgen parity ground truth)

`bench/parity/gen_ref.py` builds the ground-truth side of the worldgen parity effort:
a **vanilla Minecraft 26.2, seed 1, overworld-only** world in which chunks
`x = 0..47, z = 0..47` (2304 chunks) are all at `Status=minecraft:full` on disk,
with **every biome feature disabled** and **every structure disabled**. What remains
is exactly the terrain stages Cinder implements: multi-noise biomes, noise +
aquifers + ore veins, surface rules and cave carvers.

Region files are compared block-by-block against `src/vanilla_gen.c`; see
`$CINDER_REF_DIR/ref_verify.json` (written by the verify stage) for the
machine-readable statistics of the recorded run.

## Re-running

```sh
# everything: wipe + rebuild the world, generate, verify
python3 bench/parity/gen_ref.py

# individual stages
python3 bench/parity/gen_ref.py --stage setup    # eula/properties/datapack only
python3 bench/parity/gen_ref.py --stage run      # boot, forceload, wait for full, stop
python3 bench/parity/gen_ref.py --stage verify   # re-read the region files, print stats
python3 bench/parity/gen_ref.py --gen-timeout 5400   # slower box: longer full-status wait
```

* The script is Python 3 standard library only and POSIX/WSL oriented. On Windows
  it re-executes itself inside WSL (`wsl.exe`) so the same command works from either
  side of the mount.
* **Idempotent by cleanup**: the setup stage always deletes the previous world
  directory (and `server.log` / `ref_verify.json`) before writing anything, so a
  re-run is a full clean rebuild. It never reuses an existing world.
* Paths can be overridden by environment variables:

  | variable | default | meaning |
  |---|---|---|
  | `CINDER_REF_DIR` | `/home/nataxcan/dev/ref26.2` | scratch dir holding the server + world |
  | `CINDER_REF_JAR` | `/home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar` | deobfuscated vanilla server jar |
  | `CINDER_REF_LIBS` | `/home/nataxcan/dev/c2me-run/libraries` | library jars for the classpath |
  | `CINDER_REF_JAVA` | `~/jdk-25/bin/java` | Java 25 (system `java` is 11 and will not run 26.2) |
  | `CINDER_REF_DIMENSION` | `minecraft/overworld` | dimension path under `world/dimensions/` |

* Artifacts written next to the world: `server.log` (full server output) and
  `ref_verify.json` (verification summary).

## Launching the server

`server-26.2.jar` is a **plain remapped jar, not a bundler**: it has no `Class-Path`
manifest and no embedded libraries, so `java -jar server.jar` dies with
`NoClassDefFoundError: joptsimple/ValueConverter`. The server is therefore launched
with an explicit classpath:

```sh
CP="$(find /home/nataxcan/dev/c2me-run/libraries -name '*.jar' | paste -sd: -):/home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar"
~/jdk-25/bin/java -Xmx2G -cp "$CP" net.minecraft.server.Main nogui    # cwd = $CINDER_REF_DIR
```

The script checks whether `server-port` is already taken and switches to a free
ephemeral port before booting, so it does not collide with other local servers.

## `server.properties`

```properties
allow-nether=false
difficulty=peaceful
enable-jmx-monitoring=false
enforce-secure-profile=false
generate-structures=false
level-name=world
level-seed=1
level-type=cinder_ref:no_structures
max-players=1
max-tick-time=-1
motd=Cinder reference world: seed 1, no features, no structures
online-mode=false
pause-when-empty-seconds=0
region-file-compression=deflate
server-port=25599
simulation-distance=4
spawn-protection=0
sync-chunk-writes=false
view-distance=4
```

Keys that are not obvious:

* `level-seed=1` — the parity seed; `level-name=world` keeps the world at
  `$CINDER_REF_DIR/world`.
* `level-type=cinder_ref:no_structures` — the datapack world preset (below).
* `generate-structures=false` — **this is what actually disables structures in
  26.2** (see the next section). `ChunkStatusTasks.generateStructureStarts` skips
  `createStructures` entirely when `WorldOptions.generateStructures()` is false.
* `pause-when-empty-seconds=0` — a headless server with no players otherwise
  *pauses* after 60 s (`MinecraftServer.tickServer` compares `emptyTicks` against
  `pauseWhenEmptySeconds * 20`; a value ≤ 0 never pauses). A paused server stops
  driving the chunk system, so generation would stall. `0` is required here.
* `max-tick-time=-1` — watchdog off; bulk forceload generation occasionally exceeds
  the 60 s tick budget warning ("Can't keep up!") and must not be killed.
* `view-distance=4` / `simulation-distance=4` — only forceloaded chunks matter;
  small distances keep spawn-area work cheap.
* `region-file-compression=deflate` (default; keeps the reader's zlib path valid)
  and `sync-chunk-writes=false` (faster flush) are the values requested for the
  reference world.
* `allow-nether=false`, `difficulty=peaceful`, `max-players=1`, `online-mode=false`
  — nothing may spawn, connect or load extra dimensions.

## Datapack (`world/datapacks/refpack/`)

Created **before the first server start**; the server logs
`Found new data pack file/refpack, loading it automatically`, and
`datapack list` reports `There are 2 data pack(s) enabled: [vanilla (built-in)], [file/refpack (world)]`.

```
world/datapacks/refpack/
├── pack.mcmeta
├── data/cinder_ref/worldgen/world_preset/no_structures.json
└── data/minecraft/worldgen/biome/<name>.json          (66 files)
```

* `pack.mcmeta` declares the server's own data pack format, read from the jar's
  `version.json` (`pack_version.data_major.minor` = `107.1` in 26.2):
  `{"pack": {"min_format": [107, 0], "max_format": [107, 1], "description": ...}}`.
  No `pack_format`/version warning appears in the log.
* `no_structures.json` is a verbatim copy of the jar's
  `data/minecraft/worldgen/world_preset/normal.json` (three dimensions, noise
  generators, no other tweaks).
* Every one of the 66 vanilla `data/minecraft/worldgen/biome/*.json` files is
  overridden with a copy in which `"features"` is replaced by the **same number of
  empty steps**. The vanilla step counts differ per biome (observed `0, 1, 8, 10, 11`;
  11 for normal overworld biomes), so the script preserves each array's length
  rather than hard-coding ten. `"carvers"`, `"effects"`, `"temperature"`,
  `"downfall"`, `"has_precipitation"`, `"spawners"` and `"spawn_costs"` are copied
  unchanged, which means **biome climate/surface parameters are untouched** and the
  multi-noise source still resolves the vanilla parameter list.

### Why `generate-structures=false` instead of the preset?

In 26.2 a **noise generator no longer has a `structure_overrides` field** — the
vanilla `world_preset/normal.json` has none, and `structure_overrides` now only
exists on *flat* generator settings. Structures come from
`ChunkGeneratorStructureState.createForNormal(...)`, which takes **every**
structure set in the registry whose biomes intersect the dimension's biomes.
Removing a key from a preset therefore cannot disable structures.

The preset is still shipped and still used as `level-type` (it is a valid,
self-documenting vanilla equivalent), and structures are really disabled by
`generate-structures=false`, which is checked in
`ChunkStatusTasks.generateStructureStarts` before any structure start is created.

## Console sequence

The run stage performs exactly this (commands as sent, responses as logged):

```
tick freeze                        -> The game is frozen
forceload add 0 0 255 255          -> Marked 256 chunks in minecraft:overworld from [0, 0] to [15, 15] to be force loaded
forceload add 0 256 255 511        -> ... from [0, 16] to [15, 31] ...
forceload add 0 512 255 767        -> ... from [0, 32] to [15, 47] ...
forceload add 256 0 511 255        -> ... from [16, 0] to [31, 15] ...
forceload add 256 256 511 511      -> ... from [16, 16] to [31, 31] ...
forceload add 256 512 511 767      -> ... from [16, 32] to [31, 47] ...
forceload add 512 0 767 255        -> ... from [32, 0] to [47, 15] ...
forceload add 512 256 767 511      -> ... from [32, 16] to [31, 47] ...
forceload add 512 512 767 767      -> ... from [32, 32] to [47, 47] ...
save-all flush                     -> Saved the game        (repeated while polling)
datapack list                      -> There are 2 data pack(s) enabled: ...
stop
```

* Nine 16×16-chunk tiles = 256 chunks per command, the vanilla cap for a single
  `forceload add`.
* **Nothing but `tick freeze` is needed.** `/tick freeze` covers every
  block-mutating mechanic at once (random ticks — grass spreading/decay,
  precipitation, fire, vine spread — plus fluid ticks and time), because
  `ServerLevel.tick` runs that whole phase only when
  `tickRateManager().runsNormally()`. Per-rule `gamerule` commands were tried
  first and are **not** used: in 26.2 the rule ids are namespaced
  (`minecraft:random_tick_speed`, …), so the camelCase names were rejected with
  `Incorrect argument for command` — and because the server echoes a rejected
  command, a naive log match looked like success. The run stage therefore scans
  the console for `Incorrect argument for command`/`Unknown ...` and aborts on a
  command the server refused.
* Completion detection: the run polls `save-all flush` and re-reads the region
  headers; once all 2304 slots are occupied it parses every saved chunk and counts
  those whose NBT `Status` ends in `full`. Only then does it stop the server.
* `tick freeze` is issued **before the first forceload**. Without it, aquifers keep
  flowing for as long as the chunks stay loaded, so the saved blocks become a
  function of wall-clock time instead of worldgen: two unfrozen runs produced 161
  vs 218 obsidian and ~9 k different water blocks. Freezing does not slow the
  chunk system down — chunks still reach `full` while frozen — and it makes the
  output reproducible (see "Determinism" below).
* `--settle-seconds N` holds the chunks for N extra seconds before the final save;
  it exists to demonstrate that the frozen output no longer depends on exposure
  time.

## Verification

`--stage verify` re-implements a minimal region/NBT reader (zlib + big-endian NBT
tags) and reports, over the 2304 chunks in range:

* region directory + files, chunk count, missing coordinates, and the `Status`
  histogram (`{'minecraft:full': 2304}`);
* a **block-name histogram** of every stored section. Sections are decoded with the
  same palette/packing rules as the game (`bits = max(4, ceil(log2(palette)))`,
  little-endian bit stream); the fast path reads nibbles/bytes directly, and a
  generic bit-unpacking fallback covers exotic palettes. Every section must decode
  to exactly 4096 blocks (the script raises otherwise);
* minimum and maximum Y of non-air blocks (computed at 16³-section resolution);
* the set of distinct biome names in the section biome palettes;
* bucket checks: `unexpected_blocks` (names outside the expected terrain set),
  `fluid_artifacts` (see below) and `feature_like_blocks` (substring screen for
  leaves/planks/logs/saplings/flowers/geodes/cobble/…), which must be empty — that
  is the evidence that features and structures really produced nothing.

Expected blocks are noise terrain (`stone`, `deepslate`, `tuff`, `granite`,
`diorite`, `andesite`), the noise-stage ore veins (`copper_ore`,
`deepslate_iron_ore`, `raw_iron_block`, … — `OreVeinifier` fills copper veins with
granite and iron veins with tuff), the blocks referenced by
`worldgen/noise_settings/overworld.json`'s `surface_rule` (bedrock, dirt/grass/
podzol/mycelium/mud/coarse_dirt/rooted_dirt, sand/red_sand/sandstone/clay, gravel,
water, ice/packed_ice/snow_block/powder_snow, calcite, terracottas, cinnabar,
sulfur, …), cave-carver results (air, lava, cave_air) and aquifer fluids. Nothing
else is legal.

### Known non-worldgen artifact: obsidian

The recorded run contains **94 `minecraft:obsidian`** blocks out of ~226.5 M
(≈0.00004 %, all at y = −55/−56). They are *not* a feature/structure leak and not
worldgen at all:

* no worldgen class in the jar can place obsidian — a constant-pool scan for
  `Blocks.OBSIDIAN` hits only `LiquidBlock`/`BaseFireBlock`/pistons/portals and the
  End features/structure pieces, and structures are off;
* the positions are strictly lava-aquifer/water contacts near the bottom of the
  world (y = −55/−56 in the sampled chunks);
* the count is stable while chunks remain loaded (sampled every 10 s for 96 s) and
  identical for two frozen runs with different exposure (see Determinism), so the
  remaining conversions are one-shot events applied when a generated chunk is
  finalized, not an ongoing flow.

Without the `tick freeze` the number is larger and exposure-dependent — 218
obsidian in a run whose chunks stayed loaded for ~3 minutes versus 161 in a ~50 s
run (accompanying ~9 k water blocks of aquifer spread). Parity tooling should
either mask `minecraft:obsidian`, or use the 94 positions as the exact list of
fluid-interaction cells that a static generator cannot produce. The verify stage
reports it separately (`fluid_artifacts`) instead of lumping it in with feature
blocks.

### Determinism

Worldgen must not depend on how long the chunks stay loaded, or parity runs would
disagree for no reason. Measured with the frozen configuration:

* unfrozen runs drift with exposure: `obsidian` 218 (chunks loaded ~3 min) vs 161
  (~50 s), water ±9 k, and 9 `dirt`/`grass_block` blocks swapped by random-tick
  grass spreading — aquifers keep flowing and random ticks keep firing as long as
  the world ticks;
* frozen runs do not: the canonical `ref26.2` world and second, independent worlds
  built with `--settle-seconds 60` and `--settle-seconds 180` (the chunks held for
  one and three extra minutes before saving) agree on **0 of 2304 differing chunk
  digests** — every section's palette and packed block data, and every biome
  palette, is identical (obsidian 94, water 3 584 265, air 153 841 263 in all of
  them). Three independent frozen runs of the same seed therefore produced the same
  world.

The comparison was done with a throwaway script that SHA-256s every section's
`(Y, palette, data)` and biome palette per chunk; only the block data is compared,
so chunk metadata such as `InhabitedTime` cannot mask a real difference.

## Recorded run

Canonical run captured in `$CINDER_REF_DIR/run_canonical.log` (all times
2026-09-19):

| step | result |
|---|---|
| setup | deleted the previous world, wrote eula/properties/datapack, 66 biome overrides, pack format 107.1 |
| boot | `Done (2.3s)!`, data pack `file/refpack` loaded automatically |
| `tick freeze` | accepted (`The game is frozen`), no other console setup needed |
| 9 × `forceload add` | 256 chunks each, 2304 chunks ticketed |
| generation | 2304/2304 slots occupied, 2304/2304 `minecraft:full` at t = 56 s |
| total run stage | 60 s wall clock (server exited with code 0) |
| verify stage | 6.1 s |

Final statistics (also in `ref_verify.json`):

```
chunks              : 2304 scanned / 2304 expected (0 missing)
status histogram    : {'minecraft:full': 2304}
sections            : 55296
blocks counted      : 226,492,416
non-air Y range     : -64 .. 124
distinct biomes     : 12
   minecraft:beach, minecraft:birch_forest, minecraft:deep_ocean,
   minecraft:dripstone_caves, minecraft:flower_forest, minecraft:forest,
   minecraft:ocean, minecraft:old_growth_birch_forest, minecraft:plains,
   minecraft:river, minecraft:stony_shore, minecraft:sunflower_plains

153,841,263  minecraft:air            65,690  minecraft:tuff
 35,073,002  minecraft:deepslate        46,802  minecraft:granite
 30,122,860  minecraft:stone            17,654  minecraft:deepslate_iron_ore
  3,584,265  minecraft:water            14,361  minecraft:copper_ore
  1,769,119  minecraft:bedrock             343  minecraft:raw_iron_block
  1,211,639  minecraft:dirt                298  minecraft:raw_copper_block
    368,410  minecraft:grass_block          94  minecraft:obsidian
    117,984  minecraft:gravel
    103,899  minecraft:sand
     87,406  minecraft:lava
     67,327  minecraft:sandstone

unexpected blocks   : none
feature-like blocks : none
```

18 distinct block names, every one of them terrain/surface/carver/ore-vein output;
no logs, planks, leaves, saplings, flowers, ores (beyond the two noise-stage vein
types), geodes, moss, cobblestone or structure blocks — i.e. features and
structures produced nothing.

## Variant reference worlds (small: 8x8 chunks)

Two extra ground-truth worlds isolate the remaining worldgen differences without
paying for another 2304-chunk run. Both are **seed 1, overworld only, chunks
`x = 16..23, z = 8..15`** (64 chunks) and are built by
`bench/parity/gen_ref_variants.py`, which reuses `gen_ref.py`'s pipeline unchanged -
same datapack machinery, `tick freeze`, `save-all flush` polling until every chunk is
`minecraft:full`, `stop` - and only rebinds `REF_DIR`, the generated rectangle and
the datapack. Force-loading the whole rectangle is a single command:
`forceload add 256 128 383 255` -> `Marked 64 chunks ... from [16, 8] to [23, 15]`.

| variant | directory | datapack | worldgen settings |
|---|---|---|---|
| **carvers-off** | `/home/nataxcan/dev/ref-carvers-off26.2` | `refpack`: 66 biome overrides with features emptied **and** `"carvers": []` | `generate-structures=false`, `level-type=cinder_ref:no_structures` (identical to `ref26.2` apart from the motd) |
| **canonical** | `/home/nataxcan/dev/ref-canonical26.2` | none at all (server reports 1 data pack: vanilla) | `generate-structures=true`, `level-type=minecraft:normal` |

`carvers-off` answers "is a remaining block difference *carver logic* or an upstream
input?": with carving switched off, anything that still disagrees with Cinder cannot
come from carver logic. `canonical` is the vanilla-default world (features **and**
structures on), so the difference between it and `ref26.2` is exactly the feature
stage. "Canonical" means vanilla default *worldgen*: every other `server.properties`
key (view distance, `pause-when-empty-seconds=0`, difficulty, no nether, ...) and the
`tick freeze` are inherited from `ref26.2`, since without them the saved blocks stop
being a function of worldgen.

### Re-running

```sh
python3 bench/parity/gen_ref_variants.py                      # both worlds, setup + run + verify
python3 bench/parity/gen_ref_variants.py --variant carvers-off
python3 bench/parity/gen_ref_variants.py --variant canonical --stage setup|run|verify
python3 bench/parity/gen_ref_variants.py --compare            # no generation: status scan + air diff
```

Environment overrides: `CINDER_CARVERS_OFF_DIR`, `CINDER_CANONICAL_DIR` (defaults
above), `CINDER_REF_DIR` (the 48x48 world the `--compare` air stage reads), plus
everything `gen_ref.py` reads (`CINDER_REF_JAR`, `CINDER_REF_LIBS`,
`CINDER_REF_JAVA`). Each variant writes `server.log`, `ref_verify.json` and
`variant_summary.json` next to its world.

### `"carvers": []`, not `"carvers": {}`

In 26.2 the biome `carvers` field is a **carver id, a list of carver ids, or a step
map** - not a map of step -> list, and an empty map is not tolerated. A pack with
`"carvers": {}` is rejected outright:

```
[Worker-Main-3/ERROR]: Carver: Failed to parse either. First: Not a string: {};
Second: Failed to parse either. First: Not a json array: {}; Second: No key type in MapLike[{}]
...
[main/WARN]: Failed to load datapacks, can't proceed with server load.
Caused by: java.lang.IllegalStateException: Failed to parse minecraft:badlands from pack file/refpack
```

The valid empty form is the empty **list**, which is what vanilla itself uses for
its 6 carver-less biomes: of the 66 vanilla biome files, 55 carry
`["minecraft:cave", "minecraft:cave_extra_underground", "minecraft:canyon"]`,
5 a single `"minecraft:nether_cave"`, and 6 `[]`. The variant driver therefore
rewrites 60 files to `[]` (5 of them from the single-id form) and leaves the 6
already-empty ones alone.

### `gen_ref.py` changes needed by the variants

Both are inert for the 48x48 world (it still uses the 0..47 rectangle, and only
4/8-bit section palettes):

* the generated rectangle is now per-axis (`CHUNK_MIN`/`CHUNK_MAX` for x,
  `CHUNK_MIN_Z`/`CHUNK_MAX_Z` for z, defaulting to the old 0..47 for both), so a
  driver can ask for `x = 16..23, z = 8..15` with a single `forceload` tile;
* `section_counts`' generic bit-unpacking branch decoded the legacy cross-long
  stream. Vanilla uses `SimpleBitStorage` (`64 // bits` values per long, no
  boundary spanning), which `regions.py:unpack_long_array` already implements; the
  canonical world has 17..128-entry palettes (bits 5..7) and the old branch decoded
  garbage (`IndexError: list index out of range`) until it was fixed to match.

### Recorded runs (2026-09-19)

Full log: `/home/nataxcan/dev/ref-variants-both.log`; the two determinism re-runs are
in `/home/nataxcan/dev/det-carvers-off.log` and `/home/nataxcan/dev/det-canonical.log`.

| step | carvers-off | canonical |
|---|---|---|
| setup | 66 biome overrides, 60 rewritten to `"carvers": []`, pack format 107.1 | no datapack, `generate-structures=true`, `level-type=minecraft:normal` |
| `tick freeze` | accepted (`The game is frozen`) | accepted |
| `forceload add 256 128 383 255` | `Marked 64 chunks ... from [16, 8] to [23, 15]` | same |
| generation | 64/64 slots occupied, 64/64 `minecraft:full` at t = 13 s | 64/64 at t = 21 s |
| run stage | server exited 0 after 15.1 s, 5,169,152 B of region files | exited 0 after 23.2 s, 6,270,976 B |
| verify stage | 0.4 s | 1.7 s |

### Verification

`regions.py --scan` over the whole region directory (it also walks the chunks the
vanilla server always prepares around the global world spawn - identical in both
variants, and present in `ref26.2` too - which is why the totals exceed 64):

```
carvers-off : {'minecraft:structure_starts': 896, 'minecraft:biomes': 76,
               'minecraft:carvers': 71, 'minecraft:initialize_light': 60,
               'minecraft:full': 145}   (total 1248)
canonical   : (identical counts)        (total 1248)
ref26.2     : {'minecraft:structure_starts': 2112, 'minecraft:biomes': 228,
               'minecraft:carvers': 220, 'minecraft:initialize_light': 212,
               'minecraft:full': 2704}  (total 5476)
```

Restricted to the requested rectangle (`gen_ref_variants.py --compare` prints this
with the same `regions.py` reader), **all three worlds are
`{'minecraft:full': 64}` - 64/64**: the deliverable chunks live in `r.0.0.mca` and
each `ref_verify.json` reports `chunks: 64 scanned / 64 expected (0 missing)` with a
status histogram of exactly `{'minecraft:full': 64}`.

No cave air and a lower air count in `carvers-off` (same 64 chunks):

| | ref26.2 | carvers-off | delta |
|---|---|---|---|
| air (all) | 4,190,911 | 4,155,712 | **-35,199 (-0.84 %)** |
| `minecraft:cave_air` | 0 | 0 | 0 |
| air in the 4 sections below y = 0 | 24,103 | 17,200 | -6,903 |
| `minecraft:lava` / `minecraft:water` | 1,136 / 1,392 | 0 / 446 | -1,136 / -946 |
| `minecraft:stone` / `minecraft:deepslate` | - | +27,283 / +9,708 | caves filled back in |
| ore veins (noise stage) | - | - | unchanged (<32 blocks) |

The 35,199 missing air blocks plus the ~37 k extra stone/deepslate, 1,136 lava and
946 water are the carved cave volume: with `"carvers": []` the underground is solid
rock again, and the ref26.2-vs-carvers-off histogram difference is a pure cave
signature (no logs/leaves/ores moved; dirt +258, grass_block +32).

`carvers-off` block histogram (9 names, 6,291,456 blocks, 1536 sections, biome
`minecraft:forest`, non-air y = -64..76, `unexpected_blocks`/`feature_like_blocks`/
`fluid_artifacts` all empty):

```
     4,155,712  minecraft:air            48,981  minecraft:bedrock
     1,052,104  minecraft:deepslate       43,654  minecraft:dirt
       973,719  minecraft:stone           16,396  minecraft:grass_block
                                              446  minecraft:water
                                              417  minecraft:granite
                                               27  minecraft:copper_ore
```

`canonical` block histogram (105 distinct names, 6,291,456 blocks, biome
`minecraft:forest`, non-air y = -64..87, top of one recorded run):

```
     4,168,003  minecraft:air           18,658  minecraft:oak_leaves
       887,038  minecraft:deepslate      15,988  minecraft:grass_block
       703,215  minecraft:stone           8,560  minecraft:leaf_litter
        70,946  minecraft:andesite        6,452  minecraft:coal_ore
        70,265  minecraft:tuff            5,444  minecraft:copper_ore
        66,761  minecraft:diorite         3,990  minecraft:birch_leaves
        64,131  minecraft:granite         3,609  minecraft:polished_tuff
        62,587  minecraft:dirt            3,445  minecraft:iron_ore
        48,981  minecraft:bedrock         2,846  minecraft:waxed_oxidized_copper
        28,758  minecraft:gravel          2,639  minecraft:waxed_copper_block
        26,356  minecraft:tuff_bricks     2,210  minecraft:deepslate_redstone_ore
                                           1,896  minecraft:water
                                           1,654  minecraft:oak_log
                                           1,613  minecraft:chiseled_tuff_bricks
                                           1,586  minecraft:deepslate_diamond_ore
                                           1,544  minecraft:deepslate_gold_ore
                                           1,461  minecraft:deepslate_iron_ore
                                           1,379  minecraft:lava
                                           1,034  minecraft:smooth_basalt
```

Feature/structure blocks are present as expected: logs and leaves (`oak`, `birch`),
`short_grass`, `leaf_litter`, the full vanilla ore set (`coal`/`copper`/`iron`/
`gold`/`redstone`/`lapis`/`diamond`, stone and deepslate variants), amethyst geodes
(`budding_amethyst`, `amethyst_cluster`, buds, `calcite`, `smooth_basalt`) and trial
chambers (`tuff_bricks`, `polished_tuff`, `chiseled_tuff`, `vault`, `trial_spawner`,
`copper`/`waxed_*` fittings, `decorated_pot`, `cobweb`, beds, `ladder`). The verify
buckets `unexpected_blocks` / `feature_like_blocks` are therefore non-empty for this
world by design - here they *are* the evidence that features and structures ran.
The requested rectangle covers a single biome (`minecraft:forest`) in all three
worlds, so no water-lily or other-biome vegetation appears.

### Determinism of the variants

Measured by building each variant a second time into a throwaway directory
(`CINDER_CARVERS_OFF_DIR` / `CINDER_CANONICAL_DIR`) and comparing every section's
`(Y, palette, data)` and every biome palette per chunk - block content only, no
chunk metadata:

| world | chunks differing (of the 907 stored in `r.0.0.mca`) | inside x=16..23 z=8..15 |
|---|---|---|
| carvers-off | 0 | 0 / 64 |
| canonical | 208 | 61 / 64 |

* **carvers-off is byte-identical across runs**, so carver output can be diffed
  against it block by block; the features-off determinism result recorded above for
  `ref26.2` carries over to the variant rectangle.
* **canonical is not reproducible, and that is inherent to vanilla rather than to the
  harness**: two runs of the same seed differ in 61 of the 64 chunks, with a total
  absolute difference of **4,774 blocks (0.076 %)** over the 6,291,456 blocks, and
  the movers are exactly the feature-stage blocks - `air` -2,232, `oak_leaves`
  +1,657, `leaf_litter` +243, `birch_leaves` +211, `oak_log` +155, `short_grass` -22,
  `andesite`/`diorite` +95/-97 (the equivalent of a few dozen trees either way).
  Features may write into the neighbouring chunk (radius 1) during the feature
  stage, so the order in which neighbouring chunks run decides e.g. whether a
  spilled tree canopy suppresses a vegetation patch, and the chunk system does not
  fix that order. Serializing it with `-Dmax.bg.threads=1` narrows the spread
  (49-59 of the 64 chunks differed in the same test) but does not remove it, so the
  delivered worlds use the default parallel configuration.
  Consequence for parity: a Cinder-vs-canonical diff has a ~0.08 % noise floor in
  vegetation/ore-feature blocks and must be read outside it, while
  `ref26.2`/`carvers-off` stay exact.

## Region file layout

26.2 writes dimension data under `world/dimensions/<dimension>/`:

```
world/dimensions/minecraft/overworld/region/r.<x>.<z>.mca
```

`world/region` does **not** exist in a vanilla 26.2 world. For the parity tooling
(and the acceptance criterion) the run stage additionally creates a compatibility
symlink **after the server has stopped**:

```
world/region -> world/dimensions/minecraft/overworld/region
```

It is created only when the server is down, so world loading can never see it, and
never replaces a real directory. Set `CINDER_REF_DIMENSION` to point the reader at a
different dimension.

## Files

| path | contents |
|---|---|
| `bench/parity/gen_ref.py` | the whole pipeline (setup / run / verify) |
| `bench/parity/gen_ref_variants.py` | the two small variant worlds (setup / run / verify / compare) |
| `/home/nataxcan/dev/ref-carvers-off26.2/**`, `/home/nataxcan/dev/ref-canonical26.2/**` | variant worlds (`world/`, `ref_verify.json`, `variant_summary.json`, `server.log`) |
| `/home/nataxcan/dev/ref-variants-both.log` | captured stdout of the recorded both-variants run |
| `bench/parity/ref_config.md` | this document |
| `$CINDER_REF_DIR/server.properties`, `eula.txt`, `server.jar` (symlink) | server config |
| `$CINDER_REF_DIR/world/datapacks/refpack/**` | the datapack (preset + 66 biome overrides) |
| `$CINDER_REF_DIR/world/...` | the world (overworld region files) |
| `$CINDER_REF_DIR/server.log` | full server log of the recorded run |
| `$CINDER_REF_DIR/run_canonical.log` | captured stdout of the recorded `gen_ref.py` run |
| `$CINDER_REF_DIR/ref_verify.json` | verification summary (statuses, histogram, biomes, buckets) |
