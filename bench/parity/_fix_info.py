import pathlib

p = pathlib.Path('src/vanilla_surface.c')
s = p.read_text()

# Move the public biome-info accessor to AFTER sr_init_perlins' definition.
block = '''/* Biome.BIOME_INFO_NOISE.getValue(x, z, false) - the noise the flower-count and
 * surface-relative placements use (PerlinSimplexNoise, fixed legacy seed 2345,
 * octave 0, useNoiseStart = false). */
double biome_info_noise_value(Gen *g, double x, double z) {
  (void)g;
  if (!sr_perlin_ready) sr_init_perlins();
  return sr_perlin_value(&sr_pt_biome_info, x, z, 0);
}'''
assert block in s
s = s.replace(block + '\n', '', 1)

marker = '''static void sr_init_perlins(void) {'''
assert marker in s
# find the end of sr_init_perlins and insert after it
idx = s.index(marker)
end = s.index('\n}\n', idx) + 3
s = s[:end] + '\n' + block + '\n' + s[end:]
p.write_text(s)
print('accessor moved after sr_init_perlins')
