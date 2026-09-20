
/* Vanilla 26.2 surface system: SurfaceSystem + SurfaceRules, ported 1:1.
 *
 * Sources of truth (decompiled 26.2):
 *   world/level/levelgen/SurfaceSystem.java
 *   world/level/levelgen/SurfaceRules.java
 *   world/level/biome/Biome.java (getTemperature / TemperatureModifier)
 *   world/level/levelgen/synth/{SimplexNoise,PerlinSimplexNoise}.java
 *
 * Included by src/vanilla_gen.c; every helper here is `static`.
 *
 * Contract notes (dependencies on the shared header, all used exactly as
 * declared in vanilla_common.h):
 *   - hm_world_surface / hm_ocean_floor are first-available maps; hm_top is
 *     vanilla's ChunkAccess.getHeight(...).  Both are written back through
 *     hm_update for every surface write, because ProtoChunk.setBlockState
 *     updates Heightmap.Types.WORLD_SURFACE_WG (NOT_AIR) and OCEAN_FLOOR_WG
 *     (MATERIAL_MOTION_BLOCKING) while the chunk is at status NOISE/SURFACE.
 *     Surface writes really do move WORLD_SURFACE_WG: the badlands/iceberg
 *     extensions place snow/ice/default blocks above the current surface, and
 *     buildSurface re-reads the heightmap for that reason.
 *   - prelim_surface_level(g, ce, x, z) must be a pure function of (x, z)
 *     evaluated like NoiseChunk.preliminarySurfaceLevel (quantized to 4, then
 *     floor of the preliminary_surface_level density).  buildSurface asks for
 *     the four cell corners of the column's 16x16 cell, so x and z can be up
 *     to 16 blocks outside the chunk being generated.
 *   - biome_at_block(g, ce, x, y, z) is BiomeManager.getBiome (the fuzzy
 *     biome zoomer), used both for the column's surface biome and for
 *     SurfaceRules.Context.getBiome().
 *   - xoro_at/xoro_from_hash/xoro_fork_positional mirror
 *     PositionalRandomFactory.at / .fromHashOf / RandomSource.forkPositional,
 *     and gen_noise(g, id) mirrors RandomState.getOrCreateNoise (all pure
 *     derivations from the world positional factory).
 *   - `under_fluid` in surface_top_material is vanilla's `underFluid` flag from
 *     CarvingContext.topMaterial: non-zero means "the carved state was a
 *     fluid", which sets waterHeight = blockY + 1 in the surface context.
 *
 * Known block-name gaps (the B_* enum has no id for these; block_name() wins
 * whenever it knows the name, so extending the enum later fixes them):
 *   white/orange/yellow/brown/red/light_gray_terracotta -> B_TERRACOTTA
 *   coarse_dirt                                        -> B_DIRT
 *   cinnabar                                           -> B_TERRACOTTA
 *   sulfur                                             -> B_TERRACOTTA
 *   red_sandstone                                      -> B_SANDSTONE
 * The badlands clay-band array is therefore uniform terracotta in this port
 * even though the RNG stream (band positions) is reproduced exactly.
 */
/* Bend inlines this file into a temporary directory, so sibling includes are
 * absolute - the same convention the generated embed.h include already uses. */
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H

#include <limits.h>

/* ------------------------------------------------------------------- names */

/* Vanilla resource names, tolerating a missing "minecraft:" prefix. */
static int sr_name_eq(const char *a, const char *b) {
  if (!a || !b) return 0;
  if (!strncmp(a, "minecraft:", 10)) a += 10;
  if (!strncmp(b, "minecraft:", 10)) b += 10;
  return strcmp(a, b) == 0;
}

/* Substitutions for rule blocks that have no B_* id yet.  Only consulted when
 * block_name() does not know the name, so extending the enum supersedes them. */
static const struct {
  const char *name;
  int id;
} SR_BLOCK_SUBST[] = {
    {"minecraft:white_terracotta", B_TERRACOTTA},
    {"minecraft:orange_terracotta", B_TERRACOTTA},
    {"minecraft:yellow_terracotta", B_TERRACOTTA},
    {"minecraft:brown_terracotta", B_TERRACOTTA},
    {"minecraft:red_terracotta", B_TERRACOTTA},
    {"minecraft:light_gray_terracotta", B_TERRACOTTA},
    {"minecraft:coarse_dirt", B_DIRT},
    {"minecraft:cinnabar", B_TERRACOTTA},
    {"minecraft:sulfur", B_TERRACOTTA},
    {"minecraft:red_sandstone", B_SANDSTONE},
    /* spellings that could differ between the vanilla name and block_name() */
    {"minecraft:grass_block", B_GRASS},
    {"minecraft:grass", B_GRASS},
    {"minecraft:snow_block", B_SNOW_BLOCK},
};

/* Vanilla resource name -> B_* id, or -1 when unknown. */
static int sr_block_id(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < B_COUNT; i++) {
    const char *n = block_name(i);
    if (n && sr_name_eq(n, name)) return i;
  }
  for (unsigned i = 0; i < sizeof(SR_BLOCK_SUBST) / sizeof(SR_BLOCK_SUBST[0]); i++)
    if (sr_name_eq(SR_BLOCK_SUBST[i].name, name)) return SR_BLOCK_SUBST[i].id;
  return -1;
}

/* Vanilla resource name -> BIO_* id, or -1 when unknown. */
static int sr_biome_id(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < BIO_COUNT; i++) {
    const char *n = biome_name(i);
    if (n && sr_name_eq(n, name)) return i;
  }
  return -1;
}

/* --------------------------------------------- SimplexNoise / PerlinNoise */
/* Java: SimplexNoise (legacy-seeded permutation) + PerlinSimplexNoise, used by
 * Biome's temperature noise.  SimplexNoise.getValue(x, y) is the exact port. */

static const int SR_GRADIENT[16][3] = {
    {1, 1, 0},   {-1, 1, 0},  {1, -1, 0},  {-1, -1, 0}, {1, 0, 1},  {-1, 0, 1},
    {1, 0, -1},  {-1, 0, -1}, {0, 1, 1},   {0, -1, 1},  {0, 1, -1}, {0, -1, -1},
    {1, 1, 0},   {0, -1, 1},  {-1, 1, 0},  {0, -1, -1},
};

typedef struct {
  double f2, g2; /* Java: 0.5*(sqrt(3)-1), (3-sqrt(3))/6 */
  double xo, yo, zo;
  int p[512];
  int ok;
} SrSimplex;

typedef struct {
  SrSimplex levels[8];
  int nlevels;
  double input_factor, value_factor;
} SrPerlin;

static void sr_simplex_init(SrSimplex *s, LegacyRng *r) {
  s->f2 = 0.5 * (sqrt(3.0) - 1.0);
  s->g2 = (3.0 - sqrt(3.0)) / 6.0;
  s->xo = legacy_next_double(r) * 256.0;
  s->yo = legacy_next_double(r) * 256.0;
  s->zo = legacy_next_double(r) * 256.0;
  for (int i = 0; i < 256; i++) s->p[i] = i;
  for (int i = 0; i < 256; i++) {
    int off = legacy_next_bound(r, 256 - i);
    int tmp = s->p[i];
    s->p[i] = s->p[off + i];
    s->p[off + i] = tmp;
  }
  s->ok = 1;
}

static int sr_sp(const SrSimplex *s, int x) { return s->p[x & 0xFF]; }

static double sr_simplex_dot(const int g[3], double x, double y, double z) {
  return (double)g[0] * x + (double)g[1] * y + (double)g[2] * z;
}

static double sr_simplex_corner(int index, double x, double y, double z, double base) {
  double t0 = base - x * x - y * y - z * z;
  if (t0 < 0.0) return 0.0;
  t0 *= t0;
  return t0 * t0 * sr_simplex_dot(SR_GRADIENT[index], x, y, z);
}

static double sr_simplex2(const SrSimplex *s, double xin, double yin) {
  double sk = (xin + yin) * s->f2;
  int i = jfloor(xin + sk);
  int j = jfloor(yin + sk);
  double t = (double)(i + j) * s->g2;
  double x0 = xin - ((double)i - t);
  double y0 = yin - ((double)j - t);
  int i1, j1;
  if (x0 > y0) {
    i1 = 1;
    j1 = 0;
  } else {
    i1 = 0;
    j1 = 1;
  }
  double x1 = x0 - (double)i1 + s->g2;
  double y1 = y0 - (double)j1 + s->g2;
  double x2 = x0 - 1.0 + 2.0 * s->g2;
  double y2 = y0 - 1.0 + 2.0 * s->g2;
  int ii = i & 0xFF;
  int jj = j & 0xFF;
  int gi0 = sr_sp(s, ii + sr_sp(s, jj)) % 12;
  int gi1 = sr_sp(s, ii + i1 + sr_sp(s, jj + j1)) % 12;
  int gi2 = sr_sp(s, ii + 1 + sr_sp(s, jj + 1)) % 12;
  double n0 = sr_simplex_corner(gi0, x0, y0, 0.0, 0.5);
  double n1 = sr_simplex_corner(gi1, x1, y1, 0.0, 0.5);
  double n2 = sr_simplex_corner(gi2, x2, y2, 0.0, 0.5);
  return 70.0 * (n0 + n1 + n2);
}

static double sr_simplex3(const SrSimplex *s, double xin, double yin, double zin) {
  double sk = (xin + yin + zin) * 0.3333333333333333;
  int i = jfloor(xin + sk);
  int j = jfloor(yin + sk);
  int k = jfloor(zin + sk);
  double g3 = 0.16666666666666666;
  double t = (double)(i + j + k) * 0.16666666666666666;
  double x0 = xin - ((double)i - t);
  double y0 = yin - ((double)j - t);
  double z0 = zin - ((double)k - t);
  int i1, j1, k1, i2, j2, k2;
  if (x0 >= y0) {
    if (y0 >= z0) {
      i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0;
    } else if (x0 >= z0) {
      i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1;
    } else {
      i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1;
    }
  } else if (y0 < z0) {
    i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1;
  } else if (x0 < z0) {
    i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1;
  } else {
    i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0;
  }
  double x1 = x0 - (double)i1 + g3;
  double y1 = y0 - (double)j1 + g3;
  double z1 = z0 - (double)k1 + g3;
  double x2 = x0 - (double)i2 + 0.3333333333333333;
  double y2 = y0 - (double)j2 + 0.3333333333333333;
  double z2 = z0 - (double)k2 + 0.3333333333333333;
  double x3 = x0 - 1.0 + 0.5;
  double y3 = y0 - 1.0 + 0.5;
  double z3 = z0 - 1.0 + 0.5;
  int ii = i & 0xFF;
  int jj = j & 0xFF;
  int kk = k & 0xFF;
  int gi0 = sr_sp(s, ii + sr_sp(s, jj + sr_sp(s, kk))) % 12;
  int gi1 = sr_sp(s, ii + i1 + sr_sp(s, jj + j1 + sr_sp(s, kk + k1))) % 12;
  int gi2 = sr_sp(s, ii + i2 + sr_sp(s, jj + j2 + sr_sp(s, kk + k2))) % 12;
  int gi3 = sr_sp(s, ii + 1 + sr_sp(s, jj + 1 + sr_sp(s, kk + 1))) % 12;
  double n0 = sr_simplex_corner(gi0, x0, y0, z0, 0.6);
  double n1 = sr_simplex_corner(gi1, x1, y1, z1, 0.6);
  double n2 = sr_simplex_corner(gi2, x2, y2, z2, 0.6);
  double n3 = sr_simplex_corner(gi3, x3, y3, z3, 0.6);
  return 32.0 * (n0 + n1 + n2 + n3);
}

static int sr_octave_has(const int *octaves, int noct, int octave) {
  for (int i = 0; i < noct; i++)
    if (octaves[i] == octave) return 1;
  return 0;
}

/* Java: new PerlinSimplexNoise(new WorldgenRandom(new LegacyRandomSource(seed)),
 * ImmutableList.of(octaves...)).  WorldgenRandom delegates next(bits) to the
 * wrapped LegacyRandomSource, so a plain LegacyRng reproduces the stream. */
static void sr_perlin_init(SrPerlin *n, uint64_t seed, const int *octaves, int noct) {
  int lo = octaves[0], hi = octaves[0];
  for (int i = 1; i < noct; i++) {
    if (octaves[i] < lo) lo = octaves[i];
    if (octaves[i] > hi) hi = octaves[i];
  }
  int count = -lo + hi + 1;
  int zero_index = hi;
  for (int i = 0; i < (int)(sizeof(n->levels) / sizeof(n->levels[0])); i++) n->levels[i].ok = 0;
  if (count > (int)(sizeof(n->levels) / sizeof(n->levels[0]))) {
    fprintf(stderr, "cinder: surface: perlin octave count %d too large\n", count);
    count = (int)(sizeof(n->levels) / sizeof(n->levels[0]));
  }
  n->nlevels = count;

  LegacyRng r;
  memset(&r, 0, sizeof r); /* LEGACY_RNG_LCG */
  legacy_set_seed(&r, seed);
  SrSimplex zero;
  sr_simplex_init(&zero, &r);
  if (zero_index >= 0 && zero_index < count && sr_octave_has(octaves, noct, 0)) n->levels[zero_index] = zero;
  for (int i = zero_index + 1; i < count; i++) {
    if (i >= 0 && sr_octave_has(octaves, noct, zero_index - i)) {
      SrSimplex s;
      sr_simplex_init(&s, &r);
      n->levels[i] = s;
    } else {
      legacy_consume(&r, 262);
    }
  }
  if (hi > 0) {
    long positive_octave_seed = (long)(sr_simplex3(&zero, zero.xo, zero.yo, zero.zo) * 9.223372e18f);
    LegacyRng hr;
    memset(&hr, 0, sizeof hr); /* LEGACY_RNG_LCG */
    legacy_set_seed(&hr, (uint64_t)positive_octave_seed);
    for (int i = zero_index - 1; i >= 0; i--) {
      if (i < count && sr_octave_has(octaves, noct, zero_index - i)) {
        SrSimplex s;
        sr_simplex_init(&s, &hr);
        n->levels[i] = s;
      } else {
        legacy_consume(&hr, 262);
      }
    }
  }
  n->input_factor = pow(2.0, (double)hi);
  n->value_factor = 1.0 / (pow(2.0, (double)count) - 1.0);
}

static double sr_perlin_value(const SrPerlin *n, double x, double y, int use_noise_start) {
  double value = 0.0;
  double factor = n->input_factor;
  double value_factor = n->value_factor;
  for (int i = 0; i < n->nlevels; i++) {
    const SrSimplex *s = &n->levels[i];
    if (s->ok)
      value += sr_simplex2(s, x * factor + (use_noise_start ? s->xo : 0.0),
                           y * factor + (use_noise_start ? s->yo : 0.0)) *
               value_factor;
    factor /= 2.0;
    value_factor *= 2.0;
  }
  return value;
}

/* -------------------------------------------------- biome temperature */
/* Biome.ClimateSettings.temperature comes from the biome JSONs (the header's
 * biome_temperature); only frozen_ocean and deep_frozen_ocean carry
 * TemperatureModifier.FROZEN in the 26.2 data, so the modifier is selected by
 * biome id. */

static int sr_biome_is_frozen_modifier(int biome) {
  return biome == BIO_FROZEN_OCEAN || biome == BIO_DEEP_FROZEN_OCEAN;
}

static float surface_biome_temperature(Gen *g, int biome) {
  return (float)biome_temperature(g, biome);
}

static SrPerlin sr_pt_temperature; /* Biome.TEMPERATURE_NOISE, seed 1234 */
static SrPerlin sr_pt_frozen;      /* Biome.FROZEN_TEMPERATURE_NOISE, seed 3456 */
static SrPerlin sr_pt_biome_info;  /* Biome.BIOME_INFO_NOISE, seed 2345 */

/* Perlin instances are seeded by fixed legacy seeds (Biome's static noises), so
 * they are initialised once, independently of the world seed. */
static int sr_perlin_ready;


static void sr_init_perlins(void) {
  static const int oct0[] = {0};
  static const int oct_210[] = {-2, -1, 0};
  sr_perlin_init(&sr_pt_temperature, 1234L, oct0, 1);
  sr_perlin_init(&sr_pt_frozen, 3456L, oct_210, 3);
  sr_perlin_init(&sr_pt_biome_info, 2345L, oct0, 1);
  sr_perlin_ready = 1;
}

/* Biome.BIOME_INFO_NOISE.getValue(x, z, false) - the noise the flower-count and
 * surface-relative placements use (PerlinSimplexNoise, fixed legacy seed 2345,
 * octave 0, useNoiseStart = false). */
double biome_info_noise_value(Gen *g, double x, double z) {
  (void)g;
  if (!sr_perlin_ready) sr_init_perlins();
  return sr_perlin_value(&sr_pt_biome_info, x, z, 0);
}

/* ------------------------------------------------------ surface rule program */

enum {
  SC_ABOVE_PRELIM = 0,
  SC_BIOME,
  SC_NOISE,
  SC_STONE_DEPTH,
  SC_Y_ABOVE,
  SC_WATER,
  SC_VGRAD,
  SC_STEEP,
  SC_HOLE,
  SC_TEMPERATURE,
  SC_NOT,
};

typedef struct {
  unsigned char kind;
  unsigned char is3d;              /* SC_NOISE */
  unsigned char add_surface_depth; /* SC_STONE_DEPTH */
  unsigned char add_stone_depth;   /* SC_Y_ABOVE, SC_WATER */
  unsigned char ceiling;           /* SC_STONE_DEPTH: surface_type == ceiling */
  int offset;                      /* SC_STONE_DEPTH, SC_WATER */
  int secondary_depth_range;       /* SC_STONE_DEPTH */
  int multiplier;                  /* SC_Y_ABOVE, SC_WATER */
  int anchor_y;                    /* SC_Y_ABOVE (resolved) */
  int sub;                         /* SC_NOT */
  double min_threshold, max_threshold; /* SC_NOISE */
  int true_at_and_below, false_at_and_above; /* SC_VGRAD */
  XoroFactory vgrad_factory;       /* SC_VGRAD */
  NormalNoise *noise;              /* SC_NOISE */
  uint64_t biome_mask;             /* SC_BIOME */
} SrCond;

enum { SRK_SEQ = 0, SRK_TEST, SRK_BLOCK, SRK_BANDLANDS };

typedef struct {
  unsigned char kind;
  uint16_t block; /* SRK_BLOCK */
  int cond;       /* SRK_TEST */
  int then_rule;  /* SRK_TEST */
  int link_head, link_tail, link_count; /* SRK_SEQ */
} SrRule;

typedef struct {
  int rule, next;
} SrLink;

typedef struct {
  SrRule *rules;
  int nrules, rules_cap;
  SrCond *conds;
  int nconds, conds_cap;
  SrLink *links;
  int nlinks, links_cap;
  int root;
} SrProgram;

static int sr_arr_len(Jv *v) {
  int n = 0;
  if (!v || v->kind != J_ARR) return 0;
  for (Jv *c = v->child; c; c = c->next) n++;
  return n;
}

static Jv *sr_arr_at(Jv *v, int i) {
  if (!v || v->kind != J_ARR || i < 0) return 0;
  int k = 0;
  for (Jv *c = v->child; c; c = c->next, k++)
    if (k == i) return c;
  return 0;
}

static int sr_rule_new(SrProgram *p) {
  if (p->nrules == p->rules_cap) {
    p->rules_cap = p->rules_cap ? p->rules_cap * 2 : 64;
    p->rules = (SrRule *)realloc(p->rules, (size_t)p->rules_cap * sizeof(SrRule));
  }
  int i = p->nrules++;
  memset(&p->rules[i], 0, sizeof(SrRule));
  p->rules[i].link_head = p->rules[i].link_tail = -1;
  p->rules[i].cond = p->rules[i].then_rule = -1;
  return i;
}

static int sr_cond_new(SrProgram *p) {
  if (p->nconds == p->conds_cap) {
    p->conds_cap = p->conds_cap ? p->conds_cap * 2 : 64;
    p->conds = (SrCond *)realloc(p->conds, (size_t)p->conds_cap * sizeof(SrCond));
  }
  int i = p->nconds++;
  memset(&p->conds[i], 0, sizeof(SrCond));
  return i;
}

static void sr_link_append(SrProgram *p, int rule_idx, int child_rule) {
  if (p->nlinks == p->links_cap) {
    p->links_cap = p->links_cap ? p->links_cap * 2 : 64;
    p->links = (SrLink *)realloc(p->links, (size_t)p->links_cap * sizeof(SrLink));
  }
  int l = p->nlinks++;
  p->links[l].rule = child_rule;
  p->links[l].next = -1;
  if (p->rules[rule_idx].link_head < 0) p->rules[rule_idx].link_head = l;
  else p->links[p->rules[rule_idx].link_tail].next = l;
  p->rules[rule_idx].link_tail = l;
  p->rules[rule_idx].link_count++;
}

static int sr_compile_rule(Gen *g, SrProgram *p, Jv *v);
static int sr_compile_cond(Gen *g, SrProgram *p, Jv *c);

/* Java: VerticalAnchor.resolveY(WorldGenerationContext) for the overworld. */
static int sr_anchor_resolve(Jv *a) {
  Jv *v;
  if ((v = jget(a, "absolute"))) return (int)jnum(v, 0);
  if ((v = jget(a, "above_bottom"))) return MIN_Y + (int)jnum(v, 0);
  if ((v = jget(a, "below_top"))) return MIN_Y + HEIGHT - 1 - (int)jnum(v, 0);
  fprintf(stderr, "cinder: surface: rule anchor without a known type\n");
  return MIN_Y;
}

static void sr_cond_add_biome(SrProgram *p, int ci, const char *name) {
  int id = sr_biome_id(name);
  if (id < 0) {
    fprintf(stderr, "cinder: surface: rule references biome %s missing from BIOME_NAMES\n", name);
    return;
  }
  if (id >= 64) {
    fprintf(stderr, "cinder: surface: biome id %d out of mask range\n", id);
    return;
  }
  p->conds[ci].biome_mask |= (uint64_t)1 << id;
}

static int sr_compile_cond(Gen *g, SrProgram *p, Jv *c) {
  const char *t = jstr(jget(c, "type"));
  if (!t) {
    fprintf(stderr, "cinder: surface: condition without type\n");
    return sr_cond_new(p);
  }
  int i = sr_cond_new(p);
  if (!strcmp(t, "minecraft:above_preliminary_surface")) {
    p->conds[i].kind = SC_ABOVE_PRELIM;
  } else if (!strcmp(t, "minecraft:biome")) {
    p->conds[i].kind = SC_BIOME;
    Jv *bi = jget(c, "biome_is");
    const char *single = jstr(bi);
    if (single) {
      sr_cond_add_biome(p, i, single);
    } else {
      int n = sr_arr_len(bi);
      for (int k = 0; k < n; k++) {
        const char *nm = jstr(sr_arr_at(bi, k));
        if (nm) sr_cond_add_biome(p, i, nm);
      }
    }
  } else if (!strcmp(t, "minecraft:noise_threshold")) {
    const char *nid = jstr(jget(c, "noise"));
    p->conds[i].kind = SC_NOISE;
    p->conds[i].is3d = (unsigned char)jbool(jget(c, "is_3d"), 0);
    p->conds[i].min_threshold = jnum(jget(c, "min_threshold"), 0.0);
    p->conds[i].max_threshold = jnum(jget(c, "max_threshold"), 0.0);
    p->conds[i].noise = nid ? gen_noise(g, nid) : 0;
    if (!p->conds[i].noise)
      fprintf(stderr, "cinder: surface: rule noise %s unavailable\n", nid ? nid : "(none)");
  } else if (!strcmp(t, "minecraft:stone_depth")) {
    const char *st = jstr(jget(c, "surface_type"));
    p->conds[i].kind = SC_STONE_DEPTH;
    p->conds[i].offset = (int)jnum(jget(c, "offset"), 0);
    p->conds[i].add_surface_depth = (unsigned char)jbool(jget(c, "add_surface_depth"), 0);
    p->conds[i].secondary_depth_range = (int)jnum(jget(c, "secondary_depth_range"), 0);
    p->conds[i].ceiling = (unsigned char)(st && !strcmp(st, "ceiling"));
  } else if (!strcmp(t, "minecraft:y_above")) {
    p->conds[i].kind = SC_Y_ABOVE;
    p->conds[i].anchor_y = sr_anchor_resolve(jget(c, "anchor"));
    p->conds[i].multiplier = (int)jnum(jget(c, "surface_depth_multiplier"), 0);
    p->conds[i].add_stone_depth = (unsigned char)jbool(jget(c, "add_stone_depth"), 0);
  } else if (!strcmp(t, "minecraft:water")) {
    p->conds[i].kind = SC_WATER;
    p->conds[i].offset = (int)jnum(jget(c, "offset"), 0);
    p->conds[i].multiplier = (int)jnum(jget(c, "surface_depth_multiplier"), 0);
    p->conds[i].add_stone_depth = (unsigned char)jbool(jget(c, "add_stone_depth"), 0);
  } else if (!strcmp(t, "minecraft:vertical_gradient")) {
    const char *rn = jstr(jget(c, "random_name"));
    p->conds[i].kind = SC_VGRAD;
    p->conds[i].true_at_and_below = sr_anchor_resolve(jget(c, "true_at_and_below"));
    p->conds[i].false_at_and_above = sr_anchor_resolve(jget(c, "false_at_and_above"));
    if (rn) p->conds[i].vgrad_factory = gen_random_factory(g, rn);
  } else if (!strcmp(t, "minecraft:steep")) {
    p->conds[i].kind = SC_STEEP;
  } else if (!strcmp(t, "minecraft:hole")) {
    p->conds[i].kind = SC_HOLE;
  } else if (!strcmp(t, "minecraft:temperature")) {
    p->conds[i].kind = SC_TEMPERATURE;
  } else if (!strcmp(t, "minecraft:not")) {
    p->conds[i].kind = SC_NOT;
    int sub = sr_compile_cond(g, p, jget(c, "invert"));
    p->conds[i].sub = sub;
  } else {
    fprintf(stderr, "cinder: surface: unknown condition type %s\n", t);
  }
  return i;
}

static int sr_compile_rule(Gen *g, SrProgram *p, Jv *v) {
  const char *t = v ? jstr(jget(v, "type")) : 0;
  if (!t) {
    fprintf(stderr, "cinder: surface: rule without type\n");
    return sr_rule_new(p);
  }
  if (!strcmp(t, "minecraft:block")) {
    Jv *st = jget(v, "result_state");
    const char *name = st ? jstr(jget(st, "Name")) : 0;
    int id = sr_block_id(name);
    if (id < 0) {
      fprintf(stderr, "cinder: surface: unknown block %s\n", name ? name : "(none)");
      id = B_STONE;
    }
    int idx = sr_rule_new(p);
    p->rules[idx].kind = SRK_BLOCK;
    p->rules[idx].block = (uint16_t)id;
    return idx;
  }
  if (!strcmp(t, "minecraft:bandlands")) {
    int idx = sr_rule_new(p);
    p->rules[idx].kind = SRK_BANDLANDS;
    return idx;
  }
  if (!strcmp(t, "minecraft:sequence")) {
    Jv *seq = jget(v, "sequence");
    int n = sr_arr_len(seq);
    /* SequenceRuleSource.apply: a single-element sequence collapses. */
    if (n == 1) return sr_compile_rule(g, p, sr_arr_at(seq, 0));
    int idx = sr_rule_new(p);
    p->rules[idx].kind = SRK_SEQ;
    for (int k = 0; k < n; k++) {
      int child = sr_compile_rule(g, p, sr_arr_at(seq, k));
      sr_link_append(p, idx, child);
    }
    return idx;
  }
  if (!strcmp(t, "minecraft:condition")) {
    int cond = sr_compile_cond(g, p, jget(v, "if_true"));
    int then_rule = sr_compile_rule(g, p, jget(v, "then_run"));
    int idx = sr_rule_new(p);
    p->rules[idx].kind = SRK_TEST;
    p->rules[idx].cond = cond;
    p->rules[idx].then_rule = then_rule;
    return idx;
  }
  fprintf(stderr, "cinder: surface: unknown rule type %s\n", t);
  return sr_rule_new(p);
}

static void sr_program_free(SrProgram *p) {
  free(p->rules);
  free(p->conds);
  free(p->links);
  memset(p, 0, sizeof(*p));
  p->root = -1;
}

static void sr_program_compile(Gen *g, SrProgram *p) {
  sr_program_free(p);
  if (!g->surface_rule) {
    fprintf(stderr, "cinder: surface: no surface_rule in noise settings\n");
    return;
  }
  p->root = sr_compile_rule(g, p, g->surface_rule);
}

/* ------------------------------------------------------------ noise instances */

static NormalNoise *sr_nn_surface;
static NormalNoise *sr_nn_surface_secondary;
static NormalNoise *sr_nn_clay_offset;
static NormalNoise *sr_nn_badlands_pillar;
static NormalNoise *sr_nn_badlands_pillar_roof;
static NormalNoise *sr_nn_badlands_surface;
static NormalNoise *sr_nn_iceberg_pillar;
static NormalNoise *sr_nn_iceberg_pillar_roof;
static NormalNoise *sr_nn_iceberg_surface;


static uint16_t sr_clay_bands[192];
static uint16_t sr_default_block_id = B_STONE;

/* Java: Biome.getHeightAdjustedTemperature / getTemperature / coldEnoughToSnow. */
static float sr_height_adjusted_temperature(Gen *g, int biome, int x, int y, int z, int sea_level) {
  float adjusted = surface_biome_temperature(g, biome);
  if (sr_biome_is_frozen_modifier(biome)) {
    /* Biome.TemperatureModifier.FROZEN */
    double large = sr_perlin_value(&sr_pt_frozen, (double)x * 0.05, (double)z * 0.05, 0) * 7.0;
    double edge = sr_perlin_value(&sr_pt_biome_info, (double)x * 0.2, (double)z * 0.2, 0);
    double ice_patches = large + edge;
    if (ice_patches < 0.3) {
      double small = sr_perlin_value(&sr_pt_biome_info, (double)x * 0.09, (double)z * 0.09, 0);
      if (small < 0.8) adjusted = 0.2f;
    }
  }
  int snow_level = sea_level + 17;
  if (y > snow_level) {
    float v = (float)(sr_perlin_value(&sr_pt_temperature, (double)((float)x / 8.0f),
                                      (double)((float)z / 8.0f), 0) *
                      8.0);
    return adjusted - (v + (float)y - (float)snow_level) * 0.05f / 40.0f;
  }
  return adjusted;
}

static int sr_cold_enough_to_snow(Gen *g, int biome, int x, int y, int z) {
  float t = sr_height_adjusted_temperature(g, biome, x, y, z, g->sea_level);
  return !(t >= 0.15f);
}

static int sr_melt_iceberg_slightly(Gen *g, int biome, int x, int z) {
  float t = sr_height_adjusted_temperature(g, biome, x, g->sea_level, z, g->sea_level);
  return t > 0.1f;
}

/* Java: SurfaceSystem.getSurfaceDepth. */
static int sr_surface_depth(Gen *g, int blockX, int blockZ) {
  double noise_value = normal_noise_get(sr_nn_surface, (double)blockX, 0.0, (double)blockZ);
  Xoro r;
  xoro_at(g->world_random, blockX, 0, blockZ, &r);
  return (int)(noise_value * 2.75 + 3.0 + xoro_next_double(&r) * 0.25);
}

/* Java: SurfaceSystem.getSurfaceSecondary. */
static double sr_surface_secondary(int blockX, int blockZ) {
  return normal_noise_get(sr_nn_surface_secondary, (double)blockX, 0.0, (double)blockZ);
}

/* Java: SurfaceSystem.getBand.  Math.round(d) == floor(d + 0.5). */
static uint16_t sr_get_band(int worldX, int y, int worldZ) {
  double n = normal_noise_get(sr_nn_clay_offset, (double)worldX, 0.0, (double)worldZ);
  int offset = (int)((long)floor(n * 4.0 + 0.5));
  return sr_clay_bands[(y + offset + 192) % 192];
}

static int sr_xoro_next_bool(Xoro *r) { return (xoro_next_long(r) & 1L) != 0; }

/* Java: SurfaceSystem.generateBands(random) with the noiseRandom fromHashOf
 * ("minecraft:clay_bands"). */
static void sr_generate_bands(Gen *g) {
  Xoro r;
  xoro_from_hash(g->world_random, "minecraft:clay_bands", &r);
  for (int i = 0; i < 192; i++) sr_clay_bands[i] = B_TERRACOTTA;
  for (int i = 0; i < 192; i++) {
    i += xoro_next_bound(&r, 5) + 1;
    if (i < 192) sr_clay_bands[i] = B_TERRACOTTA; /* ORANGE_TERRACOTTA (no id) */
  }
  /* makeBands(random, bands, 1, YELLOW_TERRACOTTA) etc.  All dyed terracottas
   * collapse onto B_TERRACOTTA in this port. */
  for (int pass = 0; pass < 3; pass++) {
    int base_width = pass == 1 ? 2 : 1;
    int band_count = xoro_next_bound(&r, 10) + 6; /* nextIntBetweenInclusive(6, 15) */
    for (int i = 0; i < band_count; i++) {
      int width = base_width + xoro_next_bound(&r, 3);
      int start = xoro_next_bound(&r, 192);
      for (int p = 0; start + p < 192 && p < width; p++) sr_clay_bands[start + p] = B_TERRACOTTA;
    }
  }
  int white_band_count = xoro_next_bound(&r, 7) + 9; /* nextIntBetweenInclusive(9, 15) */
  int i = 0;
  for (int start = 0; i < white_band_count && start < 192; start += xoro_next_bound(&r, 16) + 4) {
    sr_clay_bands[start] = B_TERRACOTTA; /* WHITE_TERRACOTTA */
    if (start - 1 > 0 && sr_xoro_next_bool(&r)) sr_clay_bands[start - 1] = B_TERRACOTTA;
    if (start + 1 < 192 && sr_xoro_next_bool(&r)) sr_clay_bands[start + 1] = B_TERRACOTTA;
    i++;
  }
}

/* ------------------------------------------------------ context and evaluation */

typedef struct {
  Gen *g;
  Chunk *c;
  ChunkEval *ce;
  int blockX, blockZ, blockY;
  int surfaceDepth;
  int waterHeight;
  int stoneDepthBelow, stoneDepthAbove;
  int biome_valid, biome;
  int ms_valid, ms_cell_x, ms_cell_z, ms_level;
} SrCtx;

/* Java: SurfaceRules.Context.updateY. */
static void sr_ctx_set_y(SrCtx *cx, int y, int stone_above, int stone_below, int water_height) {
  cx->biome_valid = 0;
  cx->blockY = y;
  cx->waterHeight = water_height;
  cx->stoneDepthBelow = stone_below;
  cx->stoneDepthAbove = stone_above;
}

/* Java: SurfaceRules.Context.getBiome (biomeGetter = BiomeManager::getBiome). */
static int sr_ctx_biome(SrCtx *cx) {
  if (!cx->biome_valid) {
    cx->biome = biome_at_block_fuzzy(cx->g, cx->ce, cx->blockX, cx->blockY, cx->blockZ);
    cx->biome_valid = 1;
  }
  return cx->biome;
}

/* Java: SurfaceRules.Context.getMinSurfaceLevel (cache keyed by cell origin). */
static int sr_min_surface_level(SrCtx *cx) {
  int cell_x = cx->blockX >> 4;
  int cell_z = cx->blockZ >> 4;
  if (!cx->ms_valid || cx->ms_cell_x != cell_x || cx->ms_cell_z != cell_z) {
    int c0 = prelim_surface_level(cx->g, cx->ce, cell_x << 4, cell_z << 4);
    int c1 = prelim_surface_level(cx->g, cx->ce, (cell_x + 1) << 4, cell_z << 4);
    int c2 = prelim_surface_level(cx->g, cx->ce, cell_x << 4, (cell_z + 1) << 4);
    int c3 = prelim_surface_level(cx->g, cx->ce, (cell_x + 1) << 4, (cell_z + 1) << 4);
    double a1 = (double)((float)(cx->blockX & 15) / 16.0f);
    double a2 = (double)((float)(cx->blockZ & 15) / 16.0f);
    int preliminary = jfloor(jlerp2(a1, a2, (double)c0, (double)c1, (double)c2, (double)c3));
    cx->ms_valid = 1;
    cx->ms_cell_x = cell_x;
    cx->ms_cell_z = cell_z;
    cx->ms_level = preliminary + cx->surfaceDepth - 8;
  }
  return cx->ms_level;
}

/* Java: Mth.map (unclamped). */
static double sr_map(double v, double from_min, double from_max, double to_min, double to_max) {
  return jlerp(jinverse_lerp(v, from_min, from_max), to_min, to_max);
}

/* Java: SurfaceRules.Context.SteepMaterialCondition. */
static int sr_steep(SrCtx *cx) {
  const int32_t *hm = cx->c->hm_world_surface;
  int chunk_x = cx->blockX & 15;
  int chunk_z = cx->blockZ & 15;
  int z_north = chunk_z - 1 > 0 ? chunk_z - 1 : 0;
  int z_south = chunk_z + 1 < 15 ? chunk_z + 1 : 15;
  int height_north = hm_top(hm, chunk_x, z_north);
  int height_south = hm_top(hm, chunk_x, z_south);
  if (height_south >= height_north + 4) return 1;
  int x_west = chunk_x - 1 > 0 ? chunk_x - 1 : 0;
  int x_east = chunk_x + 1 < 15 ? chunk_x + 1 : 15;
  int height_west = hm_top(hm, x_west, chunk_z);
  int height_east = hm_top(hm, x_east, chunk_z);
  return height_west >= height_east + 4;
}

static int sr_cond_test(SrCtx *cx, const SrProgram *p, int ci) {
  const SrCond *k = &p->conds[ci];
  switch (k->kind) {
    case SC_ABOVE_PRELIM:
      return cx->blockY >= sr_min_surface_level(cx);
    case SC_BIOME: {
      int b = sr_ctx_biome(cx);
      if (b < 0 || b >= 64) return 0;
      return (k->biome_mask >> b) & 1u;
    }
    case SC_NOISE: {
      if (!k->noise) return 0;
      double value = k->is3d ? normal_noise_get(k->noise, (double)cx->blockX, (double)cx->blockY,
                                                (double)cx->blockZ)
                             : normal_noise_get(k->noise, (double)cx->blockX, 0.0, (double)cx->blockZ);
      return value >= k->min_threshold && value <= k->max_threshold;
    }
    case SC_STONE_DEPTH: {
      int stone_depth = k->ceiling ? cx->stoneDepthBelow : cx->stoneDepthAbove;
      int surface_depth = k->add_surface_depth ? cx->surfaceDepth : 0;
      int secondary = 0;
      if (k->secondary_depth_range != 0)
        secondary = (int)sr_map(sr_surface_secondary(cx->blockX, cx->blockZ), -1.0, 1.0, 0.0,
                                (double)k->secondary_depth_range);
      return stone_depth <= 1 + k->offset + surface_depth + secondary;
    }
    case SC_Y_ABOVE:
      return cx->blockY + (k->add_stone_depth ? cx->stoneDepthAbove : 0) >=
             k->anchor_y + cx->surfaceDepth * k->multiplier;
    case SC_WATER:
      return cx->waterHeight == INT_MIN ||
             cx->blockY + (k->add_stone_depth ? cx->stoneDepthAbove : 0) >=
                 cx->waterHeight + k->offset + cx->surfaceDepth * k->multiplier;
    case SC_VGRAD: {
      int y = cx->blockY;
      if (y <= k->true_at_and_below) return 1;
      if (y >= k->false_at_and_above) return 0;
      double probability = sr_map((double)y, (double)k->true_at_and_below,
                                  (double)k->false_at_and_above, 1.0, 0.0);
      Xoro r;
      xoro_at(k->vgrad_factory, cx->blockX, y, cx->blockZ, &r);
      return xoro_next_float(&r) < probability;
    }
    case SC_STEEP:
      return sr_steep(cx);
    case SC_HOLE:
      return cx->surfaceDepth <= 0;
    case SC_TEMPERATURE:
      return sr_cold_enough_to_snow(cx->g, sr_ctx_biome(cx), cx->blockX, cx->blockY, cx->blockZ);
    case SC_NOT:
      return !sr_cond_test(cx, p, k->sub);
    default:
      return 0;
  }
}

static int sr_rule_apply(SrCtx *cx, const SrProgram *p, int ri, uint16_t *out) {
  for (;;) {
    const SrRule *r = &p->rules[ri];
    switch (r->kind) {
      case SRK_BLOCK:
        *out = r->block;
        return 1;
      case SRK_BANDLANDS:
        *out = sr_get_band(cx->blockX, cx->blockY, cx->blockZ);
        return 1;
      case SRK_TEST:
        if (!sr_cond_test(cx, p, r->cond)) return 0;
        ri = r->then_rule;
        continue;
      case SRK_SEQ:
        for (int l = r->link_head; l >= 0; l = p->links[l].next)
          if (sr_rule_apply(cx, p, p->links[l].rule, out)) return 1;
        return 0;
      default:
        return 0;
    }
  }
}

/* ------------------------------------------------------------- write + heightmap */

static int sr_is_stone(uint16_t b) { return b != B_AIR && !is_fluid(b); }

/* Java: BlockColumn.setBlock + ProtoChunk.setBlockState: the block is written
 * first, then both worldgen heightmaps are updated with it. */
static void sr_col_set(Chunk *c, int x, int y, int z, uint16_t state) {
  if (!in_chunk(x, y, z)) return;
  setb(c, x, y, z, state);
  hm_update(c->hm_world_surface, c, x, y, z, HM_NOT_AIR);
  hm_update(c->hm_ocean_floor, c, x, y, z, HM_MOTION_BLOCKING);
}

/* ------------------------------------------------------------------ system */

static pthread_mutex_t sr_mu = PTHREAD_MUTEX_INITIALIZER;
static SrProgram sr_prog = {.root = -1};
static const Jv *sr_src;
static uint64_t sr_seed_lo, sr_seed_hi;
static int sr_ready;

static uint16_t sr_default_block(Gen *g) {
  Jv *db = g->overworld ? jget(g->overworld, "default_block") : 0;
  const char *name = db ? jstr(jget(db, "Name")) : 0;
  int id = sr_block_id(name);
  return (uint16_t)(id >= 0 ? id : B_STONE);
}

static void sr_system_init(Gen *g) {
  pthread_mutex_lock(&sr_mu);
  if (!sr_perlin_ready) sr_init_perlins();
  if (!sr_ready || sr_src != g->surface_rule || sr_seed_lo != g->world_random.lo ||
      sr_seed_hi != g->world_random.hi) {
    sr_nn_surface = gen_noise(g, "minecraft:surface");
    sr_nn_surface_secondary = gen_noise(g, "minecraft:surface_secondary");
    sr_nn_clay_offset = gen_noise(g, "minecraft:clay_bands_offset");
    sr_nn_badlands_pillar = gen_noise(g, "minecraft:badlands_pillar");
    sr_nn_badlands_pillar_roof = gen_noise(g, "minecraft:badlands_pillar_roof");
    sr_nn_badlands_surface = gen_noise(g, "minecraft:badlands_surface");
    sr_nn_iceberg_pillar = gen_noise(g, "minecraft:iceberg_pillar");
    sr_nn_iceberg_pillar_roof = gen_noise(g, "minecraft:iceberg_pillar_roof");
    sr_nn_iceberg_surface = gen_noise(g, "minecraft:iceberg_surface");
    sr_default_block_id = sr_default_block(g);
    sr_generate_bands(g);
    sr_program_compile(g, &sr_prog);
    sr_src = g->surface_rule;
    sr_seed_lo = g->world_random.lo;
    sr_seed_hi = g->world_random.hi;
    sr_ready = 1;
  }
  pthread_mutex_unlock(&sr_mu);
}

/* Java: SurfaceSystem.erodedBadlandsExtension. */
static void sr_eroded_badlands_extension(Chunk *c, int x, int z, int blockX, int blockZ, int height) {
  double pillar_buffer =
      jmin(fabs(normal_noise_get(sr_nn_badlands_surface, (double)blockX, 0.0, (double)blockZ) * 8.25),
           normal_noise_get(sr_nn_badlands_pillar, (double)blockX * 0.2, 0.0, (double)blockZ * 0.2) *
               15.0);
  if (!(pillar_buffer <= 0.0)) {
    double pillar_floor =
        fabs(normal_noise_get(sr_nn_badlands_pillar_roof, (double)blockX * 0.75, 0.0,
                              (double)blockZ * 0.75) *
             1.5);
    double extension_top = 64.0 + jmin(pillar_buffer * pillar_buffer * 2.5, ceil(pillar_floor * 50.0) + 24.0);
    int start_y = jfloor(extension_top);
    if (height <= start_y) {
      for (int y = start_y; y >= MIN_Y; y--) {
        uint16_t old_state = getb(c, x, y, z);
        if (old_state == sr_default_block_id) break;
        if (old_state == B_WATER) return;
      }
      for (int y = start_y; y >= MIN_Y && getb(c, x, y, z) == B_AIR; y--)
        sr_col_set(c, x, y, z, sr_default_block_id);
    }
  }
}

/* Java: SurfaceSystem.frozenOceanExtension. */
static void sr_frozen_ocean_extension(Gen *g, Chunk *c, int x, int z, int blockX, int blockZ,
                                     int height, int min_surface_level, int biome) {
  double iceberg =
      jmin(fabs(normal_noise_get(sr_nn_iceberg_surface, (double)blockX, 0.0, (double)blockZ) * 8.25),
           normal_noise_get(sr_nn_iceberg_pillar, (double)blockX * 1.28, 0.0, (double)blockZ * 1.28) *
               15.0);
  if (!(iceberg <= 1.8)) {
    double iceberg_roof =
        fabs(normal_noise_get(sr_nn_iceberg_pillar_roof, (double)blockX * 1.17, 0.0,
                              (double)blockZ * 1.17) *
             1.5);
    double top = jmin(iceberg * iceberg * 1.2, ceil(iceberg_roof * 40.0) + 14.0);
    if (sr_melt_iceberg_slightly(g, biome, blockX, blockZ)) top -= 2.0;

    double extension_bottom;
    if (top > 2.0) {
      extension_bottom = (double)g->sea_level - top - 7.0;
      top += (double)g->sea_level;
    } else {
      top = 0.0;
      extension_bottom = 0.0;
    }
    double extension_top = top;
    int ext_top = (int)extension_top;
    int ext_bottom = (int)extension_bottom;

    Xoro random;
    xoro_at(g->world_random, blockX, 0, blockZ, &random);
    int max_snow_depth = 2 + xoro_next_bound(&random, 4);
    int min_snow_height = g->sea_level + 18 + xoro_next_bound(&random, 10);
    int snow_depth = 0;

    int y0 = height > ext_top + 1 ? height : ext_top + 1;
    for (int y = y0; y >= min_surface_level; y--) {
      uint16_t cur = getb(c, x, y, z);
      int place = (cur == B_AIR && y < ext_top && xoro_next_double(&random) > 0.01) ||
                  (cur == B_WATER && y > ext_bottom && y < g->sea_level && extension_bottom != 0.0 &&
                   xoro_next_double(&random) > 0.15);
      if (place) {
        if (snow_depth <= max_snow_depth && y > min_snow_height) {
          sr_col_set(c, x, y, z, B_SNOW_BLOCK);
          snow_depth++;
        } else {
          sr_col_set(c, x, y, z, B_PACKED_ICE);
        }
      }
    }
  }
}

void gen_surface(Gen *g, Chunk *c, ChunkEval *ce) {
  sr_system_init(g);
  if (sr_prog.root < 0) return;

  int min_block_x = c->cx * 16;
  int min_block_z = c->cz * 16;
  int legacy_random = jbool(jget(g->overworld, "legacy_random_source"), 0);

  for (int x = 0; x < 16; x++) {
    for (int z = 0; z < 16; z++) {
      int blockX = min_block_x + x;
      int blockZ = min_block_z + z;
      int starting_height = c->hm_world_surface[z * 16 + x];
      int surface_biome =
          biome_at_block(g, ce, blockX, legacy_random ? 0 : starting_height, blockZ);
      if (surface_biome == BIO_ERODED_BADLANDS)
        sr_eroded_badlands_extension(c, x, z, blockX, blockZ, starting_height);

      int height = c->hm_world_surface[z * 16 + x];
      SrCtx cx;
      memset(&cx, 0, sizeof(cx));
      cx.g = g;
      cx.c = c;
      cx.ce = ce;
      cx.blockX = blockX;
      cx.blockZ = blockZ;
      cx.surfaceDepth = sr_surface_depth(g, blockX, blockZ);

      int stone_above_depth = 0;
      int water_height = INT_MIN;
      int next_ceiling_stone_y = INT_MAX;
      int end_y = MIN_Y;
      int way_below_min_y = MIN_Y * 16; /* DimensionType.WAY_BELOW_MIN_Y = MIN_Y << 4 */

      for (int y = height; y >= end_y; y--) {
        uint16_t old = getb(c, x, y, z);
        if (old == B_AIR) {
          stone_above_depth = 0;
          water_height = INT_MIN;
        } else if (is_fluid(old)) {
          if (water_height == INT_MIN) water_height = y + 1;
        } else {
          if (next_ceiling_stone_y >= y) {
            next_ceiling_stone_y = way_below_min_y;
            for (int lookahead_y = y - 1; lookahead_y >= end_y - 1; lookahead_y--) {
              uint16_t next_state = getb(c, x, lookahead_y, z);
              if (!sr_is_stone(next_state)) {
                next_ceiling_stone_y = lookahead_y + 1;
                break;
              }
            }
          }
          stone_above_depth++;
          int stone_below_depth = y - next_ceiling_stone_y + 1;
          sr_ctx_set_y(&cx, y, stone_above_depth, stone_below_depth, water_height);
          if (old == sr_default_block_id) {
            uint16_t state;
            if (sr_rule_apply(&cx, &sr_prog, sr_prog.root, &state)) sr_col_set(c, x, y, z, state);
          }
        }
      }

      if (surface_biome == BIO_FROZEN_OCEAN || surface_biome == BIO_DEEP_FROZEN_OCEAN)
        sr_frozen_ocean_extension(g, c, x, z, blockX, blockZ, starting_height,
                                  sr_min_surface_level(&cx), surface_biome);
    }
  }
}

/* CarvingContext.topMaterial: applies the surface rule to a single position.
 * `under_fluid` is vanilla's `underFluid` flag from CarvingContext.topMaterial:
 * non-zero means "the carved state was a fluid", which sets
 * waterHeight = blockY + 1 in the surface context.  Returns TOP_MATERIAL_NONE
 * for vanilla's Optional.empty(). */
uint16_t surface_top_material(Gen *g, Chunk *c, ChunkEval *ce, int lx, int ly, int lz, int under_fluid) {
  sr_system_init(g);
  if (sr_prog.root < 0) return (uint16_t)TOP_MATERIAL_NONE;

  SrCtx cx;
  memset(&cx, 0, sizeof(cx));
  cx.g = g;
  cx.c = c;
  cx.ce = ce;
  cx.blockX = c->cx * 16 + lx;
  cx.blockZ = c->cz * 16 + lz;
  cx.surfaceDepth = sr_surface_depth(g, cx.blockX, cx.blockZ);
  sr_ctx_set_y(&cx, ly, 1, 1, under_fluid ? ly + 1 : INT_MIN);

  uint16_t state;
  int ok = sr_rule_apply(&cx, &sr_prog, sr_prog.root, &state);
  return ok ? state : (uint16_t)TOP_MATERIAL_NONE;
}
