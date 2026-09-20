# World generation — Cinder vs Pumpkin vs Folia vs C2ME

Same machine: AMD Ryzen 7 5700G (8 cores / 16 threads), 47 GiB RAM. Same target: **1024 chunks** (32×32).

| | Cinder | Pumpkin | Folia 26.2-7 | C2ME 0.4.2-α.0.52 |
| --- | --- | --- | --- | --- |
| Runtime | native C (Bend) | native Rust | JVM Java 25 | JVM Java 25 + Fabric |
| Terrain | vanilla 26.2 density graph + Full stages | vanilla-like 3D noise + features | vanilla (caves, ores, biomes) | vanilla (parity by default) |
| Threads used | 8 / 16 | 8 / 16 | Moonrise 8 workers + Folia regions | C2ME 8 workers |
| Persistence | in-memory fingerprint | in-memory | Anvil region files | Anvil region files |
| 1024-chunk gen | **19.90 s** @ 8t / **16.12 s** @ 16t | **4.88 s** @ 8t / 5.10 s @ 16t | **11.9 s** | **8.62 s** |
| vs Cinder 8t | 1× | 4.08× faster (3.15× @ 16t) | **1.67× faster** | **2.31× faster** |
| JVM/`Done` only | — | 0.43 s listen, no spawn chunks | **10.7 s** | **13.3 s** |
| Spawn → 1024 chunks | **20.2 s** @ 8t / **15.1 s** @ 16t (gen then ping) | ~5.2 s if you gen then boot | **23.3 s** | **21.6 s** |

[C2ME](https://github.com/RelativityMC/C2ME-fabric) is RelativityMC’s Fabric concurrent chunk engine (gen, I/O, loading). On this Anvil pregen it is **1.38× faster** than Folia, **1.77× slower** than Pumpkin, and **2.03× faster** than Cinder’s in-memory Full gen at 8 threads (Cinder’s numbers are from the current build, which runs the vanilla feature port; see [`WORLD.md`](WORLD.md)).

## C2ME samples (3 cold worlds)

| Run | `Done` | forceload → 1024 on disk | spawn → 1024 |
| ---: | ---: | ---: | ---: |
| 1 | 13.6 s | 8.72 s | 22.3 s |
| 2 | 13.0 s | 8.62 s | 21.6 s |
| 3 | 13.3 s | 8.25 s | 21.6 s |
| median | 13.3 s | 8.62 s | 21.6 s |

Stack: C2ME `0.4.2-alpha.0.52+26.2`, Fabric loader 0.19.5, Fabric API `0.161.0+26.2`, Lithium `0.25.3+mc26.2` (C2ME’s recommended pair). Java 25 Temurin, `-Xms2G -Xmx4G`, `globalExecutorParallelism = 8`. Port 25567.

`forceload` is capped at 256 chunks per command, so the bench issues the same four 16×16 quadrants as Folia. 26.2 writes overworld Anvil at `world/dimensions/minecraft/overworld/region/`. Shutdown saved 3364 block chunks (ticket radius), same pattern as Folia.

MCA counts stay near 1 until `save-all flush` (2 s cadence), then jump. The number is **time until 1024 chunks exist on disk**, matching Folia, not a CPU-only generate.

No [c2me-ocl](https://modrinth.com/mod/c2me-ocl). That is a separate OpenCL accelerator; this run is CPU C2ME.

## What this is not

- Cinder runs the vanilla 26.2 Full stages including the feature stage (`FeatureSorter` over the real biome feature lists, plus ports of the overworld's configured features). Not bitwise-identical to Java: structures are not generated and a few thousand feature blocks land differently — 97.72 % block agreement against a canonical vanilla world, see [`WORLD.md`](WORLD.md). Pumpkin is vanilla-like; Folia/C2ME are vanilla + Anvil.
- C2ME `Done (2.6s)` in the Minecraft log is after world prepare. The 13.3 s above is process spawn → that line (JVM + Fabric + mixins). 26.2 has no spawn chunks (`Loading 0 persistent chunks`).
- Folia is designed to tick many regions with players online. C2ME is designed to scale chunk gen/IO. Neither is trying to beat a 21 ms heightmap.

## Reproduce

```bash
bash scripts/fetch_c2me.sh
python3 scripts/c2me_bench.py
```
