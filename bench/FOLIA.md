# World generation — Cinder vs Pumpkin vs Folia vs C2ME

Same machine: AMD Ryzen 7 5700G (8 cores / 16 threads), 47 GiB RAM. Same target: **1024 chunks** (32×32). C2ME details: [`C2ME.md`](C2ME.md).

| | Cinder | Pumpkin | Folia 26.2-7 | C2ME 0.4.2-α.0.52 |
| --- | --- | --- | --- | --- |
| Runtime | native C (Bend) | native Rust | JVM Java 25 | JVM Java 25 + Fabric |
| Terrain | vanilla 26.2 density graph + every Full stage, features included | vanilla-like 3D noise + features | vanilla (caves, ores, biomes) | vanilla (parity by default) |
| Threads used | 8 / 16 | 8 / 16 | Moonrise 8 workers + Folia regions | C2ME 8 workers |
| Persistence | in-memory fingerprint | in-memory | Anvil region files | Anvil region files |
| 1024-chunk gen | **19.90 s** @ 8t / **16.12 s** @ 16t | **4.88 s** @ 8t / 5.10 s @ 16t | **11.9 s** | **8.62 s** |
| vs Cinder 8t | 1× | 4.08× faster (3.15× @ 16t) | **1.67× faster** | **2.31× faster** |
| JVM/`Done` only | — | 0.43 s listen, no spawn chunks | **10.7 s** | **13.3 s** |
| Spawn → 1024 chunks | **20.2 s** @ 8t / **15.1 s** @ 16t (gen then ping) | ~5.2 s if you gen then boot | **23.3 s** | **21.6 s** |

Cinder's numbers are from the current build (vanilla feature port in place); see
[`WORLD.md`](WORLD.md) for the parity behind them and the per-stage split
(surface rules 11.1 s of the 17.5 s, features 2.7 s).

Folia is Paper’s region-threaded Java server. C2ME is Fabric’s concurrent chunk-gen/IO mod. Folia is about **2.4× slower** than Pumpkin on raw generation; against Cinder it is now **1.20× faster** at 16 threads (11.9 s vs 14.30 s), and slower only on time-to-world: 23.3 s spawn → 1024 chunks against Cinder’s 13.54 s. Both Java servers also write disk and load ticket-radius neighbors (shutdown saved ~2200–3364 block chunks).

## Folia samples (3 cold worlds)

| Run | `Done` | forceload → 1024 on disk | spawn → 1024 |
| ---: | ---: | ---: | ---: |
| 1 | 10.5 s | 10.4 s | 20.8 s |
| 2 | 11.4 s | 11.9 s | 23.3 s |
| 3 | 10.7 s | 13.3 s | 23.9 s |
| median | 10.7 s | 11.9 s | 23.3 s |

`forceload` is capped at 256 chunks per command, so the bench issues four 16×16 quadrants. Moonrise was set to 8 worker threads. Autosave 1 s so the Anvil count is not stuck on the default 5-minute flush.

## What this is not

- Cinder runs the vanilla 26.2 Full stages including the feature stage (`FeatureSorter` over the real biome feature lists, plus ports of the overworld's configured features). Not bitwise-identical to Java: structures are not generated and a few thousand feature blocks land differently — 97.72 % block agreement against a canonical vanilla world, see [`WORLD.md`](WORLD.md). Folia and C2ME write Anvil; Pumpkin and Cinder are in-memory.
- Folia `Done (10s)` is datapacks, recipes, spawn search, and the JVM — not 1024 chunks. Spawn prep was ~2.5 s of that.
- Folia is designed to tick many regions at once with players online, not to win a 1024-chunk pregen race.

## Reproduce

```bash
# Folia 26.2-7 + Java 25
python3 scripts/folia_bench.py
```
