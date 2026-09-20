import pathlib

p = pathlib.Path('src/vanilla_rand_noise.c')
s = p.read_text()

# add a Java String.hashCode helper and use it in the legacy noise path
if 'static int32_t java_string_hash' not in s:
    s = s.replace('''/* ------------------------------------------------- legacy NormalNoise ---- */''',
'''/* java.lang.String.hashCode (signed int) */
static int32_t java_string_hash(const char *s) {
  int32_t h = 0;
  for (; *s; s++) h = h * 31 + (int32_t)(unsigned char)*s;
  return h;
}

/* ------------------------------------------------- legacy NormalNoise ---- */''')

# the legacy perlin init must run over the LegacyRng, whose draw sequence is
# nextDouble()*256 then the 256 nextInt(256-i) shuffle - same as perlin_init
# expects, but perlin_init takes a Xoro; add a legacy variant.
if 'static void perlin_init_legacy' not in s:
    s = s.replace('''NormalNoise *normal_noise_create_legacy(LegacyRng *r, int first_octave, const double *amps, int namp) {''',
'''static void perlin_init_legacy(Perlin *n, LegacyRng *r) {
  n->ox = legacy_next_double(r) * 256.0;
  n->oy = legacy_next_double(r) * 256.0;
  n->oz = legacy_next_double(r) * 256.0;
  for (int i = 0; i < 256; i++) n->p[i] = (uint8_t)i;
  for (int i = 0; i < 256; i++) {
    int j = i + legacy_next_bound(r, 256 - i);
    uint8_t t = n->p[i];
    n->p[i] = n->p[j];
    n->p[j] = t;
  }
}

NormalNoise *normal_noise_create_legacy(LegacyRng *r, int first_octave, const double *amps, int namp) {''')
    s = s.replace('      perlin_init(&nn->oct[nn->n], &octave_rng);',
                  '      perlin_init_legacy(&nn->oct[nn->n], &octave_rng);')
p.write_text(s)
print('legacy perlin init wired')
