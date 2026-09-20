// C-side twin of bench/parity/noise_ref.java: prints the same values with the
// same keys so noise_check.sh can diff them.
//
// Build (see noise_check.sh):
//   clang -O1 -std=gnu11 -I src -o /tmp/noise_check \
//       bench/parity/noise_check.c src/vanilla_rand_noise.c -lm
//
// The implementation lives in src/vanilla_rand_noise.c, which is compiled as its
// own translation unit by the command above; the #include here is guarded by the
// same macro the implementation's include guard uses, so including it documents
// the dependency without duplicating the definitions at link time.

#define CINDER_VANILLA_RAND_NOISE_C 1
#include "vanilla_common.h"
#include "../src/vanilla_rand_noise.c"

static void pi(const char *key, int v) { printf("i %s %08x\n", key, (unsigned)v); }
static void pl(const char *key, int64_t v) { printf("l %s %016llx\n", key, (unsigned long long)v); }

static void pf(const char *key, float v) {
  unsigned u;
  memcpy(&u, &v, sizeof u);
  printf("f %s %08x\n", key, u);
}

static void pd(const char *key, double v) {
  uint64_t u;
  memcpy(&u, &v, sizeof u);
  printf("d %s %016llx\n", key, (unsigned long long)u);
}

static void pi_i(const char *prefix, int i, int v) {
  char key[64];
  snprintf(key, sizeof key, "%s_%d", prefix, i);
  pi(key, v);
}

static void pl_i(const char *prefix, int i, int64_t v) {
  char key[64];
  snprintf(key, sizeof key, "%s_%d", prefix, i);
  pl(key, v);
}

static void pf_i(const char *prefix, int i, float v) {
  char key[64];
  snprintf(key, sizeof key, "%s_%d", prefix, i);
  pf(key, v);
}

static void pd_i(const char *prefix, int i, double v) {
  char key[64];
  snprintf(key, sizeof key, "%s_%d", prefix, i);
  pd(key, v);
}

/* 20 integer block positions shared by the NormalNoise and BlendedNoise dumps. */
static int block_x(int i) { return i * 16 - 48; }
static int block_y(int i) { return 64 + i * 7; }
static int block_z(int i) { return i * 13 - 40; }

int main(void) {
  /* ------------------------------------------------------------------- (a) */
  printf("== xoroshiro seed 1234 ==\n");
  Xoro xs;
  xoro_from_seed(&xs, 1234ull);
  for (int i = 0; i < 8; i++) pl_i("xoro_long", i, (int64_t)xoro_next_long(&xs));
  for (int i = 0; i < 8; i++) pi_i("xoro_int", i, xoro_next_int(&xs));
  for (int i = 0; i < 8; i++) pi_i("xoro_bound7", i, xoro_next_bound(&xs, 7));
  for (int i = 0; i < 8; i++) pi_i("xoro_bound128", i, xoro_next_bound(&xs, 128));
  for (int i = 0; i < 8; i++) pf_i("xoro_float", i, xoro_next_float(&xs));
  for (int i = 0; i < 8; i++) pd_i("xoro_double", i, xoro_next_double(&xs));

  /* -------------------------------------------------- (a2) positional factory */
  printf("== xoroshiro positional ==\n");
  Xoro positional_src;
  xoro_from_seed(&positional_src, 777ull);
  XoroFactory fac = xoro_fork_positional(&positional_src);
  static const int positions[5][3] = {{0, 0, 0}, {1, 2, 3}, {-5, 100, -17}, {334, 64, -1024}, {1234567, -300, 987654}};
  for (int i = 0; i < 5; i++) {
    Xoro at;
    xoro_at(fac, positions[i][0], positions[i][1], positions[i][2], &at);
    pl_i("xoro_at_long", i, (int64_t)xoro_next_long(&at));
    pi_i("xoro_at_bound7", i, xoro_next_bound(&at, 7));
    pd_i("xoro_at_double", i, xoro_next_double(&at));
  }
  Xoro hashed;
  xoro_from_hash(fac, "minecraft:overworld", &hashed);
  for (int i = 0; i < 4; i++) pl_i("xoro_hash_long", i, (int64_t)xoro_next_long(&hashed));
  Xoro hashed_oct;
  xoro_from_hash(fac, "octave_-8", &hashed_oct);
  for (int i = 0; i < 4; i++) pi_i("xoro_hash_oct_bound1000", i, xoro_next_bound(&hashed_oct, 1000));
  uint64_t lo, hi;
  xoro_seed_from_hash("octave_-8", &lo, &hi);
  pl("seed_hash_lo_octave_m8", (int64_t)lo);
  pl("seed_hash_hi_octave_m8", (int64_t)hi);
  xoro_seed_from_hash("octave_3", &lo, &hi);
  pl("seed_hash_lo_octave_3", (int64_t)lo);
  pl("seed_hash_hi_octave_3", (int64_t)hi);
  xoro_seed_from_hash("", &lo, &hi);
  pl("seed_hash_lo_empty", (int64_t)lo);
  pl("seed_hash_hi_empty", (int64_t)hi);

  /* ------------------------------------------------------------------- (b) */
  static const double amps0[] = {1.0};
  static const double amps1[] = {1.0, 1.0, 1.0, 1.0};
  static const double amps2[] = {1.5, 0.0, 1.0, 0.0, 0.0, 1.0};
  const double *amp_sets[3] = {amps0, amps1, amps2};
  const int amp_counts[3] = {1, 4, 6};
  printf("== normal noise (seed 1234, firstOctave -8) ==\n");
  for (int s = 0; s < 3; s++) {
    Xoro r;
    xoro_from_seed(&r, 1234ull);
    NormalNoise *nn = normal_noise_create(&r, -8, amp_sets[s], amp_counts[s]);
    char key[64];
    snprintf(key, sizeof key, "nn%d_max", s);
    pd(key, normal_noise_max(nn));
    for (int i = 0; i < 20; i++) {
      snprintf(key, sizeof key, "nn%d_v_%d", s, i);
      pd(key, normal_noise_get(nn, block_x(i) + 0.25, block_y(i) + 0.5, block_z(i) + 0.75));
    }
  }

  /* Zero-amplitude octaves: the overworld noise files carry these, and the skip
     path (levels created only for non-zero amplitudes, plus the
     2^(n-1)/(2^n - 1) value factor) is easy to get wrong. */
  static const double zamps0[] = {1.0, 1.0, 0.0, 1.0};
  static const double zamps1[] = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0};
  static const double zamps2[] = {0.0, 1.0};
  static const double zamps3[] = {1.0, 0.0};
  static const double zamps4[] = {0.0, 1.0, 0.0, 0.0, 1.0, 1.0};
  const double *zamp_sets[5] = {zamps0, zamps1, zamps2, zamps3, zamps4};
  const int zamp_counts[5] = {4, 8, 2, 2, 6};
  printf("== normal noise zero amplitudes ==\n");
  for (int s = 0; s < 5; s++) {
    Xoro r;
    xoro_from_seed(&r, 1234ull);
    NormalNoise *nn = normal_noise_create(&r, -8, zamp_sets[s], zamp_counts[s]);
    char key[64];
    snprintf(key, sizeof key, "nz%d_max", s);
    pd(key, normal_noise_max(nn));
    for (int i = 0; i < 20; i++) {
      snprintf(key, sizeof key, "nz%d_v_%d", s, i);
      pd(key, normal_noise_get(nn, block_x(i) + 0.25, block_y(i) + 0.5, block_z(i) + 0.75));
    }
  }

  /* Same amplitudes, different firstOctave: the offset of the zero octave moves. */
  static const double shifted_amps[] = {1.0, 0.0, 1.0, 0.0, 0.0, 1.0};
  printf("== normal noise shifted first octave ==\n");
  for (int s = 0; s < 3; s++) {
    Xoro r;
    xoro_from_seed(&r, 1234ull);
    NormalNoise *nn = normal_noise_create(&r, -8 + s * 2, shifted_amps, 6);
    char key[64];
    snprintf(key, sizeof key, "ns%d_max", s);
    pd(key, normal_noise_max(nn));
    for (int i = 0; i < 20; i++) {
      snprintf(key, sizeof key, "ns%d_v_%d", s, i);
      pd(key, normal_noise_get(nn, block_x(i) + 0.25, block_y(i) + 0.5, block_z(i) + 0.75));
    }
  }

  /* ------------------------------------------------------------------- (c) */
  printf("== blended noise (seed 99) ==\n");
  Xoro blend_src;
  xoro_from_seed(&blend_src, 99ull);
  BlendedNoise *bn = blended_noise_create(&blend_src, 0.25, 0.125, 80.0, 160.0, 8.0);
  pd("bn_max", blended_noise_max(bn));
  for (int i = 0; i < 20; i++) {
    pd_i("bn_v", i, blended_noise_compute(bn, block_x(i), block_y(i), block_z(i)));
  }

  /* ------------------------------------------------------------------- (d) */
  printf("== legacy source (seed 42) ==\n");
  LegacyRng lr;
  legacy_set_seed(&lr, 42ull);
  for (int i = 0; i < 8; i++) pi_i("legacy_int", i, legacy_next_int(&lr));
  for (int i = 0; i < 8; i++) pi_i("legacy_bound1000", i, legacy_next_bound(&lr, 1000));
  for (int i = 0; i < 8; i++) pi_i("legacy_bound7", i, legacy_next_bound(&lr, 7));
  for (int i = 0; i < 8; i++) pi_i("legacy_bound1024", i, legacy_next_bound(&lr, 1024));
  for (int i = 0; i < 8; i++) pf_i("legacy_float", i, legacy_next_float(&lr));
  for (int i = 0; i < 8; i++) pd_i("legacy_double", i, legacy_next_double(&lr));
  for (int i = 0; i < 8; i++) pl_i("legacy_long", i, legacy_next_long(&lr));

  printf("== legacy consumeCount ==\n");
  LegacyRng cr;
  legacy_set_seed(&cr, 42ull);
  legacy_consume(&cr, 262);
  for (int i = 0; i < 4; i++) pi_i("consume_int", i, legacy_next_int(&cr));
  for (int i = 0; i < 4; i++) pl_i("consume_long", i, legacy_next_long(&cr));
  for (int i = 0; i < 4; i++) pd_i("consume_double", i, legacy_next_double(&cr));

  /* Wide sweep: negative coordinates, and coordinates large enough to exercise
     PerlinNoise.wrap (|x / 3.3554432E7| near/over 0.5). */
  printf("== noise coordinate sweep ==\n");
  static const uint64_t sweep_seeds[4] = {0ull, 1ull, (uint64_t)-1LL, 123456789012345ull};
  static const int sweep_x[10] = {0, -1, 1, -12345, 987654, 33554432, 33554433, -33554432, 67108864, 1000000000};
  static const int sweep_y[7] = {-64, 0, 63, 64, 320, -33554432, 33554432};
  static const int sweep_z[6] = {7, -7, 12345678, -98765432, 33554431, -1000000000};
  for (int si = 0; si < 4; si++) {
    Xoro r;
    xoro_from_seed(&r, sweep_seeds[si]);
    NormalNoise *nn = normal_noise_create(&r, -8, shifted_amps, 6);
    Xoro br;
    xoro_from_seed(&br, sweep_seeds[si]);
    BlendedNoise *sbn = blended_noise_create(&br, 1.0, 1.0, 80.0, 160.0, 8.0);
    char key[80];
    for (int i = 0; i < 10; i++) {
      for (int j = 0; j < 7; j++) {
        for (int k = 0; k < 6; k++) {
          snprintf(key, sizeof key, "sw%d_nn_%d_%d_%d", si, i, j, k);
          pd(key, normal_noise_get(nn, sweep_x[i] + 0.5, sweep_y[j] - 0.25, sweep_z[k] + 0.125));
        }
      }
    }
    for (int i = 0; i < 10; i++) {
      for (int j = 0; j < 7; j++) {
        snprintf(key, sizeof key, "sw%d_bn_%d_%d", si, i, j);
        pd(key, blended_noise_compute(sbn, sweep_x[i], sweep_y[j], sweep_z[i % 6]));
      }
    }
  }

  /* positional factory across many coordinates, including negative */
  printf("== positional sweep ==\n");
  Xoro psrc;
  xoro_from_seed(&psrc, (uint64_t)-424242LL);
  XoroFactory sfac = xoro_fork_positional(&psrc);
  for (int x = -2; x <= 2; x++) {
    for (int y = -2; y <= 2; y++) {
      for (int z = -2; z <= 2; z++) {
        Xoro at;
        char key[64];
        xoro_at(sfac, x * 1000003, y * 7919, z * 104729, &at);
        snprintf(key, sizeof key, "ps_%d_%d_%d", x, y, z);
        pl(key, (int64_t)xoro_next_long(&at));
        snprintf(key, sizeof key, "psb_%d_%d_%d", x, y, z);
        pi(key, xoro_next_bound(&at, 13));
      }
    }
  }

  printf("== worldgen large feature / decoration seeds ==\n");
  LegacyRng lfs;
  legacy_set_seed(&lfs, 0ull); /* WorldgenRandom super(0L) */
  legacy_set_large_feature_seed(&lfs, 1ull, -3, 7);
  for (int i = 0; i < 8; i++) pl_i("lfs_long", i, legacy_next_long(&lfs));
  for (int i = 0; i < 4; i++) pi_i("lfs_bound1000", i, legacy_next_bound(&lfs, 1000));

  LegacyRng dec;
  legacy_set_seed(&dec, 0ull);
  legacy_set_decoration_seed(&dec, 1ull, -3, 7);
  for (int i = 0; i < 8; i++) pl_i("dec_long", i, legacy_next_long(&dec));
  for (int i = 0; i < 4; i++) pi_i("dec_bound1000", i, legacy_next_bound(&dec, 1000));

  printf("== java.util.Random.nextLong(bound) ==\n");
  LegacyRng jl;
  legacy_set_seed(&jl, 42ull);
  for (int i = 0; i < 8; i++) pl_i("jl_bound1000", i, legacy_next_long_bound(&jl, 1000));
  for (int i = 0; i < 8; i++) pl_i("jl_bound1024", i, legacy_next_long_bound(&jl, 1024));
  for (int i = 0; i < 8; i++) pl_i("jl_bound7", i, legacy_next_long_bound(&jl, 7));
  for (int i = 0; i < 4; i++) pl_i("jl_bound1", i, legacy_next_long_bound(&jl, 1));
  for (int i = 0; i < 4; i++) pl_i("jl_bound3", i, legacy_next_long_bound(&jl, 3));
  for (int i = 0; i < 4; i++) pl_i("jl_boundmaxint", i, legacy_next_long_bound(&jl, INT32_MAX));
  for (int i = 0; i < 4; i++) pl_i("jl_bound2p62", i, legacy_next_long_bound(&jl, (int64_t)1 << 62));
  for (int i = 0; i < 4; i++) pl_i("jl_boundmaxlong", i, legacy_next_long_bound(&jl, INT64_MAX));
  /* Bounds whose rejection loop triggers with probability 1/4..1/2: for
     bound = 2^62 + k the loop rejects whenever u / bound == k_max, i.e. with
     probability (2^63 - k_max*bound) / 2^63. Plain bounds like 1000 reject with
     probability ~1e-16, so they never exercise the retry at all. */
  for (int i = 0; i < 12; i++) pl_i("jl_boundreject", i, legacy_next_long_bound(&jl, 4611686018427387905LL));
  for (int i = 0; i < 12; i++) pl_i("jl_boundreject3", i, legacy_next_long_bound(&jl, 2305843009213693953LL));
  for (int i = 0; i < 12; i++) pl_i("jl_boundreject2", i, legacy_next_long_bound(&jl, 3074457345618258603LL));

  printf("== xoroshiro high-rejection bounds ==\n");
  Xoro xr;
  xoro_from_seed(&xr, 2024ull);
  /* Lemire rejects when (r & 0xFFFFFFFF) < (2^32 mod bound); for
     1431655766 that is 1431655764/2^32 ~ 1/3 of draws. */
  for (int i = 0; i < 12; i++) pi_i("xr_reject", i, xoro_next_bound(&xr, 1431655766));
  for (int i = 0; i < 12; i++) pi_i("xr_reject2", i, xoro_next_bound(&xr, 1073741825));
  for (int i = 0; i < 12; i++) pi_i("xr_reject3", i, xoro_next_bound(&xr, 1000000007));
  for (int i = 0; i < 12; i++) pi_i("xr_pow2", i, xoro_next_bound(&xr, 1073741824));
  for (int i = 0; i < 12; i++) pi_i("xr_bound1", i, xoro_next_bound(&xr, 1));

  return 0;
}
