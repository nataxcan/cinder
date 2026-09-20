# Cinder

A Minecraft Java server written in [Bend](https://github.com/bendlang/bend).

When using Bend:
- run `bend guide` to learn it
- use `LAWS.bend` to keep important rules
- run `bend PROOF.bend` before committing
- parallelize the code whenever possible

## Layout

- `src/codec.bend` — VarInt, packet framing (pure)
- `src/proto.bend` — Minecraft 26.2 / protocol 776 packets (pure)
- `src/world.bend` — vanilla Full worldgen via `src/vanilla_gen.c`; heightmap microbench still pure
- `src/net.bend` — binary TCP (`List<U32>` bytes, not UTF-8 strings)
- `src/server.bend` — handshake / status / login / config / play loop
- `src/main.bend` — server entry
- `src/bench.bend` — worldgen throughput bench
- `src/vanilla_gen.c` — worldgen orchestrator: JSON, noising setup, chunk driver, `--dump`
- `src/vanilla_common.h` — shared types/contract for every worldgen module
- `src/vanilla_rand_noise.c` — Xoroshiro128++/positional factories, legacy Java random, perlin/normal/blended noise
- `src/vanilla_df.c` — density-function compile/eval, per-chunk caches
- `src/vanilla_biomes.c` — vanilla multi-noise parameter list + `Climate.RTree`, biome fill
- `src/vanilla_biome_manager.c` — `BiomeManager` fuzzy zoom lookup (SHA-256 seed, fiddled corners)
- `src/vanilla_fill.c` — noise fill: aquifer, ore veins, `find_top_surface`, WG heightmaps
- `src/vanilla_surface.c` — vanilla `SurfaceSystem` + `surface_rule` tree
- `src/vanilla_carvers.c` — vanilla cave/canyon carving over the 17×17 source-chunk neighbourhood
- `src/vanilla_features.c` — ores/trees/ground cover (Cinder's own; not a vanilla port)
- `src/vanilla_biomes.h` — generated parameter table (`bench/parity/gen_biome_table.py`)
- `tests/` — checker-normalized unit tests
- `bench/` — cross-server benchmarks; `bench/WORLD.md` has the parity numbers
- `bench/parity/` — vanilla reference generation, region decoder, dump comparator, primitive probes
- `web/` — landing page

## Commands

```
bend tests/codec.bend
bend tests/world.bend
bend tests/proto.bend
bend src/server.bend          # typecheck only (no main)
bend src/main.bend -o cinder  # native binary (clang 14+)
bend src/bench.bend -o cinder-bench
./cinder --threads 8
./cinder-bench --gpu 4GB      # clang 19+ / CUDA 12
bash scripts/build.sh         # proofs + tests + both binaries
bash scripts/check_c.sh       # syntax-check every C module (fast)
bash scripts/check_worldgen_checksums.sh   # guard: seed-1 32x32 checksums must not change
bash scripts/bench_cinder_worldgen.sh 8 16
```

Bend has no Windows host; build and run under WSL or Linux. The C modules are
merged into one translation unit by `src/vanilla_gen.c`, so they use absolute
`CINDER_INC_*` include macros (Bend inlines the file into a temp directory).
