// BiomeManager: the fuzzy biome lookup (SHA-256 zoom seed + fiddled 8 corners).
//
// Vanilla's surface rules and carvers' topMaterial ask BiomeManager.getBiome,
// which does not return the plain quart-cell biome: it picks one of the eight
// corners of the 4-block cell around (pos - 2) by the smallest "fiddled"
// distance, so biome borders wobble at the surface. This file mirrors that
// exactly (LinearCongruentialGenerator + getFiddle + getFiddledDistance).
//
// Included by vanilla_biomes.c; kept separate so the SHA-256 code stays out of
// the biome table's way.
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H

/* ------------------------------------------------------------- SHA-256 ---- */

// Guava's Hashing.sha256().hashLong(seed).asLong(): the seed is hashed in
// little-endian byte order and the first 8 digest bytes are read little-endian.
static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static inline uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256(const uint8_t *msg, size_t len, uint8_t out[32]) {
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  size_t total = ((len + 8) / 64 + 1) * 64;
  uint8_t *buf = (uint8_t *)calloc(total, 1);
  memcpy(buf, msg, len);
  buf[len] = 0x80;
  uint64_t bits = (uint64_t)len * 8;
  for (int i = 0; i < 8; i++) buf[total - 1 - i] = (uint8_t)(bits >> (8 * i));
  for (size_t off = 0; off < total; off += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
      w[i] = ((uint32_t)buf[off + 4 * i] << 24) | ((uint32_t)buf[off + 4 * i + 1] << 16) |
             ((uint32_t)buf[off + 4 * i + 2] << 8) | (uint32_t)buf[off + 4 * i + 3];
    }
    for (int i = 16; i < 64; i++) {
      uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
      uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t t1 = hh + s1 + ch + SHA256_K[i] + w[i];
      uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
      uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = s0 + maj;
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }
  free(buf);
  for (int i = 0; i < 8; i++) {
    out[4 * i] = (uint8_t)(h[i] >> 24);
    out[4 * i + 1] = (uint8_t)(h[i] >> 16);
    out[4 * i + 2] = (uint8_t)(h[i] >> 8);
    out[4 * i + 3] = (uint8_t)h[i];
  }
}

int64_t biome_obfuscate_seed(uint64_t seed) {
  uint8_t msg[8];
  for (int i = 0; i < 8; i++) msg[i] = (uint8_t)(seed >> (8 * i));
  uint8_t digest[32];
  sha256(msg, sizeof msg, digest);
  uint64_t value = 0;
  for (int i = 0; i < 8; i++) value |= (uint64_t)digest[i] << (8 * i);
  return (int64_t)value;
}

/* ------------------------------------------------------ BiomeManager ---- */

/* LinearCongruentialGenerator.next: Java longs wrap, so the arithmetic is
 * unsigned here (identical bits, defined behaviour). */
static inline int64_t lcg_next(int64_t rval, int64_t c) {
  uint64_t r = (uint64_t)rval;
  return (int64_t)(r * (r * 6364136223846793005ULL + 1442695040888963407ULL) + (uint64_t)c);
}

static double get_fiddle(int64_t rval) {
  int64_t mod = (rval >> 24) % 1024;
  if (mod < 0) mod += 1024;
  double uniform = (double)mod / 1024.0;
  return (uniform - 0.5) * 0.9;
}

static double get_fiddled_distance(int64_t seed, int x_random, int y_random, int z_random,
                                   double distance_x, double distance_y, double distance_z) {
  int64_t rval = seed;
  rval = lcg_next(rval, x_random);
  rval = lcg_next(rval, y_random);
  rval = lcg_next(rval, z_random);
  rval = lcg_next(rval, x_random);
  rval = lcg_next(rval, y_random);
  rval = lcg_next(rval, z_random);
  double fiddle_x = get_fiddle(rval);
  rval = lcg_next(rval, seed);
  double fiddle_y = get_fiddle(rval);
  rval = lcg_next(rval, seed);
  double fiddle_z = get_fiddle(rval);
  double a = distance_z + fiddle_z;
  double b = distance_y + fiddle_y;
  double c = distance_x + fiddle_x;
  return a * a + b * b + c * c;
}

int biome_at_block_fuzzy(Gen *g, ChunkEval *ce, int x, int y, int z) {
  int abs_x = x - 2, abs_y = y - 2, abs_z = z - 2;
  int parent_x = abs_x >> 2, parent_y = abs_y >> 2, parent_z = abs_z >> 2;
  double fract_x = (abs_x & 3) / 4.0;
  double fract_y = (abs_y & 3) / 4.0;
  double fract_z = (abs_z & 3) / 4.0;
  int min_i = 0;
  double min_distance = INFINITY;
  for (int i = 0; i < 8; i++) {
    int x_even = (i & 4) == 0;
    int y_even = (i & 2) == 0;
    int z_even = (i & 1) == 0;
    int corner_x = x_even ? parent_x : parent_x + 1;
    int corner_y = y_even ? parent_y : parent_y + 1;
    int corner_z = z_even ? parent_z : parent_z + 1;
    double distance_x = x_even ? fract_x : fract_x - 1.0;
    double distance_y = y_even ? fract_y : fract_y - 1.0;
    double distance_z = z_even ? fract_z : fract_z - 1.0;
    double next = get_fiddled_distance(g->biome_zoom_seed, corner_x, corner_y, corner_z, distance_x,
                                       distance_y, distance_z);
    if (min_distance > next) {
      min_i = i;
      min_distance = next;
    }
  }
  int biome_x = (min_i & 4) == 0 ? parent_x : parent_x + 1;
  int biome_y = (min_i & 2) == 0 ? parent_y : parent_y + 1;
  int biome_z = (min_i & 1) == 0 ? parent_z : parent_z + 1;
  return biome_at_quart(g, ce, biome_x, biome_y, biome_z);
}
