import pathlib

p = pathlib.Path('src/vanilla_rand_noise.c')
s = p.read_text()

# Replace the half-written legacy noise block with one that reuses the existing
# RnPerlin/RnImproved machinery through a LegacyRng-backed ImprovedNoise init.
start = s.index('/* java.lang.String.hashCode (signed int) */')
end = s.index('double normal_noise_max(const NormalNoise *nn) {')
new = r'''/* java.lang.String.hashCode (signed int) */
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

'''
s = s[:start] + new + s[end:]
p.write_text(s)
print('legacy noise rewritten')
