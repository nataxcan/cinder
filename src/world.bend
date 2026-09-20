# Parallel terrain.
#
# A chunk is a 16x16 heightmap. Heights are generated independently, so
# Bend spreads the work across every CPU core — or the GPU, via `!`.
import Base

# Mix a lattice point into a 32-bit hash. All arithmetic is wrapping U32.
def World.mix(+x: U32, +z: U32, +seed: U32) -> U32:
  +n = (x * 374761393 + z * 668265263 + seed * 1442695041 : U32)
  +n2 = (n .^. U32.shrn(n, 13n) : U32)
  +n3 = (n2 * 1274126177 : U32)
  (n3 .^. U32.shrn(n3, 16n) : U32)

# Several hashes per column so a chunk is real work, not one multiply.
def World.fbm(n: Nat, +x: U32, +z: U32, +seed: U32, acc: U32) -> U32:
  match n:
    case 0n:
      acc
    case 1n++p:
      +h = World.mix(x, z, seed)
      World.fbm(p, (x + 19 : U32), (z + 37 : U32), (seed + 101 : U32), (acc + h : U32))

# Surface height in [48, 79]. Flat enough to walk, noisy enough to see.
def World.height(+x: U32, +z: U32, +seed: U32) -> U32:
  (48 + U32.and(World.fbm(8n, x, z, seed, 0), 31) : U32)

# Column index 0..255 of a chunk -> world-space height.
def World.column(+i: U32, +cx: U32, +cz: U32, +seed: U32) -> U32:
  +lx = U32.and(i, 15)
  +lz = U32.and(U32.shrn(i, 4n), 15)
  +wx = (U32.shln(cx, 4n) + lx : U32)
  +wz = (U32.shln(cz, 4n) + lz : U32)
  World.height(wx, wz, seed)

# 16x16 = 256 columns, filled sequentially. Parallelism lives in World.grid.
def World.fill(d: Nat, +base: U32, +cx: U32, +cz: U32, +seed: U32) -> Array<U32>:
  match d:
    case 0n:
      ALeaf{World.column(base, cx, cz, seed)}
    case 1n++p:
      +half = U32.shln(1, p)
      l = World.fill(p, base, cx, cz, seed)
      r = World.fill(p, (base + half : U32), cx, cz, seed)
      ANode{l, r}

def World.chunk(+cx: U32, +cz: U32, +seed: U32) -> Array<U32>:
  World.fill(8n, 0, cx, cz, seed)

# Fold a heightmap into a checksum. Used by the bench and by tests.
def World.checksum(a: Array<U32>) -> U32:
  match a:
    case ALeaf{x}:
      x
    case ANode{xs, ys}:
      (World.checksum(xs) + World.checksum(ys) : U32)

# 2^d by 2^d chunks, generated in parallel, checksummed.
def World.grid(d: Nat, +seed: U32, +ox: U32, +oz: U32) -> U32:
  match d:
    case 0n:
      World.checksum(World.chunk(ox, oz, seed))
    case 1n++p:
      +half = U32.shln(1, p)
      a b = World.grid(p, seed, ox, oz) World.grid(p, seed, (ox + half : U32), oz)
      c e = World.grid(p, seed, ox, (oz + half : U32)) World.grid(p, seed, (ox + half : U32), (oz + half : U32))
      (((a + b : U32) + c : U32) + e : U32)

# Materialize a column: world Y -64..319 stored as index 0..383.
# Blocks: 0 air, 1 bedrock, 2 stone, 3 dirt, 4 grass.
def World.block.dirt(+yi: U32, +surf: U32, is_d: Bool) -> U32:
  match is_d:
    case True{}:
      3
    case False{}:
      2

def World.block.grass(+yi: U32, +surf: U32, is_g: Bool) -> U32:
  match is_g:
    case True{}:
      4
    case False{}:
      World.block.dirt(yi, surf, U32.is_ge(yi, U32.sub(surf, 2)))

def World.block.bed(+yi: U32, +surf: U32, is_bed: Bool) -> U32:
  match is_bed:
    case True{}:
      1
    case False{}:
      World.block.grass(yi, surf, U32.is_eq(yi, surf))

def World.block.air(+yi: U32, +surf: U32, is_air: Bool) -> U32:
  match is_air:
    case True{}:
      0
    case False{}:
      World.block.bed(yi, surf, U32.is_eq(yi, 0))

def World.block(+yi: U32, +h: U32) -> U32:
  +surf = (h + 64 : U32)
  World.block.air(yi, surf, U32.is_lt(surf, yi))

# Walk 384 Y levels of one column; acc is a fingerprint of every block id.
def World.column_fp(+h: U32, fuel: Nat, +yi: U32, acc: U32) -> U32:
  match fuel:
    case 0n:
      acc
    case 1n++p:
      World.column_fp(h, p, (yi + 1 : U32), (acc + World.block(yi, h) : U32))

def World.fold_heights(a: Array<U32>, +yfuel: Nat, acc: U32) -> U32:
  match a:
    case ALeaf{+h}:
      World.column_fp(h, yfuel, 0, acc)
    case ANode{xs, ys}:
      World.fold_heights(xs, yfuel, World.fold_heights(ys, yfuel, acc))

def World.chunk_stats(+cx: U32, +cz: U32, +seed: U32, +yfuel: Nat) -> U32:
  World.fold_heights(World.chunk(cx, cz, seed), yfuel, (cx * 131 + cz : U32))

# 2^d by 2^d chunks, each filled with 384-block columns (Y -64..319).
def World.grid_full(d: Nat, +seed: U32, +ox: U32, +oz: U32, +yfuel: Nat) -> U32:
  match d:
    case 0n:
      World.chunk_stats(ox, oz, seed, yfuel)
    case 1n++p:
      +half = U32.shln(1, p)
      a b = World.grid_full(p, seed, ox, oz, yfuel) World.grid_full(p, seed, (ox + half : U32), oz, yfuel)
      c e = World.grid_full(p, seed, ox, (oz + half : U32), yfuel) World.grid_full(p, seed, (ox + half : U32), (oz + half : U32), yfuel)
      (((a + b : U32) + c : U32) + e : U32)

# 32x32 = 1024 vanilla Full chunks (biomes, noise, aquifers, surface,
# carvers, ores/trees, lighting). Host: src/vanilla_gen.c
#
# This is the real generator and it is C, so it is not a GPU target: Bend only
# hands "!" calls to the device, and the whole pipeline runs behind one IO
# effect. World.grid_gpu below is the Bend-side path to the GPU.
def World.generate(seed: U32) -> IO(U32):
  import "./vanilla_gen.c"
  import "./vanilla_gen.js"

# The heightmap microbench, handed to the GPU.
#
# The "!" marks a parallel call: bend builds a companion <binary>.gpu program for
# it, and <binary> --gpu 4GB runs these calls on the device (Metal on macOS, CUDA
# 12 at /usr/local/cuda on Linux, clang 19+ to build). See
# bench/parity/gpu_probe.sh for the toolchain check and the device requirement.
def World.grid_gpu(d: Nat, +seed: U32, +ox: U32, +oz: U32) -> U32:
  World.grid!(d, seed, ox, oz)

# A height is always in [48, 79]: the mask is 5 bits.
def World.height_ok(+h: U32) -> Bool:
  Bool.and(U32.is_ge(h, 48), U32.is_le(h, 79))

def World.heights_ok(a: Array<U32>) -> Bool:
  match a:
    case ALeaf{+x}:
      World.height_ok(x)
    case ANode{xs, ys}:
      Bool.and(World.heights_ok(xs), World.heights_ok(ys))
