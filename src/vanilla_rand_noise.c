
// Vanilla 26.2 random sources and coherent noise: Xoroshiro/Legacy RNGs,
// ImprovedNoise, PerlinNoise, NormalNoise and BlendedNoise.
//
// Ported bit-for-bit from the decompiled 26.2 sources:
//   world/level/levelgen/Xoroshiro128PlusPlus.java
//   world/level/levelgen/XoroshiroRandomSource.java (+ XoroshiroPositionalRandomFactory)
//   world/level/levelgen/RandomSupport.java      (upgradeSeedTo128bit, mixStafford13, MD5 seeds)
//   world/level/levelgen/LegacyRandomSource.java
//   world/level/levelgen/BitRandomSource.java    (next(bits)/nextInt/nextLong/nextFloat/nextDouble)
//   world/level/levelgen/WorldgenRandom.java     (setLargeFeatureSeed/setDecorationSeed)
//   util/Mth.java                                (smoothstep, lerp3, floor, lfloor, getSeed)
//   world/level/levelgen/synth/ImprovedNoise.java
//   world/level/levelgen/synth/PerlinNoise.java
//   world/level/levelgen/synth/NormalNoise.java
//   world/level/levelgen/synth/BlendedNoise.java
//   world/level/levelgen/synth/SimplexNoise.java (GRADIENT table + dot)
//
// Java semantics are preserved literally, not "cleaned up": evaluation order,
// float-vs-double (nextFloat multiplies in float; nextDouble converts to double
// and multiplies by the double 1.1102230246251565E-16 — the decompiler renders
// that constant as "1.110223E-16F" but the class file holds a double, confirmed
// with javap -c; the ImprovedNoise y-fudge epsilon is the float 1.0E-7F widened
// to double) and wrapping integer overflow all match the JVM.
//
// MD5 is plain RFC 1321 (it stands in for Guava's Hashing.md5()); RandomSupport
// takes the two halves of the 16-byte digest big-endian (Longs.fromBytes).
//
// This file is included by src/vanilla_gen.c, but is also linked on its own by
// bench/parity/noise_check.sh. The include guard below is what lets the parity
// harness include this file for documentation while the emitted symbols come
// from the separately compiled translation unit.

#ifndef CINDER_VANILLA_RAND_NOISE_C
#define CINDER_VANILLA_RAND_NOISE_C

/* Bit-exactness beats FMA: contraction of "p0 + a * (p1 - p0)" style expressions
   changes the low bits versus Java's strictly-sequenced double math. This file is
   included inside the worldgen translation unit, so contraction stays OFF for the
   density-function code that follows rather than being restored to the default. */
#if defined(__clang__) || defined(__GNUC__)
#pragma STDC FP_CONTRACT OFF
#endif

/* Bend inlines this file into a temporary directory, so sibling includes are
 * absolute - the same convention the generated embed.h include already uses. */
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H

/* ========================================================== small helpers */

static uint64_t rn_rotl64(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

#define RN_GOLDEN 0x9E3779B97F4A7C15ULL /* RandomSupport.GOLDEN_RATIO_64 */
#define RN_SILVER 0x6A09E667F3BCC909ULL /* RandomSupport.SILVER_RATIO_64 */

/* RandomSupport.mixStafford13 */
static uint64_t rn_mix_stafford13(uint64_t z) {
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; /* -4658895280553007687L */
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; /* -7723592293110705685L */
  return z ^ (z >> 31);
}

/* =============================================================== MD5 (RFC 1321) */

static uint32_t rn_rotl32(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

static void rn_md5(const uint8_t *msg, size_t len, uint8_t out[16]) {
  static const uint32_t K[64] = {
      0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au,
      0xa8304613u, 0xfd469501u, 0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
      0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u, 0xf61e2562u, 0xc040b340u,
      0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
      0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u,
      0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
      0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u, 0x289b7ec6u, 0xeaa127fau,
      0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
      0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u,
      0xffeff47du, 0x85845dd1u, 0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
      0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u};
  static const int S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                            5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
  uint32_t h0 = 0x67452301u, h1 = 0xefcdab89u, h2 = 0x98badcfeu, h3 = 0x10325476u;
  size_t nblocks = (len + 8) / 64 + 1; /* padded length = nblocks * 64 */
  uint8_t block[64];

  for (size_t b = 0; b < nblocks; b++) {
    size_t base = b * 64;
    memset(block, 0, sizeof block);
    for (int i = 0; i < 64; i++) {
      size_t idx = base + (size_t)i;
      if (idx < len) {
        block[i] = msg[idx];
      } else if (idx == len) {
        block[i] = 0x80;
      } else if (idx >= nblocks * 64 - 8) {
        /* message length in bits, little-endian */
        block[i] = (uint8_t)((uint64_t)len * 8u >> (8 * (idx - (nblocks * 64 - 8))));
      }
    }
    uint32_t m[16];
    for (int i = 0; i < 16; i++) {
      m[i] = (uint32_t)block[4 * i] | ((uint32_t)block[4 * i + 1] << 8) |
             ((uint32_t)block[4 * i + 2] << 16) | ((uint32_t)block[4 * i + 3] << 24);
    }
    uint32_t a = h0, b_ = h1, c = h2, d = h3;
    for (int i = 0; i < 64; i++) {
      uint32_t f;
      int g;
      if (i < 16) {
        f = (b_ & c) | (~b_ & d);
        g = i;
      } else if (i < 32) {
        f = (d & b_) | (~d & c);
        g = (5 * i + 1) & 15;
      } else if (i < 48) {
        f = b_ ^ c ^ d;
        g = (3 * i + 5) & 15;
      } else {
        f = c ^ (b_ | ~d);
        g = (7 * i) & 15;
      }
      uint32_t tmp = d;
      d = c;
      c = b_;
      b_ = b_ + rn_rotl32(a + f + K[i] + m[g], S[i]);
      a = tmp;
    }
    h0 += a;
    h1 += b_;
    h2 += c;
    h3 += d;
  }

  uint32_t h[4] = {h0, h1, h2, h3};
  for (int i = 0; i < 4; i++) {
    out[4 * i] = (uint8_t)h[i];
    out[4 * i + 1] = (uint8_t)(h[i] >> 8);
    out[4 * i + 2] = (uint8_t)(h[i] >> 16);
    out[4 * i + 3] = (uint8_t)(h[i] >> 24);
  }
}

/* ================================================ Xoroshiro128++ (java.util style) */

/* Xoroshiro128PlusPlus: constructor zero-seed fallback included. */
void xoro_set_seed(Xoro *r, uint64_t lo, uint64_t hi) {
  if ((lo | hi) == 0) {
    lo = RN_GOLDEN;
    hi = RN_SILVER;
  }
  r->lo = lo;
  r->hi = hi;
}

uint64_t xoro_next_long(Xoro *r) {
  uint64_t s0 = r->lo;
  uint64_t s1 = r->hi;
  uint64_t result = rn_rotl64(s0 + s1, 17) + s0;
  s1 ^= s0;
  r->lo = rn_rotl64(s0, 49) ^ s1 ^ (s1 << 21);
  r->hi = rn_rotl64(s1, 28);
  return result;
}

int xoro_next_int(Xoro *r) { return (int)(uint32_t)xoro_next_long(r); }

/* XoroshiroRandomSource.nextInt(bound): Lemire's multiply-and-shift with the
   "unbiased buckets" rejection loop. */
int xoro_next_bound(Xoro *r, int bound) {
  uint64_t random_bits = (uint64_t)(uint32_t)xoro_next_int(r);
  uint64_t multiplied = random_bits * (uint64_t)(uint32_t)bound;
  uint64_t fractional = multiplied & 0xFFFFFFFFULL;
  if (fractional < (uint64_t)(uint32_t)bound) {
    uint32_t start = (uint32_t)(~((uint32_t)bound) + 1u) % (uint32_t)bound;
    for (; fractional < (uint64_t)start; fractional = multiplied & 0xFFFFFFFFULL) {
      random_bits = (uint64_t)(uint32_t)xoro_next_int(r);
      multiplied = random_bits * (uint64_t)(uint32_t)bound;
    }
  }
  return (int)(multiplied >> 32);
}

float xoro_next_float(Xoro *r) { return (float)(xoro_next_long(r) >> 40) * 5.9604645e-8f; }

double xoro_next_double(Xoro *r) {
  /* Bytecode: l2d then dmul by the double 1.1102230246251565E-16 (0x1.0p-53).
     (The decompiled source prints that constant as "1.110223E-16F"; the class
     file really holds a double, verified with javap -c.) */
  return (double)(xoro_next_long(r) >> 11) * 1.1102230246251565e-16;
}

/* XoroshiroRandomSource(long): RandomSupport.upgradeSeedTo128bit */
void xoro_from_seed(Xoro *r, uint64_t seed) {
  uint64_t lo = seed ^ RN_SILVER;
  uint64_t hi = lo + RN_GOLDEN;
  xoro_set_seed(r, rn_mix_stafford13(lo), rn_mix_stafford13(hi));
}

/* XoroshiroRandomSource.forkPositional */
XoroFactory xoro_fork_positional(Xoro *r) {
  XoroFactory f;
  f.lo = xoro_next_long(r);
  f.hi = xoro_next_long(r);
  return f;
}

/* Mth.getSeed; the x * 3129871 product wraps in 32-bit like Java's int math. */
long mth_get_seed(int x, int y, int z) {
  uint64_t seed = (uint64_t)(int64_t)(int32_t)((uint32_t)x * 3129871u) ^ (uint64_t)z * 116129781ull ^
                  (uint64_t)(int64_t)y;
  seed = seed * seed * 42317861ull + seed * 11ull;
  return (long)((int64_t)seed >> 16);
}

/* XoroshiroPositionalRandomFactory.at(x, y, z) */
void xoro_at(XoroFactory f, int x, int y, int z, Xoro *out) {
  uint64_t positional_seed = (uint64_t)mth_get_seed(x, y, z);
  xoro_set_seed(out, positional_seed ^ f.lo, f.hi);
}

/* RandomSupport.seedFromHashOf: MD5 of the UTF-8 name, two big-endian halves. */
void xoro_seed_from_hash(const char *name, uint64_t *lo, uint64_t *hi) {
  uint8_t d[16];
  rn_md5((const uint8_t *)name, strlen(name), d);
  uint64_t l = 0, h = 0;
  for (int i = 0; i < 8; i++) l = (l << 8) | d[i];
  for (int i = 8; i < 16; i++) h = (h << 8) | d[i];
  *lo = l;
  *hi = h;
}

/* XoroshiroPositionalRandomFactory.fromHashOf */
void xoro_from_hash(XoroFactory f, const char *name, Xoro *out) {
  uint64_t lo, hi;
  xoro_seed_from_hash(name, &lo, &hi);
  xoro_set_seed(out, lo ^ f.lo, hi ^ f.hi);
}

/* ================================================== LegacyRandomSource (LCG) */

#define RN_LCG_MULTIPLIER 25214903917ULL
#define RN_LCG_INCREMENT 11ULL
#define RN_LCG_MASK 281474976710655ULL /* 2^48 - 1 */

void legacy_set_seed(LegacyRng *r, uint64_t seed) {
  if (r->kind == LEGACY_RNG_XORO) {
    /* XoroshiroRandomSource.setSeed(long): upgradeSeedTo128bit, fresh generator. */
    xoro_from_seed(&r->xoro, seed);
    r->have_next_next_gaussian = 0;
    return;
  }
  r->seed = (seed ^ RN_LCG_MULTIPLIER) & RN_LCG_MASK;
  r->have_next_next_gaussian = 0;
}

/* MarsagliaPolarGaussian.nextGaussian (java.util.Random's algorithm). */
double legacy_next_gaussian(LegacyRng *r) {
  if (r->have_next_next_gaussian) {
    r->have_next_next_gaussian = 0;
    return r->next_next_gaussian;
  }
  double x, y, radius_squared;
  do {
    x = 2.0 * legacy_next_double(r) - 1.0;
    y = 2.0 * legacy_next_double(r) - 1.0;
    radius_squared = x * x + y * y;
  } while (radius_squared >= 1.0 || radius_squared == 0.0);
  double multiplier = sqrt(-2.0 * log(radius_squared) / radius_squared);
  r->next_next_gaussian = y * multiplier;
  r->have_next_next_gaussian = 1;
  return x * multiplier;
}

int legacy_next(LegacyRng *r, int bits) {
  if (r->kind == LEGACY_RNG_XORO) {
    /* WorldgenRandom.next(bits) over a non-legacy source:
     * (int)(randomSource.nextLong() >>> (64 - bits)). */
    return (int)(xoro_next_long(&r->xoro) >> (64 - bits));
  }
  uint64_t new_seed = (r->seed * RN_LCG_MULTIPLIER + RN_LCG_INCREMENT) & RN_LCG_MASK;
  r->seed = new_seed;
  return (int)(uint32_t)(new_seed >> (48 - bits));
}

int legacy_next_int(LegacyRng *r) { return legacy_next(r, 32); }

/* BitRandomSource.nextInt(bound) */
int legacy_next_bound(LegacyRng *r, int bound) {
  if ((bound & (bound - 1)) == 0) {
    return (int)(((int64_t)bound * (int64_t)legacy_next(r, 31)) >> 31);
  }
  int sample, modulo;
  do {
    sample = legacy_next(r, 31);
    modulo = sample % bound;
  } while ((int32_t)((uint32_t)sample - (uint32_t)modulo + (uint32_t)(bound - 1)) < 0);
  return modulo;
}

float legacy_next_float(LegacyRng *r) { return (float)legacy_next(r, 24) * 5.9604645e-8f; }

double legacy_next_double(LegacyRng *r) {
  int upper = legacy_next(r, 26);
  int lower = legacy_next(r, 27);
  int64_t combined = ((int64_t)upper << 27) + (int64_t)lower;
  /* Bytecode: l2d then dmul by the double 1.1102230246251565E-16 (0x1.0p-53). */
  return (double)combined * 1.1102230246251565e-16;
}

int64_t legacy_next_long(LegacyRng *r) {
  int upper = legacy_next(r, 32);
  int lower = legacy_next(r, 32);
  /* ((long)upper << 32) + lower, kept in unsigned arithmetic so the wrap is defined. */
  uint64_t shifted = (uint64_t)(uint32_t)upper << 32;
  return (int64_t)(shifted + (uint64_t)(int64_t)lower);
}

/* RandomSource.consumeCount: n times nextInt() (i.e. next(32)). */
void legacy_consume(LegacyRng *r, int n) {
  for (int i = 0; i < n; i++) legacy_next(r, 32);
}

/* WorldgenRandom.setLargeFeatureSeed */
void legacy_set_large_feature_seed(LegacyRng *r, uint64_t world_seed, int chunk_x, int chunk_z) {
  legacy_set_seed(r, world_seed);
  uint64_t x_scale = (uint64_t)legacy_next_long(r);
  uint64_t z_scale = (uint64_t)legacy_next_long(r);
  uint64_t result = ((uint64_t)(int64_t)chunk_x * x_scale) ^ ((uint64_t)(int64_t)chunk_z * z_scale) ^ world_seed;
  legacy_set_seed(r, result);
}

/* WorldgenRandom.setDecorationSeed */
void legacy_set_decoration_seed(LegacyRng *r, uint64_t world_seed, int min_block_x, int min_block_z) {
  legacy_set_seed(r, world_seed);
  uint64_t x_scale = (uint64_t)legacy_next_long(r) | 1ull;
  uint64_t z_scale = (uint64_t)legacy_next_long(r) | 1ull;
  uint64_t result = (((uint64_t)(int64_t)min_block_x * x_scale) + ((uint64_t)(int64_t)min_block_z * z_scale)) ^
                    world_seed;
  legacy_set_seed(r, result);
}

/* java.util.Random.nextLong(bound) */
int64_t legacy_next_long_bound(LegacyRng *r, int64_t bound) {
  int64_t x = legacy_next_long(r);
  int64_t m = bound - 1;
  if ((bound & m) == 0) {
    return x & m;
  }
  int64_t u = (int64_t)((uint64_t)x >> 1); /* ensure nonnegative */
  while ((int64_t)((uint64_t)u + (uint64_t)m - (uint64_t)(x = u % bound)) < 0) {
    u = (int64_t)((uint64_t)legacy_next_long(r) >> 1);
  }
  return x;
}

/* ============================================================ ImprovedNoise */

typedef struct {
  double xo, yo, zo;
  uint8_t p[256];
} RnImproved;

/* SimplexNoise.GRADIENT */
static const int rn_gradient[16][3] = {{1, 1, 0},  {-1, 1, 0},  {1, -1, 0},  {-1, -1, 0},
                                       {1, 0, 1},  {-1, 0, 1},  {1, 0, -1},  {-1, 0, -1},
                                       {0, 1, 1},  {0, -1, 1},  {0, 1, -1},  {0, -1, -1},
                                       {1, 1, 0},  {0, -1, 1},  {-1, 1, 0},  {0, -1, -1}};

/* SimplexNoise.dot(GRADIENT[hash & 15], x, y, z) — evaluation order preserved. */
static double rn_grad_dot(int hash, double x, double y, double z) {
  const int *g = rn_gradient[hash & 15];
  return g[0] * x + g[1] * y + g[2] * z;
}

static int rn_p(const RnImproved *n, int x) { return n->p[x & 0xFF] & 0xFF; }

static void rn_improved_init(RnImproved *n, Xoro *random) {
  n->xo = xoro_next_double(random) * 256.0;
  n->yo = xoro_next_double(random) * 256.0;
  n->zo = xoro_next_double(random) * 256.0;
  for (int i = 0; i < 256; i++) n->p[i] = (uint8_t)i;
  for (int i = 0; i < 256; i++) {
    int offset = xoro_next_bound(random, 256 - i);
    uint8_t tmp = n->p[i];
    n->p[i] = n->p[i + offset];
    n->p[i + offset] = tmp;
  }
}

static double rn_sample_and_lerp(const RnImproved *n, int x, int y, int z, double xr, double yr, double zr,
                                 double yr_original) {
  int x0 = rn_p(n, x);
  int x1 = rn_p(n, x + 1);
  int xy00 = rn_p(n, x0 + y);
  int xy01 = rn_p(n, x0 + y + 1);
  int xy10 = rn_p(n, x1 + y);
  int xy11 = rn_p(n, x1 + y + 1);
  double d000 = rn_grad_dot(rn_p(n, xy00 + z), xr, yr, zr);
  double d100 = rn_grad_dot(rn_p(n, xy10 + z), xr - 1.0, yr, zr);
  double d010 = rn_grad_dot(rn_p(n, xy01 + z), xr, yr - 1.0, zr);
  double d110 = rn_grad_dot(rn_p(n, xy11 + z), xr - 1.0, yr - 1.0, zr);
  double d001 = rn_grad_dot(rn_p(n, xy00 + z + 1), xr, yr, zr - 1.0);
  double d101 = rn_grad_dot(rn_p(n, xy10 + z + 1), xr - 1.0, yr, zr - 1.0);
  double d011 = rn_grad_dot(rn_p(n, xy01 + z + 1), xr, yr - 1.0, zr - 1.0);
  double d111 = rn_grad_dot(rn_p(n, xy11 + z + 1), xr - 1.0, yr - 1.0, zr - 1.0);
  double x_alpha = jsmoothstep(xr);
  double y_alpha = jsmoothstep(yr_original);
  double z_alpha = jsmoothstep(zr);
  return jlerp3(x_alpha, y_alpha, z_alpha, d000, d100, d010, d110, d001, d101, d011, d111);
}

/* ImprovedNoise.noise(x, y, z, yScale, yFudge) */
static double rn_improved_noise(const RnImproved *n, double _x, double _y, double _z, double yScale,
                                double yFudge) {
  double x = _x + n->xo;
  double y = _y + n->yo;
  double z = _z + n->zo;
  int xf = jfloor(x);
  int yf = jfloor(y);
  int zf = jfloor(z);
  double xr = x - xf;
  double yr = y - yf;
  double zr = z - zf;
  double yr_fudge;
  if (yScale != 0.0) {
    double fudge_limit = (yFudge >= 0.0 && yFudge < yr) ? yFudge : yr;
    yr_fudge = (double)jfloor(fudge_limit / yScale + 1.0e-7f) * yScale;
  } else {
    yr_fudge = 0.0;
  }
  return rn_sample_and_lerp(n, xf, yf, zf, xr, yr - yr_fudge, zr, yr);
}

/* ============================================================== PerlinNoise */

typedef struct {
  int nlevels;
  int first_octave;
  RnImproved **levels; /* NULL where the octave has zero amplitude (or was skipped) */
  double *amps;
  double lowest_freq_input_factor;
  double lowest_freq_value_factor;
  double max_value;
} RnPerlin;

/* PerlinNoise.wrap */
static double rn_wrap(double x) {
  return x - (double)jlfloor(x / 3.3554432e7 + 0.5) * 3.3554432e7;
}

static void rn_perlin_alloc(RnPerlin *p, int nlevels, int first_octave, const double *amps) {
  p->nlevels = nlevels;
  p->first_octave = first_octave;
  p->levels = (RnImproved **)calloc((size_t)nlevels, sizeof(RnImproved *));
  p->amps = (double *)calloc((size_t)nlevels, sizeof(double));
  for (int i = 0; i < nlevels; i++) p->amps[i] = amps[i];
}

/* PerlinNoise.edgeValue */
static double rn_perlin_edge_value(const RnPerlin *p, double noise_value) {
  double value = 0.0;
  double value_factor = p->lowest_freq_value_factor;
  for (int i = 0; i < p->nlevels; i++) {
    if (p->levels[i] != NULL) value += p->amps[i] * noise_value * value_factor;
    value_factor /= 2.0;
  }
  return value;
}

static void rn_perlin_finish(RnPerlin *p) {
  int zero_octave_index = -p->first_octave;
  int octaves = p->nlevels;
  p->lowest_freq_input_factor = pow(2.0, (double)(-zero_octave_index));
  p->lowest_freq_value_factor = pow(2.0, (double)(octaves - 1)) / (pow(2.0, (double)octaves) - 1.0);
  p->max_value = rn_perlin_edge_value(p, 2.0);
}

/* PerlinNoise.create(random, firstOctave, amplitudes): forkPositional +
   fromHashOf("octave_" + octave) per non-zero octave. */
static void rn_perlin_init_new(RnPerlin *p, Xoro *random, int first_octave, const double *amps, int nlevels) {
  rn_perlin_alloc(p, nlevels, first_octave, amps);
  XoroFactory f = xoro_fork_positional(random);
  char key[64];
  for (int i = 0; i < nlevels; i++) {
    if (p->amps[i] != 0.0) {
      Xoro sub;
      snprintf(key, sizeof key, "octave_%d", first_octave + i);
      xoro_from_hash(f, key, &sub);
      p->levels[i] = (RnImproved *)calloc(1, sizeof(RnImproved));
      rn_improved_init(p->levels[i], &sub);
    }
  }
  rn_perlin_finish(p);
}

/* PerlinNoise.skipOctave -> RandomSource.consumeCount(262) on a Xoroshiro source. */
static void rn_skip_octave(Xoro *random) {
  for (int i = 0; i < 262; i++) xoro_next_long(random);
}

/* PerlinNoise(random, pair, false): the legacy constructor used by BlendedNoise. */
static void rn_perlin_init_legacy(RnPerlin *p, Xoro *random, int first_octave, const double *amps,
                                  int nlevels) {
  rn_perlin_alloc(p, nlevels, first_octave, amps);
  int zero_octave_index = -first_octave;
  RnImproved *zero_octave = (RnImproved *)calloc(1, sizeof(RnImproved));
  rn_improved_init(zero_octave, random);
  if (zero_octave_index >= 0 && zero_octave_index < nlevels) {
    if (p->amps[zero_octave_index] != 0.0) {
      p->levels[zero_octave_index] = zero_octave;
    } /* else vanilla discards the freshly built octave */
  }
  for (int i = zero_octave_index - 1; i >= 0; i--) {
    if (i < nlevels) {
      if (p->amps[i] != 0.0) {
        p->levels[i] = (RnImproved *)calloc(1, sizeof(RnImproved));
        rn_improved_init(p->levels[i], random);
      } else {
        rn_skip_octave(random);
      }
    } else {
      rn_skip_octave(random);
    }
  }
  rn_perlin_finish(p);
}

/* createLegacyForBlendedNoise(random, rangeClosed(lo, hi)): every octave in the
   closed range gets amplitude 1.0 and the first octave is lo. */
static void rn_perlin_init_blend_octaves(RnPerlin *p, Xoro *random, int lo, int hi) {
  int nlevels = -lo + hi + 1;
  double *amps = (double *)calloc((size_t)nlevels, sizeof(double));
  for (int i = 0; i < nlevels; i++) amps[i] = 1.0;
  rn_perlin_init_legacy(p, random, lo, amps, nlevels); /* makeAmplitudes: firstOctave = -lowFreqOctaves = lo */
  free(amps);
}

/* PerlinNoise.getOctaveNoise(i) */
static const RnImproved *rn_perlin_octave(const RnPerlin *p, int i) { return p->levels[p->nlevels - 1 - i]; }

/* PerlinNoise.getValue(x, y, z) (yScale = yFudge = 0) */
static double rn_perlin_get(const RnPerlin *p, double x, double y, double z) {
  double value = 0.0;
  double factor = p->lowest_freq_input_factor;
  double value_factor = p->lowest_freq_value_factor;
  for (int i = 0; i < p->nlevels; i++) {
    const RnImproved *n = p->levels[i];
    if (n != NULL) {
      /* PerlinNoise.getValue(x, y, z, 0.0, 0.0): the yScale/yFudge arguments are
         both zero here, which makes ImprovedNoise take its yScale == 0 branch. */
      double noise_val = rn_improved_noise(n, rn_wrap(x * factor), rn_wrap(y * factor), rn_wrap(z * factor),
                                           0.0, 0.0);
      value += p->amps[i] * noise_val * value_factor;
    }
    factor *= 2.0;
    value_factor /= 2.0;
  }
  return value;
}

/* PerlinNoise.maxBrokenValue(yScale) */
static double rn_perlin_max_broken_value(const RnPerlin *p, double yScale) {
  return rn_perlin_edge_value(p, yScale + 2.0);
}

/* ============================================================== NormalNoise */

struct NormalNoise {
  RnPerlin first;
  RnPerlin second;
  double value_factor;
  double max_value;
};

/* NormalNoise.expectedDeviation */
static double rn_expected_deviation(int octave_span) {
  return 0.1 * (1.0 + 1.0 / (octave_span + 1));
}

NormalNoise *normal_noise_create(Xoro *random, int first_octave, const double *amps, int namp) {
  NormalNoise *nn = (NormalNoise *)calloc(1, sizeof(NormalNoise));
  rn_perlin_init_new(&nn->first, random, first_octave, amps, namp);
  rn_perlin_init_new(&nn->second, random, first_octave, amps, namp);

  int min_octave = INT32_MAX;
  int max_octave = INT32_MIN;
  for (int i = 0; i < namp; i++) {
    if (amps[i] != 0.0) {
      if (i < min_octave) min_octave = i;
      if (i > max_octave) max_octave = i;
    }
  }
  nn->value_factor = 0.16666666666666666 / rn_expected_deviation(max_octave - min_octave);
  nn->max_value = (nn->first.max_value + nn->second.max_value) * nn->value_factor;
  return nn;
}

double normal_noise_get(const NormalNoise *nn, double x, double y, double z) {
  double x2 = x * 1.0181268882175227;
  double y2 = y * 1.0181268882175227;
  double z2 = z * 1.0181268882175227;
  return (rn_perlin_get(&nn->first, x, y, z) + rn_perlin_get(&nn->second, x2, y2, z2)) * nn->value_factor;
}

/* java.lang.String.hashCode (signed int) */
static int32_t java_string_hash(const char *s) {
  int32_t h = 0;
  for (; *s; s++) h = h * 31 + (int32_t)(unsigned char)*s;
  return h;
}

/* ------------------------------------------------- legacy NormalNoise ---- */

/* ImprovedNoise over a LegacyRandomSource (java.util.Random draw order). */
static void rn_improved_init_legacy(RnImproved *n, LegacyRng *r) {
  n->xo = legacy_next_double(r) * 256.0;
  n->yo = legacy_next_double(r) * 256.0;
  n->zo = legacy_next_double(r) * 256.0;
  for (int i = 0; i < 256; i++) n->p[i] = (uint8_t)i;
  for (int i = 0; i < 256; i++) {
    int offset = legacy_next_bound(r, 256 - i);
    uint8_t tmp = n->p[i];
    n->p[i] = n->p[i + offset];
    n->p[i + offset] = tmp;
  }
}

/* PerlinNoise(random, firstOctave, amplitudes) with useNewInitialization = true
 * over a LegacyRandomSource: the positional factory is seeded with ONE
 * nextLong(), and each non-zero octave is seeded with
 * ("octave_" + octave).hashCode() ^ factorySeed. */
static void rn_perlin_init_legacy_source(RnPerlin *p, LegacyRng *r, int first_octave,
                                         const double *amps, int nlevels) {
  rn_perlin_alloc(p, nlevels, first_octave, amps);
  uint64_t factory_seed = (uint64_t)legacy_next_long(r);
  for (int i = 0; i < nlevels; i++) {
    if (p->amps[i] == 0.0) continue;
    char name[32];
    snprintf(name, sizeof name, "octave_%d", first_octave + i);
    uint64_t octave_seed = (uint64_t)(int64_t)java_string_hash(name) ^ factory_seed;
    LegacyRng octave_rng;
    memset(&octave_rng, 0, sizeof octave_rng); /* LEGACY_RNG_LCG */
    legacy_set_seed(&octave_rng, octave_seed);
    p->levels[i] = (RnImproved *)calloc(1, sizeof(RnImproved));
    rn_improved_init_legacy(p->levels[i], &octave_rng);
  }
  rn_perlin_finish(p);
}

NormalNoise *normal_noise_create_legacy(LegacyRng *r, int first_octave, const double *amps, int namp) {
  NormalNoise *nn = (NormalNoise *)calloc(1, sizeof(NormalNoise));
  rn_perlin_init_legacy_source(&nn->first, r, first_octave, amps, namp);
  rn_perlin_init_legacy_source(&nn->second, r, first_octave, amps, namp);
  int min_octave = INT32_MAX, max_octave = INT32_MIN;
  for (int i = 0; i < namp; i++) {
    if (amps[i] != 0.0) {
      if (i < min_octave) min_octave = i;
      if (i > max_octave) max_octave = i;
    }
  }
  nn->value_factor = 0.16666666666666666 / rn_expected_deviation(max_octave - min_octave);
  nn->max_value = (nn->first.max_value + nn->second.max_value) * nn->value_factor;
  return nn;
}

double normal_noise_max(const NormalNoise *nn) { return nn->max_value; }

/* ============================================================= BlendedNoise */

struct BlendedNoise {
  RnPerlin min_limit_noise;
  RnPerlin max_limit_noise;
  RnPerlin main_noise;
  double xz_multiplier;
  double y_multiplier;
  double xz_factor;
  double y_factor;
  double smear_scale_multiplier;
  double max_value;
  double xz_scale;
  double y_scale;
};

BlendedNoise *blended_noise_create(Xoro *random, double xz_scale, double y_scale, double xz_factor,
                                   double y_factor, double smear_scale_multiplier) {
  BlendedNoise *bn = (BlendedNoise *)calloc(1, sizeof(BlendedNoise));
  rn_perlin_init_blend_octaves(&bn->min_limit_noise, random, -15, 0);
  rn_perlin_init_blend_octaves(&bn->max_limit_noise, random, -15, 0);
  rn_perlin_init_blend_octaves(&bn->main_noise, random, -7, 0);
  bn->xz_scale = xz_scale;
  bn->y_scale = y_scale;
  bn->xz_factor = xz_factor;
  bn->y_factor = y_factor;
  bn->smear_scale_multiplier = smear_scale_multiplier;
  bn->xz_multiplier = 684.412 * xz_scale;
  bn->y_multiplier = 684.412 * y_scale;
  bn->max_value = rn_perlin_max_broken_value(&bn->min_limit_noise, bn->y_multiplier);
  return bn;
}

/* BlendedNoise.compute */
double blended_noise_compute(const BlendedNoise *bn, int block_x, int block_y, int block_z) {
  double limit_x = block_x * bn->xz_multiplier;
  double limit_y = block_y * bn->y_multiplier;
  double limit_z = block_z * bn->xz_multiplier;
  double main_x = limit_x / bn->xz_factor;
  double main_y = limit_y / bn->y_factor;
  double main_z = limit_z / bn->xz_factor;
  double limit_smear = bn->y_multiplier * bn->smear_scale_multiplier;
  double main_smear = limit_smear / bn->y_factor;
  double blend_min = 0.0;
  double blend_max = 0.0;
  double main_noise_value = 0.0;
  double pow_ = 1.0;

  for (int i = 0; i < 8; i++) {
    const RnImproved *noise = rn_perlin_octave(&bn->main_noise, i);
    if (noise != NULL) {
      main_noise_value += rn_improved_noise(noise, rn_wrap(main_x * pow_), rn_wrap(main_y * pow_),
                                            rn_wrap(main_z * pow_), main_smear * pow_, main_y * pow_) / pow_;
    }
    pow_ /= 2.0;
  }

  double factor = (main_noise_value / 10.0 + 1.0) / 2.0;
  int is_max = factor >= 1.0;
  int is_min = factor <= 0.0;
  pow_ = 1.0;

  for (int i = 0; i < 16; i++) {
    double wx = rn_wrap(limit_x * pow_);
    double wy = rn_wrap(limit_y * pow_);
    double wz = rn_wrap(limit_z * pow_);
    double y_scale_pow = limit_smear * pow_;
    if (!is_max) {
      const RnImproved *noise = rn_perlin_octave(&bn->min_limit_noise, i);
      if (noise != NULL) {
        blend_min += rn_improved_noise(noise, wx, wy, wz, y_scale_pow, limit_y * pow_) / pow_;
      }
    }
    if (!is_min) {
      const RnImproved *noise = rn_perlin_octave(&bn->max_limit_noise, i);
      if (noise != NULL) {
        blend_max += rn_improved_noise(noise, wx, wy, wz, y_scale_pow, limit_y * pow_) / pow_;
      }
    }
    pow_ /= 2.0;
  }

  return jclamped_lerp(factor, blend_min / 512.0, blend_max / 512.0) / 128.0;
}

double blended_noise_max(const BlendedNoise *bn) { return bn->max_value; }

#endif /* CINDER_VANILLA_RAND_NOISE_C */
