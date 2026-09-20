# World generation - Cinder vs Pumpkin

Regenerate with `python3 scripts/world_bench.py`; the full write-up is in [`WORLD.md`](WORLD.md).

Same box, same grid: **32x32 = 1024 chunks** around origin.

- **Cinder**: `./cinder --threads N` printing `world ready` - the whole vanilla 26.2 stage list (biomes, density, aquifers, ore veins, surface rules, carvers, vanilla features, lighting) into memory, then the listener comes up.
- **Pumpkin**: `pumpkin-gen 32 N`, `generate_single_chunk(..., Full)` - its own feature engine, in memory.

| Threads | Cinder median | Cinder best | Pumpkin median | Pumpkin best | Pumpkin time / Cinder time (best; lower = Pumpkin faster) |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 96.65 s | 94.60 s | 24.14 s | 24.14 s | 0.26x |
| 4 | 27.94 s | 27.92 s | 7.17 s | 6.96 s | 0.25x |
| 8 | 17.51 s | 17.39 s | 4.88 s | 4.82 s | 0.28x |
| 16 | 14.30 s | 14.08 s | 5.10 s | 4.84 s | 0.34x |

## The GPU row

Cinder's generator is C behind one IO effect, so it is not a GPU target - Bend only
hands `!` (parallel call) to the device and nothing in the pipeline uses it. The
Bend-side heightmap microbench (`src/world.bend`) does run there, on a different
workload, so it is listed separately and left out of the ratios above:

| Heightmap microbench | CPU 8 threads | GPU `--gpu 4GB` | speedup |
| --- | ---: | ---: | ---: |
| 32x32 chunks | 19 ms | ~2.4 s | 0.008x (device fixed cost dominates) |
| 256x256 = 65 536 chunks | 113.6 s | **59.2 s** | **1.92x** |

Both modes produce identical checksums (16647010 and 1065330270). See
`bench/WORLD.md` for the toolchain and the WSL2 caveat.

At the thread count both scale to (8): Cinder 17.51 s vs Pumpkin 4.88 s, i.e. Pumpkin is **3.59x faster**.

