# What a Bend worldgen would take

Cinder's server is Bend (`src/server.bend`, `codec.bend`, `proto.bend`, `net.bend`,
1 068 lines). The **generator is C** — 13 630 lines across `src/vanilla_*.c` —
reached through one `import "./vanilla_gen.c"` in `src/world.bend`, because Bend
inlines a foreign `.c` file into its own translation unit.

This file records what happened when I tried to move that generator into Bend:
what the language allows, what it forbids, and what a port would actually cost.
Every claim below was measured on bend 2.0.5 / clang 14 / this box, and the
reproductions are in `bench/parity/`.

## Correcting an earlier claim

I first reported that Bend's checker caps array sizes at ~4 096 and that a
chunk-sized buffer is inexpressible. **That was wrong.** `Array.new`'s second
argument is a **depth**, not an element count:

```bend
Array.new(U32, 2n, 0)   # size=4        (2^2)
Array.new(U32, 8n, 0)   # size=256      (2^8)
Array.new(U32, 17n, 0)  # size=131072   (2^17)  <- chunk-sized
Array.new(U32, 20n, 0)  # size=1048576  (2^20)
```

(`bench/parity/bend_array_depth.sh`.) My failing programs were asking for
`Array.new(U32, 98304n, 0)`, i.e. 2^98304 elements — hence "a literal too large to
expand" and "an array past the deepest block class 31". A chunk is 98 304 blocks,
so **depth 17 is chunk-sized and comfortably inside the 31-level limit.**

## What Bend's rules cost a port

Measured constraints, each with the error it produces:

| Rule | Consequence for the generator |
| --- | --- |
| `Array<T>` is a `Type` with exactly one owner; affine by default | The buffer is threaded linearly — "a write hands back the same array". **Two references are impossible without `Array.clone`.** The generator reads neighbouring blocks while writing the current chunk, and a tree at a chunk border writes into the *next* chunk: both need two references. |
| `+x` makes a variable reusable, and only for `Data`-kinded types | `+i: U32` is fine; `+buf: Array<U32>` is not. Every helper that touches the buffer must return it. |
| `match` scrutinizes a parameter, not a computed value or a local binder | Index-driven loops cannot match on `U32.is_lt(i, n)`; loops must recurse on a parameter. |
| Binders must be consumed in parameter order | The `Array & T` pair `Array.get` returns has to be the *first* parameter of the next def, opened before any `match`. |
| Local binders cannot be type-annotated inside a match arm | `l : Array<U32> = f(...)` is a parse error; every let must be inferable. |
| No top-level constants (`def`/`type`/`law` only) | Constants are inlined at each use. |
| Literals default to `Nat`; U32 positions need `(0 : U32)` | Every call site is annotated. |
| Large `Nat` literals expand in the checker | A 98 304-deep loop overflows ("the machine stack overflowed"); loops must be wide (depth ~17) rather than deep. |
| A pure binder in a `do IO` block only parses in some positions | Several arrangements of the same three-line `main` are rejected ("expected: a pattern"). |

The last row is where my benchmark attempt ended: I could not get a 30-line
"allocate a chunk-sized array, do N writes, time it" program to build. The pieces
that do work (`bench/parity/bend_array_depth.sh`) and the pieces that do not
(`bend_array_bench.bend`, `bend_write_syntax.sh`) are both in the tree.

## What this means

A full-Bend generator is not a port. Vanilla's stages are imperative and aliasing:
`fill` writes 98 304 blocks per chunk in scan order, carvers mutate a shared
17×17-chunk neighbourhood, and features write into the neighbouring chunk. Bend's
value model says a buffer has one owner and cannot be observed twice, so each of
those becomes either a linear threading of the buffer through every call (possible,
verbose) or a redesign of the algorithm (which breaks the parity the project is
built on).

The honest estimate, in order of increasing cost:

1. **Density graph, noise, biomes, surface rules** — pure functions of (x, y, z,
   seed) with no buffers. ~2 500 of the 13 630 C lines. Portable to Bend today,
   and the checker would actually verify them.
2. **Column assembly** — turning per-position densities into a block column. Needs
   the chunk buffer, so it needs the linear-threading style, but only one writer
   at a time. Doable, awkward.
3. **Carvers and features** — the aliasing and mutation core, ~7 000 lines. Either
   a redesign under affine typing, or they stay C.
4. **Structures** (not implemented yet in either language) — the same problem plus
   NBT templates.

## What I would do next

Slice 1 only, verified against the C implementation with the existing parity
harness (`CINDER_DUMP` + `bench/parity/diff.py`): a Bend density-function
evaluator that reproduces `final_density`/`slope`/`preliminary_surface_level`
bit-for-bit at sampled coordinates, with `bench/parity/df_check.sh` as the oracle.
That grows the Bend share without touching parity, and it establishes the idioms
(parameter-threaded buffers, wide recursion, annotated literals) that slices 2–3
would need.
