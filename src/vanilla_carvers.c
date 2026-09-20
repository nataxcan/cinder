// Carvers: exact port of vanilla 26.2 overworld cave/canyon carving.
//
// Mirrors net.minecraft.world.level.levelgen.carver.WorldCarver / CaveWorldCarver /
// CanyonWorldCarver / CarverConfiguration (+Cave/Canyon) / CarvingContext, plus the
// outer loop of NoiseBasedChunkGenerator.applyCarvers. Bit-exact on purpose: float
// vs double, evaluation order, int narrowing and RNG draw order all match the Java.
//
// The carver data is the embedded 26.2 data in src/vanilla/embed.h. Both overworld
// carvers are configured there ("carver:cave", "carver:cave_extra_underground",
// "carver:canyon"), and every overworld biome's "carvers" map has the same "air"
// list, in this order:
//     [minecraft:cave, minecraft:cave_extra_underground, minecraft:canyon]
// (the only biomes with an empty list are The End / the void, which the overworld
// biome source cannot produce). The `index` applyCarvers adds to the seed is
// therefore 0/1/2 for CARVER_ORDER below.
//
// Those three configs are parsed out of that JSON at first use (carver_cfg_load),
// like every other stage reads embed.h; nothing is hand-copied. Float values come
// out as C floats exactly as Java's Float.parseFloat of the same JSON number.
/* Bend inlines this file into a temporary directory, so sibling includes are
 * absolute - the same convention the generated embed.h include already uses. */
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H

/* ---------------------------------------------------------- carver tracing */

/* Compile-time-only debug hook: build this TU with -DCINDER_CARVER_TRACE (and optionally
   -DCINDER_CARVER_TRACE_CX/CZ) to print every random draw the carvers make for one target
   chunk, in the same format as bench/parity/carver_trace.java, so the two logs can be diffed
   event by event (bench/parity/carver_trace.sh does exactly that). The trace path only wraps
   the four LegacyRandomSource entry points the carvers use; it changes no behaviour, and nothing
   is compiled in without the macro. */
#ifdef CINDER_CARVER_TRACE
#include <stdio.h>
/* BIOME_NAMES, for the "SRC" line's biome column (the trace build only). */
#ifndef CINDER_INC_VANILLA_BIOMES_H
#define CINDER_INC_VANILLA_BIOMES_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_biomes.h"
#endif
#include CINDER_INC_VANILLA_BIOMES_H

#ifndef CINDER_CARVER_TRACE_CX
#define CINDER_CARVER_TRACE_CX 20
#endif
#ifndef CINDER_CARVER_TRACE_CZ
#define CINDER_CARVER_TRACE_CZ 11
#endif

static int carver_trace_on = 0;
/* Draws are only logged for the carver's own RNG: createTunnel()/doCarve() draw from a local
   RNG seeded from the tunnel seed, and the Java oracle only observes the carver-level RNG
   (RandomSource.createThreadLocalInstance is not interceptable), so the tunnel-local draws are
   mirrored by comparing the tunnel seeds instead. */
static const LegacyRng *carver_trace_rng = 0;

static void tr_set_seed(LegacyRng *r, uint64_t seed) {
  legacy_set_seed(r, seed);
  if (carver_trace_on && r == carver_trace_rng) printf("S %016llx\n", (unsigned long long)r->seed);
}

static float tr_next_float(LegacyRng *r) {
  float v = legacy_next_float(r);
  if (carver_trace_on && r == carver_trace_rng) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);
    printf("F %08x\n", bits);
  }
  return v;
}

static int tr_next_bound(LegacyRng *r, int bound) {
  int v = legacy_next_bound(r, bound);
  if (carver_trace_on && r == carver_trace_rng) printf("I %d %d\n", bound, v);
  return v;
}

static int64_t tr_next_long(LegacyRng *r) {
  int64_t v = legacy_next_long(r);
  if (carver_trace_on && r == carver_trace_rng) printf("L %016llx\n", (unsigned long long)(uint64_t)v);
  return v;
}

#define legacy_set_seed tr_set_seed
#define legacy_next_float tr_next_float
#define legacy_next_bound tr_next_bound
#define legacy_next_long tr_next_long

#endif /* CINDER_CARVER_TRACE */


/* --------------------------------------------------------------- constants */

/* WorldCarver.getRange() == 4 and neither cave nor canyon overrides it.
   maxDistance = SectionPos.sectionToBlockCoord(getRange() * 2 - 1) = 7 << 4. */
#define CARVER_RANGE 4
#define CARVER_MAX_DISTANCE ((CARVER_RANGE * 2 - 1) << 4)

/* ChunkAccess.isUpgrading() is false for a chunk without BelowZeroRetrogen, so
   carveEllipsoid always keeps 7 blocks of headroom under the build limit. */
#define CARVER_PROTECTED_BLOCKS_ON_TOP 7

/* SurfaceRules.RuleSource.tryApply() returns an Optional<BlockState>; this is the
   value surface_top_material() uses for "absent" (nothing to place). The header
   defines it too; the fallback keeps this file self-contained. */
#ifndef TOP_MATERIAL_NONE
#define TOP_MATERIAL_NONE 0xFFFFu
#endif

/* -------------------------------------------------- Mth.sin / Mth.cos ---- */

/* Mth builds a 65536 entry table of (float)Math.sin(i / 10430.378350470453) and
   indexes it with a truncated/scaled angle. */
#define CARVER_SIN_SCALE 10430.378350470453
#define CARVER_SIN_MASK 65535L
#define CARVER_SIN_QUANT 65536
#define CARVER_COS_OFFSET 16384.0

static float carver_sin_table[CARVER_SIN_QUANT];
static pthread_once_t carver_sin_once = PTHREAD_ONCE_INIT;

static void carver_sin_build(void) {
  for (int i = 0; i < CARVER_SIN_QUANT; i++) {
    carver_sin_table[i] = (float)sin((double)i / CARVER_SIN_SCALE);
  }
}

static float mth_sin(double i) {
  pthread_once(&carver_sin_once, carver_sin_build);
  return carver_sin_table[(int)((long)(i * CARVER_SIN_SCALE) & CARVER_SIN_MASK)];
}

static float mth_cos(double i) {
  pthread_once(&carver_sin_once, carver_sin_build);
  return carver_sin_table[(int)((long)(i * CARVER_SIN_SCALE + CARVER_COS_OFFSET) & CARVER_SIN_MASK)];
}

/* ------------------------------------------------- float / height providers */

typedef enum { FP_CONSTANT, FP_UNIFORM, FP_TRAPEZOID } FPKind;

typedef struct {
  FPKind kind;
  float min, max, plateau;
} FloatProv;

/* FloatProvider.sample(): UniformFloat/Mth.randomBetween, TrapezoidFloat, ConstantFloat. */
static float fp_sample(const FloatProv *p, LegacyRng *r) {
  switch (p->kind) {
    case FP_UNIFORM: {
      float min = p->min, max = p->max;
      if (min >= max) return min;
      return legacy_next_float(r) * (max - min) + min;
    }
    case FP_TRAPEZOID: {
      float range = p->max - p->min;
      float plateau_start = (range - p->plateau) / 2.0F;
      float plateau_end = range - plateau_start;
      float a = legacy_next_float(r);
      float b = legacy_next_float(r);
      return p->min + a * plateau_end + b * plateau_start;
    }
    default:
      return p->min; /* ConstantFloat: no draw */
  }
}

typedef enum { VA_ABSOLUTE, VA_ABOVE_BOTTOM, VA_BELOW_TOP } VAKind;

typedef struct {
  VAKind kind;
  int value;
} VertAnchor;

typedef struct {
  VertAnchor min_inc, max_inc;
  int constant; /* ConstantHeight: no draw at all (UniformHeight always draws, even for min==max) */
} HeightProv;

static int va_resolve(const VertAnchor *a, int min_gen_y, int gen_depth) {
  switch (a->kind) {
    case VA_ABSOLUTE:
      return a->value;
    case VA_ABOVE_BOTTOM:
      return min_gen_y + a->value;
    default:
      return gen_depth - 1 + min_gen_y - a->value;
  }
}

/* HeightProvider.sample(): ConstantHeight resolves without drawing; UniformHeight is
   Mth.randomBetweenInclusive, which draws unless the range is empty. */
static int hp_sample(const HeightProv *h, LegacyRng *r, int min_gen_y, int gen_depth) {
  int min = va_resolve(&h->min_inc, min_gen_y, gen_depth);
  if (h->constant) return min;
  int max = va_resolve(&h->max_inc, min_gen_y, gen_depth);
  if (min > max) return min;
  return legacy_next_bound(r, max - min + 1) + min;
}

/* ----------------------------------------------------------- carver config */

typedef enum { CARVER_CAVE, CARVER_CANYON } CarverKind;

typedef struct CarverCfg {
  CarverKind kind;
  float probability;
  HeightProv y;
  FloatProv y_scale;
  VertAnchor lava_level;
  /* cave (CaveCarverConfiguration) */
  FloatProv h_radius_mult, v_radius_mult, floor_level;
  /* canyon (CanyonCarverConfiguration + CanyonShapeConfiguration) */
  FloatProv vertical_rotation, distance_factor, thickness, h_radius_factor;
  FloatProv v_radius_random; /* Mth.randomBetween(random, 0.75F, 1.0F): code, not config */
  int width_smoothness;
  float v_radius_default_factor, v_radius_center_factor;
} CarverCfg;

/* The three configured carvers every overworld biome lists, in biome "carvers" order.
   Their values are parsed from the embedded 26.2 data by carver_cfg_load(), the same way
   the noise, density-function, settings and biome stages read embed.h: the embedded JSON
   is the source of truth, so a data change cannot silently leave a hand-copied literal
   behind. (It already had: the canyon's y min anchor was copied as 8 above the bottom
   instead of 10 absolute, which started every canyon 66 blocks too low.) */
#define CARVER_IDS 3
static CarverCfg CARVER_CFG_CAVE, CARVER_CFG_CAVE_EXTRA, CARVER_CFG_CANYON;
static CarverCfg *const CARVER_ORDER[CARVER_IDS] = {&CARVER_CFG_CAVE, &CARVER_CFG_CAVE_EXTRA,
                                                    &CARVER_CFG_CANYON};
static const char *const CARVER_KEY[CARVER_IDS] = {"carver:cave", "carver:cave_extra_underground",
                                                   "carver:canyon"};
static pthread_once_t carver_cfg_once = PTHREAD_ONCE_INIT;

/* VerticalAnchor.CODEC: a bare number is absolute, otherwise exactly one of the three fields. */
static VertAnchor carver_anchor(Jv *v) {
  if (v && v->kind == J_OBJ) {
    Jv *a = jget(v, "absolute");
    if (a) return (VertAnchor){VA_ABSOLUTE, (int)jnum(a, 0)};
    a = jget(v, "above_bottom");
    if (a) return (VertAnchor){VA_ABOVE_BOTTOM, (int)jnum(a, 0)};
    return (VertAnchor){VA_BELOW_TOP, (int)jnum(jget(v, "below_top"), 0)};
  }
  return (VertAnchor){VA_ABSOLUTE, (int)jnum(v, 0)};
}

/* FloatProviders: a bare number is ConstantFloat, else dispatch on "type". */
static FloatProv carver_float_prov(Jv *v) {
  const char *type = v && v->kind == J_OBJ ? jstr(jget(v, "type")) : 0;
  if (!type) return (FloatProv){FP_CONSTANT, (float)jnum(v, 0.0), 0.0F, 0.0F};
  if (!strcmp(type, "minecraft:uniform")) {
    return (FloatProv){FP_UNIFORM, (float)jnum(jget(v, "min_inclusive"), 0.0),
                       (float)jnum(jget(v, "max_exclusive"), 0.0), 0.0F};
  }
  if (!strcmp(type, "minecraft:trapezoid")) {
    return (FloatProv){FP_TRAPEZOID, (float)jnum(jget(v, "min"), 0.0),
                       (float)jnum(jget(v, "max"), 0.0), (float)jnum(jget(v, "plateau"), 0.0)};
  }
  return (FloatProv){FP_CONSTANT, (float)jnum(jget(v, "value"), 0.0), 0.0F, 0.0F};
}

/* HeightProvider.CODEC: a bare number is ConstantHeight (no draw), else UniformHeight. */
static HeightProv carver_height_prov(Jv *v) {
  if (v && v->kind == J_OBJ && jstr(jget(v, "type"))) {
    HeightProv h = {carver_anchor(jget(v, "min_inclusive")), carver_anchor(jget(v, "max_inclusive"))};
    h.constant = 0;
    return h;
  }
  HeightProv h = {carver_anchor(v), carver_anchor(v)};
  h.constant = 1;
  return h;
}

static void carver_cfg_load(void) {
  for (int i = 0; i < CARVER_IDS; i++) {
    const char *json = cinder_embed_get(CARVER_KEY[i]);
    if (!json) {
      fprintf(stderr, "cinder: missing carver %s\n", CARVER_KEY[i]);
      abort();
    }
    Arena tmp = {0};
    Jv *root = jparse(&tmp, json);
    Jv *cfg = jget(root, "config");
    const char *type = jstr(jget(root, "type"));
    CarverCfg *c = CARVER_ORDER[i];
    c->kind = type && !strcmp(type, "minecraft:canyon") ? CARVER_CANYON : CARVER_CAVE;
    c->probability = (float)jnum(jget(cfg, "probability"), 0.0);
    c->y = carver_height_prov(jget(cfg, "y"));
    c->y_scale = carver_float_prov(jget(cfg, "yScale"));
    c->lava_level = carver_anchor(jget(cfg, "lava_level"));
    if (c->kind == CARVER_CAVE) {
      c->h_radius_mult = carver_float_prov(jget(cfg, "horizontal_radius_multiplier"));
      c->v_radius_mult = carver_float_prov(jget(cfg, "vertical_radius_multiplier"));
      c->floor_level = carver_float_prov(jget(cfg, "floor_level"));
    } else {
      Jv *shape = jget(cfg, "shape");
      c->vertical_rotation = carver_float_prov(jget(cfg, "vertical_rotation"));
      c->distance_factor = carver_float_prov(jget(shape, "distance_factor"));
      c->thickness = carver_float_prov(jget(shape, "thickness"));
      c->h_radius_factor = carver_float_prov(jget(shape, "horizontal_radius_factor"));
      c->width_smoothness = (int)jnum(jget(shape, "width_smoothness"), 1);
      c->v_radius_default_factor = (float)jnum(jget(shape, "vertical_radius_default_factor"), 0.0);
      c->v_radius_center_factor = (float)jnum(jget(shape, "vertical_radius_center_factor"), 0.0);
      c->v_radius_random = (FloatProv){FP_UNIFORM, 0.75F, 1.0F, 0.0F};
    }
    free(tmp.arena);
  }
}

#ifdef CINDER_CARVER_TRACE
/* The configured-carver registry ids the vanilla trace prints, in CARVER_ORDER order. */
static const char *carver_trace_name(const CarverCfg *cfg) {
  return cfg == &CARVER_CFG_CAVE ? "minecraft:cave"
                                 : (cfg == &CARVER_CFG_CAVE_EXTRA ? "minecraft:cave_extra_underground"
                                                                  : "minecraft:canyon");
}
#endif

/* WorldCarver.isStartChunk(): random.nextFloat() <= configuration.probability. */
static int is_start_chunk(const CarverCfg *cfg, LegacyRng *r) {
  return legacy_next_float(r) <= cfg->probability;
}

/* Vanilla biomes that list air carvers; the overworld source only produces these, and
   all of them use CARVER_ORDER. */
static int biome_has_overworld_carvers(int biome) { return biome >= 0 && biome < BIO_COUNT; }

/* ------------------------------------------------------------- replaceable */

/* CarverConfiguration.replaceable = #minecraft:overworld_carver_replaceables.
   The tag was resolved from the 26.2 server jar (data/minecraft/tags/block json files)
   and mapped onto Cinder's block set:
     base_stone_overworld (stone, granite, diorite, andesite, deepslate, tuff),
     calcite, dirt (dirt, coarse_dirt, rooted_dirt), grass_block, podzol, mycelium,
     sand (+suspicious_sand), red_sand, sandstone (+red_sandstone), gravel
     (+suspicious_gravel), all *_terracotta, iron_ore/deepslate_iron_ore,
     copper_ore/deepslate_copper_ore, snow, snow_block, powder_snow, packed_ice,
     moss_block/pale_moss_block, mud/muddy_mangrove_roots, water.
   Vanilla tag entries with no Cinder id (raw_copper_block, raw_iron_block, cinnabar,
   sulfur, potent_sulfur) cannot occur in the ported world. */
static int can_replace(uint16_t b) {
  switch (b) {
    case B_STONE:
    case B_DIRT:
    case B_GRASS:
    case B_WATER:
    case B_SAND:
    case B_RED_SAND:
    case B_SANDSTONE:
    case B_GRAVEL:
    case B_IRON:
    case B_COPPER:
    case B_DS_IRON:
    case B_DS_COPPER:
    case B_DEEPSLATE:
    case B_GRANITE:
    case B_DIORITE:
    case B_ANDESITE:
    case B_TUFF:
    case B_CALCITE:
    case B_MOSS:
    case B_MUD:
    case B_PODZOL:
    case B_MYCELIUM:
    case B_ROOTED_DIRT:
    case B_PACKED_ICE:
    case B_POWDER_SNOW:
    case B_SNOW:
    case B_SNOW_BLOCK:
    case B_TERRACOTTA:
      return 1;
    default:
      return 0;
  }
}

/* ------------------------------------------------------------ carving mask */

/* CarvingMask(height, minY) over this chunk:
   index = x & 15 | (z & 15) << 4 | (y - minY) << 8.
   Vanilla keeps it on the proto chunk for the whole applyCarvers call (and for later
   feature placement); a per-call local is equivalent here. */
#define CARVE_MASK_BYTES (16 * 16 * HEIGHT / 8)

static inline int mask_get(const unsigned char *m, int x, int y, int z) {
  int i = (x & 15) | ((z & 15) << 4) | ((y - MIN_Y) << 8);
  return (m[i >> 3] >> (i & 7)) & 1;
}

static inline void mask_set(unsigned char *m, int x, int y, int z) {
  int i = (x & 15) | ((z & 15) << 4) | ((y - MIN_Y) << 8);
  m[i >> 3] = (unsigned char)(m[i >> 3] | (1u << (i & 7)));
}

/* ---------------------------------------------------------- carve context */

typedef struct {
  Gen *g;
  Chunk *chunk; /* the chunk being carved (writes land here) */
  ChunkEval *ce;
  Aquifer *aq;
  const CarverCfg *cfg;
  int min_gen_y, gen_depth; /* CarvingContext: -64 / 384 */
  int chunk_min_x, chunk_min_z;
  double mid_x, mid_z; /* chunkPos.getMiddleBlockX/Z */
  int lava_y;          /* configuration.lavaLevel.resolveY(context) */
} CarveCtx;

typedef enum { SKIP_FLOOR, SKIP_CANYON } SkipKind;

typedef struct {
  SkipKind kind;
  double floor_level;         /* cave */
  const float *width_factors; /* canyon, length gen_depth */
} SkipChecker;

static int skip_check(const SkipChecker *s, double xd, double yd, double zd, int y) {
  if (s->kind == SKIP_FLOOR) {
    /* CaveWorldCarver.shouldSkip */
    if (yd <= s->floor_level) return 1;
    return xd * xd + yd * yd + zd * zd >= 1.0;
  }
  /* CanyonWorldCarver.shouldSkip:
     (xd^2 + zd^2) * widthFactorPerHeight[y - minGenY - 1] + yd^2 / 6 >= 1 */
  int y_index = y - MIN_Y;
  return (xd * xd + zd * zd) * (double)s->width_factors[y_index - 1] + yd * yd / 6.0 >= 1.0;
}

static int can_reach(const CarveCtx *cc, double x, double z, int current_step, int total_steps,
                     float thickness) {
  double xd = x - cc->mid_x;
  double zd = z - cc->mid_z;
  double remaining = (double)(total_steps - current_step);
  double rr = (double)((thickness + 2.0F) + 16.0F);
  return xd * xd + zd * zd - remaining * remaining <= rr * rr;
}

/* WorldCarver.getCarveState() */
static int carve_state(const CarveCtx *cc, int world_x, int world_y, int world_z, uint16_t *out) {
  if (world_y <= cc->lava_y) {
    *out = B_LAVA; /* LAVA.createLegacyBlock() */
    return 1;
  }
  return aquifer_substance(cc->aq, cc->ce, world_x, world_y, world_z, 0.0, out);
}

/* WorldCarver.carveBlock(). Debug-mode paths are unreachable: SharedConstants.DEBUG_CARVERS
   is false and every embedded carver config's debug_settings has no "debug_mode". */
static int carve_block(const CarveCtx *cc, int lx, int world_x, int world_y, int lz, int world_z,
                       int *has_grass) {
  uint16_t block_state = getb(cc->chunk, lx, world_y, lz);
  if (block_state == B_GRASS || block_state == B_MYCELIUM) *has_grass = 1;

  if (!can_replace(block_state)) return 0;

  uint16_t state;
  if (!carve_state(cc, world_x, world_y, world_z, &state)) return 0;

  /* chunk.setBlockState(blockPos, state): the block write plus the heightmaps in
     getPersistedStatus().heightmapsAfter(). During applyCarvers the persisted status is
     still SURFACE, i.e. WORLDGEN_HEIGHTMAPS = {OCEAN_FLOOR_WG, WORLD_SURFACE_WG}. */
  setb(cc->chunk, lx, world_y, lz, state);
  hm_update(cc->chunk->hm_ocean_floor, cc->chunk, lx, world_y, lz, HM_MOTION_BLOCKING);
  hm_update(cc->chunk->hm_world_surface, cc->chunk, lx, world_y, lz, HM_NOT_AIR);

  /* vanilla: if (aquifer.shouldScheduleFluidUpdate() && !state.getFluidState().isEmpty())
                 chunk.markPosForPostProcessing(blockPos);
     There is no post-processing queue in the port, so the flag is read and dropped; it
     is still read on every successful carve, because that is what vanilla does. */
  (void)aquifer_schedule_update(cc->aq);

  if (*has_grass) {
    int helper_y = world_y - 1;
    if (getb(cc->chunk, lx, helper_y, lz) == B_DIRT) {
      int under_fluid = (state == B_WATER || state == B_LAVA);
      uint16_t top = surface_top_material(cc->g, cc->chunk, cc->ce, lx, helper_y, lz, under_fluid);
      if (top != TOP_MATERIAL_NONE) {
        setb(cc->chunk, lx, helper_y, lz, top);
        hm_update(cc->chunk->hm_ocean_floor, cc->chunk, lx, helper_y, lz, HM_MOTION_BLOCKING);
        hm_update(cc->chunk->hm_world_surface, cc->chunk, lx, helper_y, lz, HM_NOT_AIR);
      }
    }
  }
  return 1;
}

/* WorldCarver.carveEllipsoid() */
static int carve_ellipsoid(CarveCtx *cc, double x, double y, double z, double horizontal_radius,
                           double vertical_radius, unsigned char *mask, const SkipChecker *skip) {
  double max_delta = 16.0 + horizontal_radius * 2.0;
  if (fabs(x - cc->mid_x) > max_delta || fabs(z - cc->mid_z) > max_delta) return 0;

  int cmin_x = cc->chunk_min_x, cmin_z = cc->chunk_min_z;
  int a = jfloor(x - horizontal_radius) - cmin_x - 1;
  int min_x_index = a > 0 ? a : 0;
  int b = jfloor(x + horizontal_radius) - cmin_x;
  int max_x_index = b < 15 ? b : 15;
  int c = jfloor(y - vertical_radius) - 1;
  int gen_min = cc->min_gen_y + 1;
  int min_y = c > gen_min ? c : gen_min;
  int d = jfloor(y + vertical_radius) + 1;
  int top = cc->min_gen_y + cc->gen_depth - 1 - CARVER_PROTECTED_BLOCKS_ON_TOP;
  int max_y = d < top ? d : top;
  int e = jfloor(z - horizontal_radius) - cmin_z - 1;
  int min_z_index = e > 0 ? e : 0;
  int f = jfloor(z + horizontal_radius) - cmin_z;
  int max_z_index = f < 15 ? f : 15;

  int carved = 0;
  for (int x_index = min_x_index; x_index <= max_x_index; x_index++) {
    int world_x = cmin_x + x_index;
    double xd = ((double)world_x + 0.5 - x) / horizontal_radius;
    for (int z_index = min_z_index; z_index <= max_z_index; z_index++) {
      int world_z = cmin_z + z_index;
      double zd = ((double)world_z + 0.5 - z) / horizontal_radius;
      if (xd * xd + zd * zd >= 1.0) continue;
      int has_grass = 0;
      for (int world_y = max_y; world_y > min_y; world_y--) {
        double yd = ((double)world_y - 0.5 - y) / vertical_radius;
        if (skip_check(skip, xd, yd, zd, world_y)) continue;
        if (mask_get(mask, x_index, world_y, z_index)) continue;
        mask_set(mask, x_index, world_y, z_index);
        carved |= carve_block(cc, x_index, world_x, world_y, z_index, world_z, &has_grass);
      }
    }
  }
  return carved;
}

/* ------------------------------------------------------- CaveWorldCarver -- */

/* CaveWorldCarver.getCaveBound() */
#define CAVE_BOUND 15

/* CaveWorldCarver.getThickness() */
static float cave_thickness(LegacyRng *r) {
  float a = legacy_next_float(r);
  float b = legacy_next_float(r);
  float thickness = a * 2.0F + b;
  if (legacy_next_bound(r, 10) == 0) {
    float c = legacy_next_float(r);
    float d = legacy_next_float(r);
    thickness = thickness * (c * d * 3.0F + 1.0F);
  }
  return thickness;
}

/* CaveWorldCarver.createRoom() (getYScale() == 1.0 for caves) */
static void create_room(CarveCtx *cc, double x, double y, double z, float thickness, double y_scale,
                        unsigned char *mask, const SkipChecker *skip) {
  double horizontal_radius = 1.5 + (double)(mth_sin((double)(float)(M_PI / 2.0)) * thickness);
  double vertical_radius = horizontal_radius * y_scale;
  carve_ellipsoid(cc, x + 1.0, y, z, horizontal_radius, vertical_radius, mask, skip);
}

/* CaveWorldCarver.createTunnel() */
static void create_tunnel(CarveCtx *cc, int64_t tunnel_seed, double x, double y, double z,
                          double horizontal_radius_multiplier, double vertical_radius_multiplier,
                          float thickness, float horizontal_rotation, float vertical_rotation,
                          int step, int dist, double y_scale, unsigned char *mask,
                          const SkipChecker *skip) {
  LegacyRng rng;
  memset(&rng, 0, sizeof rng); /* LEGACY_RNG_LCG */
  legacy_set_seed(&rng, (uint64_t)tunnel_seed);
  int split_point = legacy_next_bound(&rng, dist / 2) + dist / 4;
  int steep = legacy_next_bound(&rng, 6) == 0;
  float y_rota = 0.0F;
  float x_rota = 0.0F;

  for (int current_step = step; current_step < dist; current_step++) {
    double horizontal_radius =
        1.5 + (double)(mth_sin((double)((float)M_PI * (float)current_step / (float)dist)) * thickness);
    double vertical_radius = horizontal_radius * y_scale;
    float cos_x = mth_cos((double)vertical_rotation);
    x = x + (double)(mth_cos((double)horizontal_rotation) * cos_x);
    y = y + (double)mth_sin((double)vertical_rotation);
    z = z + (double)(mth_sin((double)horizontal_rotation) * cos_x);
    vertical_rotation = vertical_rotation * (steep ? 0.92F : 0.7F);
    vertical_rotation = vertical_rotation + x_rota * 0.1F;
    horizontal_rotation = horizontal_rotation + y_rota * 0.1F;
    x_rota = x_rota * 0.9F;
    y_rota = y_rota * 0.75F;
    float ax = legacy_next_float(&rng);
    float ay = legacy_next_float(&rng);
    float az = legacy_next_float(&rng);
    x_rota = x_rota + ((ax - ay) * az * 2.0F);
    float bx = legacy_next_float(&rng);
    float by = legacy_next_float(&rng);
    float bz = legacy_next_float(&rng);
    y_rota = y_rota + ((bx - by) * bz * 4.0F);

    if (current_step == split_point && thickness > 1.0F) {
      int64_t seed1 = legacy_next_long(&rng);
      float thickness1 = legacy_next_float(&rng) * 0.5F + 0.5F;
      create_tunnel(cc, seed1, x, y, z, horizontal_radius_multiplier, vertical_radius_multiplier,
                    thickness1, horizontal_rotation - (float)(M_PI / 2.0), vertical_rotation / 3.0F,
                    current_step, dist, 1.0, mask, skip);
      int64_t seed2 = legacy_next_long(&rng);
      float thickness2 = legacy_next_float(&rng) * 0.5F + 0.5F;
      create_tunnel(cc, seed2, x, y, z, horizontal_radius_multiplier, vertical_radius_multiplier,
                    thickness2, horizontal_rotation + (float)(M_PI / 2.0), vertical_rotation / 3.0F,
                    current_step, dist, 1.0, mask, skip);
      return;
    }

    if (legacy_next_bound(&rng, 4) != 0) {
      if (!can_reach(cc, x, z, current_step, dist, thickness)) return;
      carve_ellipsoid(cc, x, y, z, horizontal_radius * horizontal_radius_multiplier,
                      vertical_radius * vertical_radius_multiplier, mask, skip);
    }
  }
}

/* CaveWorldCarver.carve() */
static void carve_cave(CarveCtx *cc, LegacyRng *rng, int src_cx, int src_cz, unsigned char *mask) {
  const CarverCfg *cfg = cc->cfg;
  int src_min_x = src_cx * 16, src_min_z = src_cz * 16;
  int a = legacy_next_bound(rng, CAVE_BOUND);
  int b = legacy_next_bound(rng, a + 1);
  int cave_count = legacy_next_bound(rng, b + 1);

  for (int cave = 0; cave < cave_count; cave++) {
    double x = (double)(src_min_x + legacy_next_bound(rng, 16));
    double y = (double)hp_sample(&cfg->y, rng, cc->min_gen_y, cc->gen_depth);
    double z = (double)(src_min_z + legacy_next_bound(rng, 16));
    double horizontal_radius_multiplier = (double)fp_sample(&cfg->h_radius_mult, rng);
    double vertical_radius_multiplier = (double)fp_sample(&cfg->v_radius_mult, rng);
    double floor_level = (double)fp_sample(&cfg->floor_level, rng);
    SkipChecker skip = {SKIP_FLOOR, floor_level, 0};
    int tunnels = 1;
    if (legacy_next_bound(rng, 4) == 0) {
      double y_scale = (double)fp_sample(&cfg->y_scale, rng);
      float thickness = 1.0F + legacy_next_float(rng) * 6.0F;
      create_room(cc, x, y, z, thickness, y_scale, mask, &skip);
      tunnels += legacy_next_bound(rng, 4);
    }

    for (int i = 0; i < tunnels; i++) {
      float horizontal_rotation = legacy_next_float(rng) * (float)(M_PI * 2.0);
      float vertical_rotation = (legacy_next_float(rng) - 0.5F) / 4.0F;
      float thickness = cave_thickness(rng);
      int distance = CARVER_MAX_DISTANCE - legacy_next_bound(rng, CARVER_MAX_DISTANCE / 4);
      int64_t tunnel_seed = legacy_next_long(rng);
      create_tunnel(cc, tunnel_seed, x, y, z, horizontal_radius_multiplier,
                    vertical_radius_multiplier, thickness, horizontal_rotation, vertical_rotation, 0,
                    distance, 1.0, mask, &skip);
    }
  }
}

/* ----------------------------------------------------- CanyonWorldCarver -- */

/* CanyonWorldCarver.updateVerticalRadius() */
static double update_vertical_radius(const CarveCtx *cc, LegacyRng *r, double vertical_radius,
                                     float distance, float current_step) {
  float vertical_multiplier = 1.0F - fabsf(0.5F - current_step / distance) * 2.0F;
  float factor =
      cc->cfg->v_radius_default_factor + cc->cfg->v_radius_center_factor * vertical_multiplier;
  double base = (double)factor * vertical_radius;
  return base * (double)fp_sample(&cc->cfg->v_radius_random, r);
}

/* CanyonWorldCarver.initWidthFactors() */
static void init_width_factors(const CarveCtx *cc, LegacyRng *r, float *out) {
  int depth = cc->gen_depth;
  float width_factor = 1.0F;
  for (int y_index = 0; y_index < depth; y_index++) {
    if (y_index == 0 || legacy_next_bound(r, cc->cfg->width_smoothness) == 0) {
      float a = legacy_next_float(r);
      float b = legacy_next_float(r);
      width_factor = 1.0F + a * b;
    }
    out[y_index] = width_factor * width_factor;
  }
}

/* CanyonWorldCarver.doCarve() */
static void do_carve(CarveCtx *cc, int64_t tunnel_seed, double x, double y, double z, float thickness,
                     float horizontal_rotation, float vertical_rotation, int step, int distance,
                     double y_scale, unsigned char *mask) {
  const CarverCfg *cfg = cc->cfg;
  LegacyRng rng;
  memset(&rng, 0, sizeof rng); /* LEGACY_RNG_LCG */
  legacy_set_seed(&rng, (uint64_t)tunnel_seed);
  float width_factors[HEIGHT];
  init_width_factors(cc, &rng, width_factors);
  SkipChecker skip = {SKIP_CANYON, 0.0, width_factors};
  float y_rota = 0.0F;
  float x_rota = 0.0F;

  for (int current_step = step; current_step < distance; current_step++) {
    double horizontal_radius =
        1.5 +
        (double)(mth_sin((double)((float)current_step * (float)M_PI / (float)distance)) * thickness);
    double vertical_radius = horizontal_radius * y_scale;
    horizontal_radius = horizontal_radius * (double)fp_sample(&cfg->h_radius_factor, &rng);
    vertical_radius =
        update_vertical_radius(cc, &rng, vertical_radius, (float)distance, (float)current_step);
    float xc = mth_cos((double)vertical_rotation);
    float xs = mth_sin((double)vertical_rotation);
    x = x + (double)(mth_cos((double)horizontal_rotation) * xc);
    y = y + (double)xs;
    z = z + (double)(mth_sin((double)horizontal_rotation) * xc);
    vertical_rotation = vertical_rotation * 0.7F;
    vertical_rotation = vertical_rotation + x_rota * 0.05F;
    horizontal_rotation = horizontal_rotation + y_rota * 0.05F;
    x_rota = x_rota * 0.8F;
    y_rota = y_rota * 0.5F;
    float ax = legacy_next_float(&rng);
    float ay = legacy_next_float(&rng);
    float az = legacy_next_float(&rng);
    x_rota = x_rota + ((ax - ay) * az * 2.0F);
    float bx = legacy_next_float(&rng);
    float by = legacy_next_float(&rng);
    float bz = legacy_next_float(&rng);
    y_rota = y_rota + ((bx - by) * bz * 4.0F);

    if (legacy_next_bound(&rng, 4) != 0) {
      if (!can_reach(cc, x, z, current_step, distance, thickness)) return;
      carve_ellipsoid(cc, x, y, z, horizontal_radius, vertical_radius, mask, &skip);
    }
  }
}

/* CanyonWorldCarver.carve() */
static void carve_canyon(CarveCtx *cc, LegacyRng *rng, int src_cx, int src_cz, unsigned char *mask) {
  const CarverCfg *cfg = cc->cfg;
  int src_min_x = src_cx * 16, src_min_z = src_cz * 16;
  double x = (double)(src_min_x + legacy_next_bound(rng, 16));
  int y = hp_sample(&cfg->y, rng, cc->min_gen_y, cc->gen_depth);
  double z = (double)(src_min_z + legacy_next_bound(rng, 16));
  float horizontal_rotation = legacy_next_float(rng) * (float)(M_PI * 2.0);
  float vertical_rotation = fp_sample(&cfg->vertical_rotation, rng);
  double y_scale = (double)fp_sample(&cfg->y_scale, rng);
  float thickness = fp_sample(&cfg->thickness, rng);
  int distance = (int)((float)CARVER_MAX_DISTANCE * fp_sample(&cfg->distance_factor, rng));
  int64_t tunnel_seed = legacy_next_long(rng);
  do_carve(cc, tunnel_seed, x, (double)y, z, thickness, horizontal_rotation, vertical_rotation, 0,
           distance, y_scale, mask);
}

/* ------------------------------------------------------------- entry point */

/* NoiseBasedChunkGenerator.applyCarvers(): 17x17 source chunks around this one; the
   source biome's carver list (always cave, cave_extra_underground, canyon here) with
   random.setLargeFeatureSeed(worldSeed + index, sourcePos.x, sourcePos.z) on the shared
   WorldgenRandom, then isStartChunk() and carve() writing into this chunk. */
void gen_carvers(Gen *g, Chunk *c, ChunkEval *ce) {
  pthread_once(&carver_cfg_once, carver_cfg_load);
  unsigned char mask[CARVE_MASK_BYTES];
  memset(mask, 0, sizeof mask);

  CarveCtx cc;
  cc.g = g;
  cc.chunk = c;
  cc.ce = ce;
  cc.aq = ce->aquifer;
  cc.min_gen_y = MIN_Y;
  cc.gen_depth = HEIGHT;
  cc.chunk_min_x = c->cx * 16;
  cc.chunk_min_z = c->cz * 16;
  cc.mid_x = (double)(cc.chunk_min_x + 8);
  cc.mid_z = (double)(cc.chunk_min_z + 8);

  LegacyRng rng;
  memset(&rng, 0, sizeof rng); /* LEGACY_RNG_LCG */
#ifdef CINDER_CARVER_TRACE
  carver_trace_on = (c->cx == CINDER_CARVER_TRACE_CX && c->cz == CINDER_CARVER_TRACE_CZ);
  carver_trace_rng = &rng;
  if (carver_trace_on) printf("# seed %llu target %d %d\n", (unsigned long long)g->world_seed, c->cx, c->cz);
#endif
  for (int dx = -8; dx <= 8; dx++) {
    for (int dz = -8; dz <= 8; dz++) {
      int src_cx = c->cx + dx, src_cz = c->cz + dz;
      int src_min_x = src_cx * 16, src_min_z = src_cz * 16;
      int biome = biome_at_quart(g, ce, src_min_x >> 2, 0, src_min_z >> 2);
      if (!biome_has_overworld_carvers(biome)) continue;
      for (int index = 0; index < CARVER_IDS; index++) {
        const CarverCfg *cfg = CARVER_ORDER[index];
#ifdef CINDER_CARVER_TRACE
        if (carver_trace_on) {
          printf("SRC %d %d %d %s %s\n", src_cx, src_cz, index, BIOME_NAMES[biome],
                 carver_trace_name(cfg));
        }
#endif
        legacy_set_large_feature_seed(&rng, g->world_seed + (uint64_t)index, src_cx, src_cz);
#ifdef CINDER_CARVER_TRACE
        if (carver_trace_on) printf("S %016llx\n", (unsigned long long)rng.seed);
#endif
        int started = is_start_chunk(cfg, &rng);
#ifdef CINDER_CARVER_TRACE
        if (carver_trace_on) printf("X %d\n", started);
#endif
        if (!started) continue;
        cc.cfg = cfg;
        cc.lava_y = va_resolve(&cfg->lava_level, cc.min_gen_y, cc.gen_depth);
        if (cfg->kind == CARVER_CAVE) {
          carve_cave(&cc, &rng, src_cx, src_cz, mask);
        } else {
          carve_canyon(&cc, &rng, src_cx, src_cz, mask);
        }
      }
    }
  }
#ifdef CINDER_CARVER_TRACE
  if (carver_trace_on) printf("# end\n");
#endif
}
