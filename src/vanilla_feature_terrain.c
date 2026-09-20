// Terrain configured features: ores, disks, springs, lakes, block blobs, geodes,
// speleothem clusters, large dripstone, sculk patches, ice, fossils, monster
// rooms, desert wells, spikes and the two selector features.
//
// Mirrors net.minecraft.world.level.levelgen.feature.* (26.2) line by line:
// same draw order, same float/double types, same iteration order. Where vanilla
// reads a block *property* (log axis, pointed-dripstone thickness/direction,
// waterlogged, chest facing, multiface vein faces) the port matches the *block*
// only, because Cinder's palette is name level; every such substitution is
// listed in this file's comments and in the port report.
/* This file is inlined into the worldgen translation unit, so sibling includes
 * are absolute - the same convention the generated embed.h include uses. */
#ifndef CINDER_INC_VANILLA_FEATURE_COMMON_H
#define CINDER_INC_VANILLA_FEATURE_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_common.h"
#endif
#include CINDER_INC_VANILLA_FEATURE_COMMON_H

/* ============================================================== Mth subset ==
 * net.minecraft.util.Mth helpers the features below use, kept bit-identical:
 * the sine table is Mth's 65536-entry (float)Math.sin(i / 10430.378350470453)
 * table (the carvers' port builds the same one), invSqrt is org.joml.Math.invsqrt,
 * which for doubles is exactly 1.0 / Math.sqrt(x) (verified against joml-1.10.8:
 * bit-equal to 1/sqrt over 200k random doubles). */

#define TERRAIN_SIN_SCALE 10430.378350470453

static float terrain_sin_table[65536];
static int terrain_sin_ready;

static void terrain_sin_init(void) {
  for (int i = 0; i < 65536; i++) {
    terrain_sin_table[i] = (float)sin((double)i / TERRAIN_SIN_SCALE);
  }
  terrain_sin_ready = 1;
}

/* Mth.sin(double) */
static float t_sin(double v) {
  if (!terrain_sin_ready) terrain_sin_init();
  return terrain_sin_table[(int)((long long)(v * TERRAIN_SIN_SCALE) & 65535LL)];
}

/* Mth.cos(double) */
static float t_cos(double v) {
  if (!terrain_sin_ready) terrain_sin_init();
  return terrain_sin_table[(int)((long long)(v * TERRAIN_SIN_SCALE + 16384.0) & 65535LL)];
}

static float t_sqrt_f(float x) { return (float)sqrt((double)x); }
static double t_inv_sqrt(double x) { return 1.0 / sqrt(x); }
static int t_floor_d(double v) { return (int)floor(v); }
static int t_floor_f(float v) { return (int)floor((double)v); }
static int t_ceil_f(float v) { return (int)ceil((double)v); }
static int t_ceil_d(double v) { return (int)ceil(v); }
static int t_abs_i(int v) { return v < 0 ? -v : v; }
static float t_abs_f(float v) { return v < 0.0F ? -v : v; }
static int t_min_i(int a, int b) { return a < b ? a : b; }
static int t_max_i(int a, int b) { return a > b ? a : b; }

/* Math.min/Math.max/Mth.clamp: NaN in, NaN out (Java semantics). */
static float t_min_f(float a, float b) {
  if (a != a) return a;
  if (b != b) return b;
  return a < b ? a : b;
}

static float t_max_f(float a, float b) {
  if (a != a) return a;
  if (b != b) return b;
  return a > b ? a : b;
}

/* Math.min/Math.max for doubles, NaN preserving. */
static double t_min_d(double a, double b) {
  if (a != a) return a;
  if (b != b) return b;
  return a < b ? a : b;
}

static double t_max_d(double a, double b) {
  if (a != a) return a;
  if (b != b) return b;
  return a > b ? a : b;
}

/* Mth.clamp(float, float, float) */
static float t_clamp_f(float v, float lo, float hi) { return v < lo ? lo : t_min_f(v, hi); }

/* Mth.clamp(double, double, double) */
static double t_clamp_d(double v, double lo, double hi) { return v < lo ? lo : (v < hi ? v : hi); }

/* Mth.clamp(int, int, int) */
static int t_clamp_i(int v, int lo, int hi) { return t_min_i(t_max_i(v, lo), hi); }

/* Mth.randomBetweenInclusive(random, min, maxInclusive) */
static int t_random_between_i(LegacyRng *r, int lo, int hi) { return legacy_next_bound(r, hi - lo + 1) + lo; }

/* Mth.randomBetween(random, min, maxExclusive) - float arithmetic */
static float t_random_between_f(LegacyRng *r, float lo, float hi) {
  return legacy_next_float(r) * (hi - lo) + lo;
}

/* Mth.clampedLerp(factor, min, max) */
static float t_clamped_lerp_f(float f, float lo, float hi) {
  if (f < 0.0F) return lo;
  return f > 1.0F ? hi : lo + f * (hi - lo);
}

static double t_clamped_lerp_d(double f, double lo, double hi) {
  if (f < 0.0) return lo;
  return f > 1.0 ? hi : lo + f * (hi - lo);
}

/* Mth.clampedMap(value, fromMin, fromMax, toMin, toMax) */
static float t_clamped_map_f(float v, float fmin, float fmax, float tmin, float tmax) {
  return t_clamped_lerp_f((v - fmin) / (fmax - fmin), tmin, tmax);
}

static double t_clamped_map_d(double v, double fmin, double fmax, double tmin, double tmax) {
  return t_clamped_lerp_d((v - fmin) / (fmax - fmin), tmin, tmax);
}

/* Mth.normal(random, mean, deviation) */
static float t_normal(LegacyRng *r, float mean, float dev) {
  return mean + (float)legacy_next_gaussian(r) * dev;
}

/* ================================================== Json block helpers ==== */

/* BlockState JSON ({"Name": "minecraft:x", "Properties": {...}}) or a bare
 * name -> Cinder block id, -1 when unknown. Properties are dropped (Cinder's
 * palette is name level). */
static int t_state_id(Jv *state) {
  if (!state) return -1;
  const char *name = state->kind == J_STR ? state->str : jstr(jget(state, "Name"));
  if (!name) return -1;
  return block_id_for_name(name);
}

/* HolderSet<Block> JSON: a "#tag" string, a block name string, or a list of
 * either (Inventory: HolderSetCodec's "string or list of strings" form). */
static int t_holder_set_contains(Jv *set, uint16_t block) {
  if (!set) return 0;
  if (set->kind == J_STR) {
    const char *name = set->str;
    if (!name) return 0;
    if (name[0] == '#') return block_tag_contains(name + 1, block);
    int id = block_id_for_name(name);
    return id >= 0 && (uint16_t)id == block;
  }
  if (set->kind == J_ARR) {
    for (Jv *e = set->child; e; e = e->next) {
      if (e->kind != J_STR || !e->str) continue;
      const char *name = e->str;
      if (name[0] == '#') {
        if (block_tag_contains(name + 1, block)) return 1;
      } else {
        int id = block_id_for_name(name);
        if (id >= 0 && (uint16_t)id == block) return 1;
      }
    }
  }
  return 0;
}

/* RuleTest (OreConfiguration.target): blockState.is(block) / .is(tag) /
 * state == state. RandomBlockMatchTest tests the block *before* drawing, which
 * is why the draw sits behind the block check here. */
static int t_rule_test(Jv *test, uint16_t state, LegacyRng *r) {
  if (!test) return 0;
  const char *t = jstr(jget(test, "predicate_type"));
  if (!t) return 0;
  if (!strncmp(t, "minecraft:", 10)) t += 10;
  if (!strcmp(t, "always_true")) return 1;
  if (!strcmp(t, "block_match")) {
    int id = block_id_for_name(jstr(jget(test, "block")));
    return id >= 0 && (uint16_t)id == state;
  }
  if (!strcmp(t, "tag_match")) {
    const char *tag = jstr(jget(test, "tag"));
    return tag && block_tag_contains(tag, state);
  }
  if (!strcmp(t, "random_block_match")) {
    int id = block_id_for_name(jstr(jget(test, "block")));
    if (!(id >= 0 && (uint16_t)id == state)) return 0;
    return legacy_next_float(r) < (float)jnum(jget(test, "probability"), 0);
  }
  if (!strcmp(t, "blockstate_match")) {
    int id = t_state_id(jget(test, "state"));
    return id >= 0 && (uint16_t)id == state;
  }
  if (!strcmp(t, "random_blockstate_match")) {
    int id = t_state_id(jget(test, "state"));
    if (!(id >= 0 && (uint16_t)id == state)) return 0;
    return legacy_next_float(r) < (float)jnum(jget(test, "probability"), 0);
  }
  return 0;
}

/* BlockStateBase.isAir(): air, cave_air and void_air are all "air". */
static int t_is_air(uint16_t b) { return b == B_AIR || b == B_CAVE_AIR; }

/* BlockStateBase.isSolid() / Block.isShapeFullBlock(getCollisionShape()): over
 * Cinder's palette these coincide (empty collision shape -> not solid; a full
 * cube -> solid), because the only sub-cube shapes in the palette are the plant
 * and layer blocks, whose bounds are well under vanilla's 35/48 cutoff. */
static int t_is_solid(uint16_t b) { return feat_is_collision_full(b); }

/* FluidState.is(FluidTags.WATER) for the palette: water itself plus the
 * waterlogged-in-practice plants Cinder stores without the property. */
static int t_water_fluid(uint16_t b) { return b == B_WATER || b == B_KELP || b == B_SEAGRASS; }

static int t_is_empty_or_water(uint16_t b) { return t_is_air(b) || b == B_WATER; }
static int t_is_empty_or_water_or_lava(uint16_t b) {
  return t_is_air(b) || b == B_WATER || b == B_LAVA;
}

/* Direction helpers. Vanilla ordinals: 0 DOWN, 1 UP, 2 NORTH, 3 SOUTH,
 * 4 WEST, 5 EAST; Direction.Plane.HORIZONTAL is NORTH, EAST, SOUTH, WEST. */
#define T_DIR_DOWN 0
#define T_DIR_UP 1
#define T_DIR_NORTH 2
#define T_DIR_SOUTH 3
#define T_DIR_WEST 4
#define T_DIR_EAST 5
static const int T_ALL_DIRS[6] = {T_DIR_DOWN, T_DIR_UP, T_DIR_NORTH, T_DIR_SOUTH, T_DIR_WEST, T_DIR_EAST};
static const int T_HORIZONTAL[4] = {T_DIR_NORTH, T_DIR_EAST, T_DIR_SOUTH, T_DIR_WEST};

static void t_dir_vec(int dir, int *dx, int *dy, int *dz) {
  *dx = *dy = *dz = 0;
  switch (dir) {
    case T_DIR_DOWN: *dy = -1; break;
    case T_DIR_UP: *dy = 1; break;
    case T_DIR_NORTH: *dz = -1; break;
    case T_DIR_SOUTH: *dz = 1; break;
    case T_DIR_WEST: *dx = -1; break;
    default: *dx = 1; break;
  }
}

static int t_dir_opposite(int dir) { return dir ^ 1; }

/* Vec3i.distSqr */
static double t_dist_sqr(int x0, int y0, int z0, int x1, int y1, int z1) {
  double dx = (double)(x0 - x1);
  double dy = (double)(y0 - y1);
  double dz = (double)(z0 - z1);
  return dx * dx + dy * dy + dz * dz;
}

/* Vec3i.closerThan(pos, distance) */
static int t_closer_than(int x, int y, int z, int ox, int oy, int oz, double d) {
  return t_dist_sqr(x, y, z, ox, oy, oz) < d * d;
}

/* Util.shuffledCopy(int[] ...) / Util.shuffle(list, random): Fisher-Yates from
 * the end, so `i` draws nextInt(i) for i = size down to 2. */
static void t_shuffle6(LegacyRng *r, int *dirs) {
  for (int i = 6; i > 1; i--) {
    int swap_to = legacy_next_bound(r, i);
    int tmp = dirs[i - 1];
    dirs[i - 1] = dirs[swap_to];
    dirs[swap_to] = tmp;
  }
}

/* Direction.allShuffled(random) */
static void t_all_dirs_shuffled(LegacyRng *r, int *dirs) {
  for (int i = 0; i < 6; i++) dirs[i] = i;
  t_shuffle6(r, dirs);
}

/* Util.getRandom(List, random) */
static Jv *t_get_random_jv(Jv *list, LegacyRng *r) {
  int n = jarr_len(list);
  if (n <= 0) return 0;
  return jget_idx(list, legacy_next_bound(r, n));
}

/* Column.scan over the window: returns 1 when the origin is inside the column
 * and stores the floor / ceiling Y (has_* = 0 when vanilla's OptionalInt is
 * empty). */
typedef struct {
  int has_floor, floor_y;
  int has_ceiling, ceiling_y;
} TColumn;

/* IntProvider.getMinValue()/getMaxValue() for the provider shapes the terrain
 * configs use (the clamp/uniform/trapezoid/bottom-biased families all carry
 * min_inclusive/max_inclusive; constant and weighted_list are derived). */
static int t_int_provider_min(Jv *p) {
  const char *t = p ? jstr(jget(p, "type")) : 0;
  if (t && !strncmp(t, "minecraft:", 10)) t += 10;
  if (!t || !strcmp(t, "uniform") || !strcmp(t, "clamped") || !strcmp(t, "trapezoid") ||
      !strcmp(t, "biased_to_bottom") || !strcmp(t, "very_biased_to_bottom")) {
    return (int)jnum(jget(p, "min_inclusive"), 0);
  }
  if (!strcmp(t, "clamped_normal")) return (int)jnum(jget(p, "min_inclusive"), 0);
  if (!strcmp(t, "constant")) return (int)jnum(jget(p, "value"), 0);
  if (!strcmp(t, "weighted_list")) {
    int best = INT32_MAX;
    Jv *dist = jget(p, "distribution");
    for (Jv *e = dist ? dist->child : 0; e; e = e->next) {
      int v = t_int_provider_min(jget(e, "data"));
      if (v < best) best = v;
    }
    return best == INT32_MAX ? 0 : best;
  }
  return (int)jnum(jget(p, "min_inclusive"), 0);
}

static int t_int_provider_max(Jv *p) {
  const char *t = p ? jstr(jget(p, "type")) : 0;
  if (t && !strncmp(t, "minecraft:", 10)) t += 10;
  if (!t || !strcmp(t, "uniform") || !strcmp(t, "clamped") || !strcmp(t, "trapezoid") ||
      !strcmp(t, "biased_to_bottom") || !strcmp(t, "very_biased_to_bottom")) {
    return (int)jnum(jget(p, "max_inclusive"), 0);
  }
  if (!strcmp(t, "clamped_normal")) return (int)jnum(jget(p, "max_inclusive"), 0);
  if (!strcmp(t, "constant")) return (int)jnum(jget(p, "value"), 0);
  if (!strcmp(t, "weighted_list")) {
    int best = INT32_MIN;
    Jv *dist = jget(p, "distribution");
    for (Jv *e = dist ? dist->child : 0; e; e = e->next) {
      int v = t_int_provider_max(jget(e, "data"));
      if (v > best) best = v;
    }
    return best == INT32_MIN ? 0 : best;
  }
  return (int)jnum(jget(p, "max_inclusive"), 0);
}

/* Column.scanDirection, shared by the speleothem/large-dripstone/magma ports.
 * `inside` and `edge` are predicate/context pairs over block ids; a NULL edge
 * predicate means "always an edge" (vanilla's callers always pass one). */
typedef int (*TBlockPred)(uint16_t b, const void *ctx);

static int t_scan_direction(FeatureCtx *fc, int x, int y, int z, int search_range, TBlockPred inside,
                            const void *inside_ctx, TBlockPred edge, const void *edge_ctx, int dir,
                            int *out_y) {
  int cy = y;
  int dx, dy, dz;
  t_dir_vec(dir, &dx, &dy, &dz);
  for (int i = 1; i < search_range && inside(feat_get(fc, x, cy, z), inside_ctx); i++) {
    cy += dy;
  }
  if (edge && edge(feat_get(fc, x, cy, z), edge_ctx)) {
    *out_y = cy;
    return 1;
  }
  return 0;
}

/* Column.scan(level, pos, range, inside, edge) */
static int t_column_scan_full(FeatureCtx *fc, int x, int y, int z, int search_range, TBlockPred inside,
                              const void *inside_ctx, TBlockPred edge, const void *edge_ctx, TColumn *out) {
  if (!inside(feat_get(fc, x, y, z), inside_ctx)) return 0;
  out->has_ceiling =
      t_scan_direction(fc, x, y, z, search_range, inside, inside_ctx, edge, edge_ctx, T_DIR_UP, &out->ceiling_y);
  out->has_floor =
      t_scan_direction(fc, x, y, z, search_range, inside, inside_ctx, edge, edge_ctx, T_DIR_DOWN, &out->floor_y);
  return 1;
}

/* SpeleothemUtils.isBase / isBaseOrLava / isNeitherEmptyNorWater */
static int t_is_base(uint16_t state, uint16_t base_block, Jv *replaceable) {
  return state == base_block || t_holder_set_contains(replaceable, state);
}

static int t_is_base_or_lava(uint16_t state, uint16_t base_block, Jv *replaceable) {
  return t_is_base(state, base_block, replaceable) || state == B_LAVA;
}

static int t_neither_empty_nor_water(uint16_t b) { return !t_is_air(b) && b != B_WATER; }

/* The predicate/context pairs vanilla's Column.scan callers use. */
typedef struct {
  uint16_t base_block;
  Jv *replaceable;
} TColumnEdgeCtx;

static int t_pred_empty_or_water(uint16_t b, const void *ctx) {
  (void)ctx;
  return t_is_empty_or_water(b);
}

static int t_pred_neither_empty_nor_water(uint16_t b, const void *ctx) {
  (void)ctx;
  return t_neither_empty_nor_water(b);
}

static int t_pred_water(uint16_t b, const void *ctx) {
  (void)ctx;
  return b == B_WATER;
}

static int t_pred_not_water(uint16_t b, const void *ctx) {
  (void)ctx;
  return b != B_WATER;
}

static int t_pred_base_or_lava(uint16_t b, const void *ctx) {
  const TColumnEdgeCtx *c = (const TColumnEdgeCtx *)ctx;
  return t_is_base_or_lava(b, c->base_block, c->replaceable);
}

/* ============================================================== ore ========
 * OreFeature: the sin/cos vein through the origin, the size-driven shell loop,
 * the BlockStateProvider per target state and the replaceable RuleTest. */

static int t_can_place_ore(FeatureCtx *fc, uint16_t ore_pos_state, Jv *config, Jv *target_state, LegacyRng *r,
                           int ox, int oy, int oz);

/* Feature.isAdjacentToAir(blockGetter, pos) */
static int t_adjacent_to_air(FeatureCtx *fc, int x, int y, int z) {
  for (int i = 0; i < 6; i++) {
    int dx, dy, dz;
    t_dir_vec(T_ALL_DIRS[i], &dx, &dy, &dz);
    if (t_is_air(feat_get(fc, x + dx, y + dy, z + dz))) return 1;
  }
  return 0;
}

/* OreFeature.shouldSkipAirCheck, from the 26.2 bytecode (it reads backwards but
 * this is what the jar does):
 *   fload_1; fconst_0; fcmpg; ifgt -> chance <= 0 returns TRUE (skip the check)
 *   fload_1; fconst_1; fcmpl; iflt -> chance >= 1 returns FALSE
 *   otherwise: nextFloat() >= chance
 * So discard_chance_on_air_exposure = 0 (every overworld ore) means the air
 * check is SKIPPED, and the draw only happens for 0 < chance < 1. */
static int t_should_skip_air_check(LegacyRng *r, float discard_chance) {
  if (discard_chance <= 0.0F) return 1;
  if (discard_chance >= 1.0F) return 0;
  return legacy_next_float(r) >= discard_chance;
}

/* OreFeature.canPlaceOre */
static int t_can_place_ore(FeatureCtx *fc, uint16_t ore_pos_state, Jv *config, Jv *target_state, LegacyRng *r,
                           int ox, int oy, int oz) {
  if (!t_rule_test(jget(target_state, "target"), ore_pos_state, r)) return 0;
  if (t_should_skip_air_check(r, (float)jnum(jget(config, "discard_chance_on_air_exposure"), 0))) return 1;
  return !t_adjacent_to_air(fc, ox, oy, oz);
}

/* OreFeature.doPlace */
static int t_ore_do_place(FeatureCtx *fc, Jv *config, double x0, double x1, double z0, double z1, double y0,
                          double y1, int x_start, int y_start, int z_start, int size_xz, int size_y) {
  LegacyRng *r = feat_rng(fc);
  int placed = 0;
  int size = (int)jnum(jget(config, "size"), 0);
  if (size <= 0) return 0;
  size_t bit_count = (size_t)size_xz * (size_t)size_y * (size_t)size_xz;
  unsigned char *tested = (unsigned char *)calloc(bit_count, 1);
  double *data = (double *)calloc((size_t)size * 4, sizeof(double));
  if (!tested || !data) {
    free(tested);
    free(data);
    return 0;
  }

  for (int i = 0; i < size; i++) {
    float step = (float)i / (float)size;
    double xx = x0 + (double)step * (x1 - x0);
    double yy = y0 + (double)step * (y1 - y0);
    double zz = z0 + (double)step * (z1 - z0);
    double ss = legacy_next_double(r) * (double)size / 16.0;
    /* Mth.sin((float) Math.PI * step): the argument is a float product, the
       result is a float, and the + 1.0F stays in float before the double mul. */
    float sin_v = t_sin((double)(3.14159274101257324F * step));
    double radius = ((double)(sin_v + 1.0F) * ss + 1.0) / 2.0;
    data[i * 4 + 0] = xx;
    data[i * 4 + 1] = yy;
    data[i * 4 + 2] = zz;
    data[i * 4 + 3] = radius;
  }

  for (int i1 = 0; i1 < size - 1; i1++) {
    if (data[i1 * 4 + 3] <= 0.0) continue;
    for (int i2 = i1 + 1; i2 < size; i2++) {
      if (data[i2 * 4 + 3] <= 0.0) continue;
      double dx = data[i1 * 4 + 0] - data[i2 * 4 + 0];
      double dy = data[i1 * 4 + 1] - data[i2 * 4 + 1];
      double dz = data[i1 * 4 + 2] - data[i2 * 4 + 2];
      double dr = data[i1 * 4 + 3] - data[i2 * 4 + 3];
      if (dr * dr > dx * dx + dy * dy + dz * dz) {
        if (dr > 0.0) {
          data[i2 * 4 + 3] = -1.0;
        } else {
          data[i1 * 4 + 3] = -1.0;
        }
      }
    }
  }

  for (int i = 0; i < size; i++) {
    double radius = data[i * 4 + 3];
    if (radius < 0.0) continue;
    double xx = data[i * 4 + 0];
    double yy = data[i * 4 + 1];
    double zz = data[i * 4 + 2];
    int x_min = t_max_i(t_floor_d(xx - radius), x_start);
    int y_min = t_max_i(t_floor_d(yy - radius), y_start);
    int z_min = t_max_i(t_floor_d(zz - radius), z_start);
    int x_max = t_max_i(t_floor_d(xx + radius), x_min);
    int y_max = t_max_i(t_floor_d(yy + radius), y_min);
    int z_max = t_max_i(t_floor_d(zz + radius), z_min);

    for (int x = x_min; x <= x_max; x++) {
      double xd = ((double)x + 0.5 - xx) / radius;
      if (!(xd * xd < 1.0)) continue;
      for (int y = y_min; y <= y_max; y++) {
        double yd = ((double)y + 0.5 - yy) / radius;
        if (!(xd * xd + yd * yd < 1.0)) continue;
        for (int z = z_min; z <= z_max; z++) {
          double zd = ((double)z + 0.5 - zz) / radius;
          if (!(xd * xd + yd * yd + zd * zd < 1.0)) continue;
          if (y < MIN_Y || y >= MIN_Y + HEIGHT) continue; /* isOutsideBuildHeight */
          size_t bit = (size_t)(x - x_start) + (size_t)(y - y_start) * (size_t)size_xz +
                       (size_t)(z - z_start) * (size_t)size_xz * (size_t)size_y;
          if (bit >= bit_count || tested[bit]) continue;
          tested[bit] = 1;
          if (!feat_in_window(fc, x, y, z)) continue; /* ensureCanWrite */
          uint16_t here = feat_get(fc, x, y, z);
          Jv *targets = jget(config, "targets");
          for (Jv *ts = targets ? targets->child : 0; ts; ts = ts->next) {
            if (t_can_place_ore(fc, here, config, ts, r, x, y, z)) {
              int id = t_state_id(jget(ts, "state"));
              if (id >= 0) {
                feat_set(fc, x, y, z, (uint16_t)id);
                placed++;
              }
              break;
            }
          }
        }
      }
    }
  }

  free(tested);
  free(data);
  return placed > 0;
}

static int feature_ore(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  int size = (int)jnum(jget(config, "size"), 0);
  float dir = legacy_next_float(r) * 3.14159274101257324F;
  float spread_xz = (float)size / 8.0F;
  int max_radius = t_ceil_f(((float)size / 16.0F * 2.0F + 1.0F) / 2.0F);
  double x0 = (double)ox + sin((double)dir) * (double)spread_xz;
  double x1 = (double)ox - sin((double)dir) * (double)spread_xz;
  double z0 = (double)oz + cos((double)dir) * (double)spread_xz;
  double z1 = (double)oz - cos((double)dir) * (double)spread_xz;
  double y0 = (double)(oy + legacy_next_bound(r, 3) - 2);
  double y1 = (double)(oy + legacy_next_bound(r, 3) - 2);
  int x_start = ox - t_ceil_f(spread_xz) - max_radius;
  int y_start = oy - 2 - max_radius;
  int z_start = oz - t_ceil_f(spread_xz) - max_radius;
  int size_xz = 2 * (t_ceil_f(spread_xz) + max_radius);
  int size_y = 2 * (2 + max_radius);

  for (int xprobe = x_start; xprobe <= x_start + size_xz; xprobe++) {
    for (int zprobe = z_start; zprobe <= z_start + size_xz; zprobe++) {
      if (y_start <= feat_height(fc, xprobe, zprobe, FEAT_HM_OCEAN_FLOOR_WG)) {
        return t_ore_do_place(fc, config, x0, x1, z0, z1, y0, y1, x_start, y_start, z_start, size_xz, size_y);
      }
    }
  }
  return 0;
}

/* ============================================================== disk =======
 * DiskFeature: a horizontal disc of radius `radius`, half_height either side. */

static int t_disk_place_column(FeatureCtx *fc, Jv *config, int x, int top, int bottom, int z) {
  Jv *target = jget(config, "target");
  Jv *provider = jget(config, "state_provider");
  int placed_any = 0;
  int placed_above = 0;

  for (int y = top; y > bottom; y--) {
    if (feat_block_predicate(fc, target, x, y, z)) {
      uint16_t state = 0;
      if (feat_state_provider(fc, provider, x, y, z, &state)) {
        feat_set(fc, x, y, z, state);
        if (!placed_above) {
          /* DiskFeature.markAboveForPostProcessing: post-processing only. */
        }
        placed_any = 1;
        placed_above = 1;
      }
    } else {
      placed_above = 0;
    }
  }
  return placed_any;
}

static int feature_disk(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  int placed_any = 0;
  int top = oy + (int)jnum(jget(config, "half_height"), 0);
  int bottom = oy - (int)jnum(jget(config, "half_height"), 0) - 1;
  int radius = feat_int_provider(jget(config, "radius"), r);

  for (int z = oz - radius; z <= oz + radius; z++) {
    for (int x = ox - radius; x <= ox + radius; x++) {
      int xd = x - ox;
      int zd = z - oz;
      if (xd * xd + zd * zd <= radius * radius) {
        placed_any |= t_disk_place_column(fc, config, x, top, bottom, z);
      }
    }
  }
  return placed_any;
}

/* ============================================================== spring =====
 * SpringFeature: a fluid blob that needs the surrounding rock/hole counts. */

static int feature_spring(FeatureCtx *fc, Jv *config) {
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  Jv *valid = jget(config, "valid_blocks");
  int requires_below = jbool(jget(config, "requires_block_below"), 1);
  int rock_count_cfg = (int)jnum(jget(config, "rock_count"), 4);
  int hole_count_cfg = (int)jnum(jget(config, "hole_count"), 1);

  if (!t_holder_set_contains(valid, feat_get(fc, ox, oy + 1, oz))) return 0;
  if (requires_below && !t_holder_set_contains(valid, feat_get(fc, ox, oy - 1, oz))) return 0;
  uint16_t current = feat_get(fc, ox, oy, oz);
  if (!t_is_air(current) && !t_holder_set_contains(valid, current)) return 0;

  int placed = 0;
  int rock_count = 0;
  if (t_holder_set_contains(valid, feat_get(fc, ox - 1, oy, oz))) rock_count++;
  if (t_holder_set_contains(valid, feat_get(fc, ox + 1, oy, oz))) rock_count++;
  if (t_holder_set_contains(valid, feat_get(fc, ox, oy, oz - 1))) rock_count++;
  if (t_holder_set_contains(valid, feat_get(fc, ox, oy, oz + 1))) rock_count++;
  if (t_holder_set_contains(valid, feat_get(fc, ox, oy - 1, oz))) rock_count++;

  int hole_count = 0;
  if (t_is_air(feat_get(fc, ox - 1, oy, oz))) hole_count++;
  if (t_is_air(feat_get(fc, ox + 1, oy, oz))) hole_count++;
  if (t_is_air(feat_get(fc, ox, oy, oz - 1))) hole_count++;
  if (t_is_air(feat_get(fc, ox, oy, oz + 1))) hole_count++;
  if (t_is_air(feat_get(fc, ox, oy - 1, oz))) hole_count++;

  if (rock_count == rock_count_cfg && hole_count == hole_count_cfg) {
    /* SpringConfiguration.state is a FluidState: Cinder stores the fluid's
     * block (water/lava). scheduleTick(fluid) has no worldgen effect. */
    int id = t_state_id(jget(config, "state"));
    if (id >= 0) feat_set(fc, ox, oy, oz, (uint16_t)id);
    placed++;
  }
  return placed > 0;
}

/* ========================================================== block_blob =====
 * BlockBlobFeature: sink to the support block, then three growing blobs. */

static int feature_block_blob(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  Jv *can_place_on = jget(config, "can_place_on");
  int id = t_state_id(jget(config, "state"));
  if (id < 0) return 0;

  while (oy > MIN_Y + 3 && !feat_block_predicate(fc, can_place_on, ox, oy - 1, oz)) {
    oy--;
  }
  if (oy <= MIN_Y + 3) return 0;

  for (int c = 0; c < 3; c++) {
    int xr = legacy_next_bound(r, 2);
    int yr = legacy_next_bound(r, 2);
    int zr = legacy_next_bound(r, 2);
    float tr = (float)(xr + yr + zr) * 0.333F + 0.5F;

    for (int z = oz - zr; z <= oz + zr; z++) {
      for (int y = oy - yr; y <= oy + yr; y++) {
        for (int x = ox - xr; x <= ox + xr; x++) {
          if (t_dist_sqr(x, y, z, ox, oy, oz) <= (double)(tr * tr)) {
            feat_set(fc, x, y, z, (uint16_t)id);
          }
        }
      }
    }

    ox += -1 + legacy_next_bound(r, 2);
    oy += -legacy_next_bound(r, 2);
    oz += -1 + legacy_next_bound(r, 2);
  }
  return 1;
}

/* ========================================================== desert_well ====
 * DesertWellFeature: sandstone ring, slab details and two suspicious-sand
 * block entities. The loot-table block entity is not modelled by Cinder; the
 * block itself is placed exactly. */
static int feature_desert_well(FeatureCtx *fc, Jv *config) {
  (void)config;
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y + 1, oz = fc->origin_z;

  int b_sand = block_id_for_name("minecraft:sand");
  int b_sandstone = block_id_for_name("minecraft:sandstone");
  int b_slab = block_id_for_name("minecraft:sandstone_slab");
  int b_water = B_WATER;
  int b_sus = block_id_for_name("minecraft:suspicious_sand");
  if (b_sand < 0 || b_sandstone < 0 || b_slab < 0) return 0;

  while (t_is_air(feat_get(fc, ox, oy, oz)) && oy > MIN_Y + 2) oy--;
  if (feat_get(fc, ox, oy, oz) != (uint16_t)b_sand) return 0;

  for (int x = -2; x <= 2; x++) {
    for (int z = -2; z <= 2; z++) {
      if (t_is_air(feat_get(fc, ox + x, oy - 1, oz + z)) && t_is_air(feat_get(fc, ox + x, oy - 2, oz + z))) {
        return 0;
      }
    }
  }

  for (int y = -2; y <= 0; y++) {
    for (int x = -2; x <= 2; x++) {
      for (int z = -2; z <= 2; z++) {
        feat_set(fc, ox + x, oy + y, oz + z, (uint16_t)b_sandstone);
      }
    }
  }

  feat_set(fc, ox, oy, oz, (uint16_t)b_water);
  for (int i = 0; i < 4; i++) {
    int dx, dy, dz;
    t_dir_vec(T_HORIZONTAL[i], &dx, &dy, &dz);
    feat_set(fc, ox + dx, oy, oz + dz, (uint16_t)b_water);
  }

  feat_set(fc, ox, oy - 1, oz, (uint16_t)b_sand);
  for (int i = 0; i < 4; i++) {
    int dx, dy, dz;
    t_dir_vec(T_HORIZONTAL[i], &dx, &dy, &dz);
    feat_set(fc, ox + dx, oy - 1, oz + dz, (uint16_t)b_sand);
  }

  for (int x = -2; x <= 2; x++) {
    for (int z = -2; z <= 2; z++) {
      if (x == -2 || x == 2 || z == -2 || z == 2) {
        feat_set(fc, ox + x, oy + 1, oz + z, (uint16_t)b_sandstone);
      }
    }
  }

  feat_set(fc, ox + 2, oy + 1, oz, (uint16_t)b_slab);
  feat_set(fc, ox - 2, oy + 1, oz, (uint16_t)b_slab);
  feat_set(fc, ox, oy + 1, oz + 2, (uint16_t)b_slab);
  feat_set(fc, ox, oy + 1, oz - 2, (uint16_t)b_slab);

  for (int x = -1; x <= 1; x++) {
    for (int z = -1; z <= 1; z++) {
      if (x == 0 && z == 0) {
        feat_set(fc, ox + x, oy + 4, oz + z, (uint16_t)b_sandstone);
      } else {
        feat_set(fc, ox + x, oy + 4, oz + z, (uint16_t)b_slab);
      }
    }
  }

  for (int y = 1; y <= 3; y++) {
    feat_set(fc, ox - 1, oy + y, oz - 1, (uint16_t)b_sandstone);
    feat_set(fc, ox - 1, oy + y, oz + 1, (uint16_t)b_sandstone);
    feat_set(fc, ox + 1, oy + y, oz - 1, (uint16_t)b_sandstone);
    feat_set(fc, ox + 1, oy + y, oz + 1, (uint16_t)b_sandstone);
  }

  if (b_sus >= 0) {
    int wx = ox, wz = oz;
    for (int i = 0; i < 2; i++) {
      int pick = legacy_next_bound(r, 5);
      switch (pick) {
        case 0: wx = ox; wz = oz; break;
        case 1: wx = ox + 1; wz = oz; break;
        case 2: wx = ox; wz = oz + 1; break;
        case 3: wx = ox - 1; wz = oz; break;
        default: wx = ox; wz = oz - 1; break;
      }
      int sy = oy - 1 - i;
      feat_set(fc, wx, sy, wz, (uint16_t)b_sus);
    }
  } else {
    legacy_next_bound(r, 5);
    legacy_next_bound(r, 5);
  }
  return 1;
}

/* ============================================================== spike ======
 * SpikeFeature (the ice spike). */
static int feature_spike(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  Jv *can_place_on = jget(config, "can_place_on");
  Jv *can_replace = jget(config, "can_replace");
  int id = t_state_id(jget(config, "state"));
  if (id < 0) return 0;

  while (t_is_air(feat_get(fc, ox, oy, oz)) && oy > MIN_Y + 2) oy--;
  if (!feat_block_predicate(fc, can_place_on, ox, oy, oz)) return 0;

  oy += legacy_next_bound(r, 4);
  int height = legacy_next_bound(r, 4) + 7;
  int width = height / 4 + legacy_next_bound(r, 2);
  if (width > 1 && legacy_next_bound(r, 60) == 0) {
    oy += 10 + legacy_next_bound(r, 30);
  }

  for (int y_off = 0; y_off < height; y_off++) {
    float scale = (1.0F - (float)y_off / (float)height) * (float)width;
    int new_width = t_ceil_f(scale);

    for (int xo = -new_width; xo <= new_width; xo++) {
      float dx = (float)t_abs_i(xo) - 0.25F;
      for (int zo = -new_width; zo <= new_width; zo++) {
        float dz = (float)t_abs_i(zo) - 0.25F;
        int inside = (xo == 0 && zo == 0) || !(dx * dx + dz * dz > scale * scale);
        int not_on_edge = (xo != -new_width && xo != new_width && zo != -new_width && zo != new_width);
        if (inside && (not_on_edge || !(legacy_next_float(r) > 0.75F))) {
          int px = ox + xo, py = oy + y_off, pz = oz + zo;
          uint16_t state = feat_get(fc, px, py, pz);
          if (t_is_air(state) || feat_block_predicate(fc, can_replace, px, py, pz)) {
            feat_set(fc, px, py, pz, (uint16_t)id);
          }
          if (y_off != 0 && new_width > 1) {
            int nx = ox + xo, ny = oy - y_off, nz = oz + zo;
            state = feat_get(fc, nx, ny, nz);
            if (t_is_air(state) || feat_block_predicate(fc, can_replace, nx, ny, nz)) {
              feat_set(fc, nx, ny, nz, (uint16_t)id);
            }
          }
        }
      }
    }
  }

  int pillar_width = width - 1;
  if (pillar_width < 0) {
    pillar_width = 0;
  } else if (pillar_width > 1) {
    pillar_width = 1;
  }

  for (int xo = -pillar_width; xo <= pillar_width; xo++) {
    for (int zo = -pillar_width; zo <= pillar_width; zo++) {
      int cx = ox + xo, cy = oy - 1, cz = oz + zo;
      int run_length = 50;
      if (t_abs_i(xo) == 1 && t_abs_i(zo) == 1) {
        run_length = legacy_next_bound(r, 5);
      }
      while (cy > 50) {
        uint16_t state = feat_get(fc, cx, cy, cz);
        if (!t_is_air(state) && !feat_block_predicate(fc, can_replace, cx, cy, cz) &&
            (int)state != id) {
          break;
        }
        feat_set(fc, cx, cy, cz, (uint16_t)id);
        cy--;
        if (--run_length <= 0) {
          cy -= legacy_next_bound(r, 5) + 1;
          run_length = legacy_next_bound(r, 5);
        }
      }
    }
  }
  return 1;
}

/* ============================================================== lake =======
 * LakeFeature: a random blob of spheres, its validation pass, the empty/fluid
 * fill, the barrier shell and the freeze pass. */

/* BlockStateProvider.getState: getOptionalState with the world's own block as
 * the fallback (RuleBasedStateProvider.getState does exactly that). */
static uint16_t t_state_or_world(FeatureCtx *fc, Jv *provider, int x, int y, int z) {
  uint16_t out = 0;
  if (feat_state_provider(fc, provider, x, y, z, &out)) return out;
  return feat_get(fc, x, y, z);
}

/* BlockStateBase.liquid(): the fluid state is non-empty. Cinder stores the
 * waterlogged-in-practice plants without their property, so kelp/seagrass count
 * as liquid like they do in vanilla. */
static int t_liquid(uint16_t b) { return feat_is_fluid(b) || b == B_KELP || b == B_SEAGRASS; }

static int feature_lake(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  if (oy <= MIN_Y + 4) return 0;

  ox -= 8;
  oy -= 4;
  oz -= 8;
  unsigned char grid[2048];
  memset(grid, 0, sizeof grid);
  int spots = legacy_next_bound(r, 4) + 4;

  for (int i = 0; i < spots; i++) {
    double xr = legacy_next_double(r) * 6.0 + 3.0;
    double yr = legacy_next_double(r) * 4.0 + 2.0;
    double zr = legacy_next_double(r) * 6.0 + 3.0;
    double xp = legacy_next_double(r) * (16.0 - xr - 2.0) + 1.0 + xr / 2.0;
    double yp = legacy_next_double(r) * (8.0 - yr - 4.0) + 2.0 + yr / 2.0;
    double zp = legacy_next_double(r) * (16.0 - zr - 2.0) + 1.0 + zr / 2.0;

    for (int xx = 1; xx < 15; xx++) {
      for (int zz = 1; zz < 15; zz++) {
        for (int yy = 1; yy < 7; yy++) {
          double xd = ((double)xx - xp) / (xr / 2.0);
          double yd = ((double)yy - yp) / (yr / 2.0);
          double zd = ((double)zz - zp) / (zr / 2.0);
          double d = xd * xd + yd * yd + zd * zd;
          if (d < 1.0) grid[(xx * 16 + zz) * 8 + yy] = 1;
        }
      }
    }
  }

  uint16_t fluid = t_state_or_world(fc, jget(config, "fluid"), ox, oy, oz);

  for (int xx = 0; xx < 16; xx++) {
    for (int zz = 0; zz < 16; zz++) {
      for (int yy = 0; yy < 8; yy++) {
        int check = !grid[(xx * 16 + zz) * 8 + yy] &&
                    ((xx < 15 && grid[((xx + 1) * 16 + zz) * 8 + yy]) ||
                     (xx > 0 && grid[((xx - 1) * 16 + zz) * 8 + yy]) ||
                     (zz < 15 && grid[(xx * 16 + zz + 1) * 8 + yy]) ||
                     (zz > 0 && grid[(xx * 16 + zz - 1) * 8 + yy]) ||
                     (yy < 7 && grid[(xx * 16 + zz) * 8 + yy + 1]) ||
                     (yy > 0 && grid[(xx * 16 + zz) * 8 + yy - 1]));
        if (check) {
          int px = ox + xx, py = oy + yy, pz = oz + zz;
          uint16_t state = feat_get(fc, px, py, pz);
          if (yy >= 4 && t_liquid(state)) return 0;
          if (yy < 4 && !t_is_solid(state) && state != fluid) return 0;
          if (!feat_block_predicate(fc, jget(config, "can_place_feature"), px, py, pz)) return 0;
        }
      }
    }
  }

  for (int xx = 0; xx < 16; xx++) {
    for (int zz = 0; zz < 16; zz++) {
      for (int yy = 0; yy < 8; yy++) {
        if (grid[(xx * 16 + zz) * 8 + yy]) {
          int px = ox + xx, py = oy + yy, pz = oz + zz;
          if (feat_block_predicate(fc, jget(config, "can_replace_with_air_or_fluid"), px, py, pz)) {
            int place_air = yy >= 4;
            feat_set(fc, px, py, pz, place_air ? B_CAVE_AIR : fluid);
            /* scheduleTick(CAVE_AIR) and markAboveForPostProcessing only affect
               post-processing. */
          }
        }
      }
    }
  }

  uint16_t barrier = t_state_or_world(fc, jget(config, "barrier"), ox, oy, oz);
  if (!t_is_air(barrier)) {
    for (int xx = 0; xx < 16; xx++) {
      for (int zz = 0; zz < 16; zz++) {
        for (int yy = 0; yy < 8; yy++) {
          int check = !grid[(xx * 16 + zz) * 8 + yy] &&
                      ((xx < 15 && grid[((xx + 1) * 16 + zz) * 8 + yy]) ||
                       (xx > 0 && grid[((xx - 1) * 16 + zz) * 8 + yy]) ||
                       (zz < 15 && grid[(xx * 16 + zz + 1) * 8 + yy]) ||
                       (zz > 0 && grid[(xx * 16 + zz - 1) * 8 + yy]) ||
                       (yy < 7 && grid[(xx * 16 + zz) * 8 + yy + 1]) ||
                       (yy > 0 && grid[(xx * 16 + zz) * 8 + yy - 1]));
          if (check && (yy < 4 || legacy_next_bound(r, 2) != 0)) {
            int px = ox + xx, py = oy + yy, pz = oz + zz;
            uint16_t state = feat_get(fc, px, py, pz);
            if (t_is_solid(state) && feat_block_predicate(fc, jget(config, "can_replace_with_barrier"), px, py, pz)) {
              feat_set(fc, px, py, pz, barrier);
            }
          }
        }
      }
    }
  }

  if (t_water_fluid(fluid)) {
    for (int xx = 0; xx < 16; xx++) {
      for (int zz = 0; zz < 16; zz++) {
        int px = ox + xx, py = oy + 4, pz = oz + zz;
        /* Biome.shouldFreeze(level, pos, false) = !warmEnoughToRain &&
           insideBuildHeight && blockLight < 10 (0 during worldgen) &&
           fluidState.is(WATER) && block instanceof LiquidBlock (so the water
           block itself, not a waterlogged plant). The TemperatureModifier
           variants (frozen ocean) are not modelled - see the report. */
        int cold = biome_temperature(fc->g, feat_biome(fc, px, py, pz)) < 0.15F;
        if (cold && feat_get(fc, px, py, pz) == B_WATER &&
            feat_block_predicate(fc, jget(config, "can_replace_with_air_or_fluid"), px, py, pz)) {
          feat_set(fc, px, py, pz, B_ICE);
        }
      }
    }
  }

  return 1;
}

/* ===================================================== underwater_magma ====
 * UnderwaterMagmaFeature: find the floor of the water column below the origin,
 * then probabilistically place magma in the (2r+1)^3 box around it. */

/* state.getFaceOcclusionShape(dir) == Shapes.empty() || !isShapeFullBlock */
static int t_visible_from_outside(FeatureCtx *fc, int x, int y, int z, int covered_dir) {
  return !feat_occludes_face(feat_get(fc, x, y, z), covered_dir);
}

static int t_magma_is_valid(FeatureCtx *fc, int x, int y, int z) {
  uint16_t state = feat_get(fc, x, y, z);
  if (t_is_air(state) || state == B_WATER) return 0;
  if (t_visible_from_outside(fc, x, y - 1, z, T_DIR_UP)) return 0;
  for (int i = 0; i < 4; i++) {
    int dir = T_HORIZONTAL[i];
    int dx, dy, dz;
    t_dir_vec(dir, &dx, &dy, &dz);
    if (t_visible_from_outside(fc, x + dx, y, z + dz, t_dir_opposite(dir))) return 0;
  }
  return 1;
}

static int feature_underwater_magma(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  int search_range = (int)jnum(jget(config, "floor_search_range"), 5);
  int radius = (int)jnum(jget(config, "placement_radius_around_floor"), 1);
  float probability = (float)jnum(jget(config, "placement_probability_per_valid_position"), 0.5);
  int floor_y = 0;
  int placed = 0;

  /* Column.scan(level, origin, range, state -> state.is(WATER),
   *            state -> !state.is(WATER)).map(Column::getFloor) */
  if (feat_get(fc, ox, oy, oz) != B_WATER) return 0;
  if (!t_scan_direction(fc, ox, oy, oz, search_range, t_pred_water, 0, t_pred_not_water, 0, T_DIR_DOWN,
                        &floor_y)) {
    return 0;
  }

  /* BlockPos.betweenClosedStream(BoundingBox.fromCorners(floorPos - r, floorPos + r)):
     x fastest, then y, then z. The probability filter draws once per position,
     including the positions the validity filter then rejects. */
  for (int z = oz - radius; z <= oz + radius; z++) {
    for (int y = floor_y - radius; y <= floor_y + radius; y++) {
      for (int x = ox - radius; x <= ox + radius; x++) {
        if (!(legacy_next_float(r) < probability)) continue;
        if (!t_magma_is_valid(fc, x, y, z)) continue;
        feat_set(fc, x, y, z, B_MAGMA);
        placed++;
      }
    }
  }
  return placed > 0;
}

/* ============================================================ blue_ice =====
 * BlueIceFeature: replace the origin and grow a blob that stays attached to
 * blue ice. */
static int feature_blue_ice(FeatureCtx *fc, Jv *config) {
  (void)config;
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;

  if (oy > SEA - 1) return 0;
  if (feat_get(fc, ox, oy, oz) != B_WATER && feat_get(fc, ox, oy - 1, oz) != B_WATER) return 0;

  int found_packed_ice = 0;
  for (int i = 0; i < 6; i++) {
    int dir = T_ALL_DIRS[i];
    if (dir == T_DIR_DOWN) continue;
    int dx, dy, dz;
    t_dir_vec(dir, &dx, &dy, &dz);
    if (feat_get(fc, ox + dx, oy + dy, oz + dz) == B_PACKED_ICE) {
      found_packed_ice = 1;
      break;
    }
  }
  if (!found_packed_ice) return 0;

  feat_set(fc, ox, oy, oz, B_BLUE_ICE);

  for (int i = 0; i < 200; i++) {
    int y_off = legacy_next_bound(r, 5) - legacy_next_bound(r, 6);
    int xz_diff = 3;
    if (y_off < 2) xz_diff += y_off / 2;
    if (xz_diff >= 1) {
      int px = ox + legacy_next_bound(r, xz_diff) - legacy_next_bound(r, xz_diff);
      int py = oy + y_off;
      int pz = oz + legacy_next_bound(r, xz_diff) - legacy_next_bound(r, xz_diff);
      uint16_t state = feat_get(fc, px, py, pz);
      if (t_is_air(state) || state == B_WATER || state == B_PACKED_ICE || state == B_ICE) {
        for (int d = 0; d < 6; d++) {
          int dx, dy, dz;
          t_dir_vec(T_ALL_DIRS[d], &dx, &dy, &dz);
          if (feat_get(fc, px + dx, py + dy, pz + dz) == B_BLUE_ICE) {
            feat_set(fc, px, py, pz, B_BLUE_ICE);
            break;
          }
        }
      }
    }
  }
  return 1;
}

/* ============================================================= iceberg =====
 * IcebergFeature: an ellipse/round ice hill above sea level plus its inverted
 * underwater half, then an optional cut-out and the smoothing pass. */

static int t_is_iceberg_state(uint16_t b) {
  return b == B_PACKED_ICE || b == B_SNOW_BLOCK || b == B_BLUE_ICE;
}

static int t_height_dependent_radius_ellipse(int y_off, int height, int width) {
  float scale = (1.0F - (float)pow((double)y_off, 2.0) / ((float)height * 1.0F)) * (float)width;
  return t_ceil_f(scale / 2.0F);
}

static int t_height_dependent_radius_round(LegacyRng *r, int y_off, int height, int width) {
  float k = 3.5F - legacy_next_float(r);
  float scale = (1.0F - (float)pow((double)y_off, 2.0) / ((float)height * k)) * (float)width;
  if (height > 15 + legacy_next_bound(r, 5)) {
    int temp_y_off = y_off < 3 + legacy_next_bound(r, 6) ? y_off / 2 : y_off;
    scale = (1.0F - (float)temp_y_off / ((float)height * k * 0.4F)) * (float)width;
  }
  return t_ceil_f(scale / 2.0F);
}

static int t_height_dependent_radius_steep(LegacyRng *r, int y_off, int height, int width) {
  float k = 1.0F + legacy_next_float(r) / 2.0F;
  float scale = (1.0F - (float)y_off / ((float)height * k)) * (float)width;
  return t_ceil_f(scale / 2.0F);
}

static int t_get_ellipse_c(int y_off, int height, int shape_ellipse_c) {
  int c = shape_ellipse_c;
  if (y_off > 0 && height - y_off <= 3) c -= 4 - (height - y_off);
  return c;
}

static double t_signed_distance_ellipse(int xo, int zo, int origin_x, int origin_z, int a, int c, double angle) {
  return pow(((double)(xo - origin_x) * cos(angle) - (double)(zo - origin_z) * sin(angle)) / (double)a, 2.0) +
         pow(((double)(xo - origin_x) * sin(angle) + (double)(zo - origin_z) * cos(angle)) / (double)c, 2.0) -
         1.0;
}

static double t_signed_distance_circle(LegacyRng *r, int xo, int zo, int origin_x, int origin_z, int radius) {
  float off = 10.0F * t_clamp_f(legacy_next_float(r), 0.2F, 0.8F) / (float)radius;
  return (double)off + pow((double)(xo - origin_x), 2.0) + pow((double)(zo - origin_z), 2.0) -
         pow((double)radius, 2.0);
}

static void t_iceberg_remove_floating_snow(FeatureCtx *fc, int x, int y, int z) {
  if (feat_get(fc, x, y + 1, z) == B_SNOW) feat_set(fc, x, y + 1, z, B_AIR);
}

static void t_iceberg_set_block(FeatureCtx *fc, LegacyRng *r, int x, int y, int z, int h_diff, int height,
                                int is_ellipse, int snow_on_top, uint16_t main_state) {
  uint16_t state = feat_get(fc, x, y, z);
  if (t_is_air(state) || state == B_SNOW_BLOCK || state == B_ICE || state == B_WATER) {
    int randomness = !is_ellipse || legacy_next_double(r) > 0.05;
    int divisor = is_ellipse ? 3 : 2;
    if (snow_on_top && state != B_WATER &&
        (double)h_diff <= (double)legacy_next_bound(r, t_max_i(1, height / divisor)) + (double)height * 0.6 &&
        randomness) {
      feat_set(fc, x, y, z, B_SNOW_BLOCK);
    } else {
      feat_set(fc, x, y, z, main_state);
    }
  }
}

static void t_iceberg_generate_block(FeatureCtx *fc, LegacyRng *r, int ox, int oy, int oz, int height, int xo,
                                     int y_off, int zo, int radius, int a, int is_ellipse, int shape_ellipse_c,
                                     double shape_angle, int snow_on_top, uint16_t main_state) {
  double signed_dist = is_ellipse
                           ? t_signed_distance_ellipse(xo, zo, 0, 0, a,
                                                       t_get_ellipse_c(y_off, height, shape_ellipse_c), shape_angle)
                           : t_signed_distance_circle(r, xo, zo, 0, 0, radius);
  if (signed_dist < 0.0) {
    int px = ox + xo, py = oy + y_off, pz = oz + zo;
    double compare_val = is_ellipse ? -0.5 : (double)(-6 - legacy_next_bound(r, 3));
    if (signed_dist > compare_val && legacy_next_double(r) > 0.9) return;
    t_iceberg_set_block(fc, r, px, py, pz, height - y_off, height, is_ellipse, snow_on_top, main_state);
  }
}

static void t_iceberg_carve(FeatureCtx *fc, int radius, int y_off, int ox, int oy, int oz, int under_water,
                            double angle, int local_ox, int local_oz, int shape_ellipse_a, int shape_ellipse_c) {
  int a = radius + 1 + shape_ellipse_a / 3;
  int c = t_min_i(radius - 3, 3) + shape_ellipse_c / 2 - 1;

  for (int xo = -a; xo < a; xo++) {
    for (int zo = -a; zo < a; zo++) {
      double signed_dist = t_signed_distance_ellipse(xo, zo, local_ox, local_oz, a, c, angle);
      if (signed_dist < 0.0) {
        int px = ox + xo, py = oy + y_off, pz = oz + zo;
        uint16_t state = feat_get(fc, px, py, pz);
        if (t_is_iceberg_state(state) || state == B_SNOW) {
          if (under_water) {
            feat_set(fc, px, py, pz, B_WATER);
          } else {
            feat_set(fc, px, py, pz, B_AIR);
            t_iceberg_remove_floating_snow(fc, px, py, pz);
          }
        }
      }
    }
  }
}

static void t_iceberg_generate_cut_out(FeatureCtx *fc, LegacyRng *r, int ox, int oy, int oz, int width, int height,
                                       int is_ellipse, int shape_ellipse_a, double shape_angle,
                                       int shape_ellipse_c) {
  int random_sign_x = legacy_next_bound(r, 2) ? -1 : 1;
  int random_sign_z = legacy_next_bound(r, 2) ? -1 : 1;
  int x_off = legacy_next_bound(r, t_max_i(width / 2 - 2, 1));
  if (legacy_next_bound(r, 2)) x_off = width / 2 + 1 - legacy_next_bound(r, t_max_i(width - width / 2 - 1, 1));
  int z_off = legacy_next_bound(r, t_max_i(width / 2 - 2, 1));
  if (legacy_next_bound(r, 2)) z_off = width / 2 + 1 - legacy_next_bound(r, t_max_i(width - width / 2 - 1, 1));
  if (is_ellipse) {
    int v = legacy_next_bound(r, t_max_i(shape_ellipse_a - 5, 1));
    x_off = v;
    z_off = v;
  }

  int local_ox = random_sign_x * x_off;
  int local_oz = random_sign_z * z_off;
  double angle = is_ellipse ? shape_angle + (M_PI / 2) : legacy_next_double(r) * 2.0 * M_PI;

  for (int y_off = 0; y_off < height - 3; y_off++) {
    int radius = t_height_dependent_radius_round(r, y_off, height, width);
    t_iceberg_carve(fc, radius, y_off, ox, oy, oz, 0, angle, local_ox, local_oz, shape_ellipse_a, shape_ellipse_c);
  }

  for (int y_off = -1; y_off > -height + legacy_next_bound(r, 5); y_off--) {
    int radius = t_height_dependent_radius_steep(r, y_off, height, width);
    t_iceberg_carve(fc, radius, y_off, ox, oy, oz, 1, angle, local_ox, local_oz, shape_ellipse_a, shape_ellipse_c);
  }
}

static void t_iceberg_smooth(FeatureCtx *fc, int ox, int oy, int oz, int width, int height, int is_ellipse,
                             int shape_ellipse_a) {
  int a = is_ellipse ? shape_ellipse_a : width / 2;

  for (int x = -a; x <= a; x++) {
    for (int z = -a; z <= a; z++) {
      for (int y_off = 0; y_off <= height; y_off++) {
        int px = ox + x, py = oy + y_off, pz = oz + z;
        uint16_t state = feat_get(fc, px, py, pz);
        if (t_is_iceberg_state(state) || state == B_SNOW) {
          if (t_is_air(feat_get(fc, px, py - 1, pz))) {
            feat_set(fc, px, py, pz, B_AIR);
            feat_set(fc, px, py + 1, pz, B_AIR);
          } else if (t_is_iceberg_state(state)) {
            int counter = 0;
            if (!t_is_iceberg_state(feat_get(fc, px - 1, py, pz))) counter++;
            if (!t_is_iceberg_state(feat_get(fc, px + 1, py, pz))) counter++;
            if (!t_is_iceberg_state(feat_get(fc, px, py, pz - 1))) counter++;
            if (!t_is_iceberg_state(feat_get(fc, px, py, pz + 1))) counter++;
            if (counter >= 3) feat_set(fc, px, py, pz, B_AIR);
          }
        }
      }
    }
  }
}

static int feature_iceberg(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oz = fc->origin_z;
  int oy = SEA; /* chunkGenerator().getSeaLevel() */
  int id = t_state_id(jget(config, "state"));
  if (id < 0) return 0;

  int snow_on_top = legacy_next_double(r) > 0.7;
  double shape_angle = legacy_next_double(r) * 2.0 * M_PI;
  int shape_ellipse_a = 11 - legacy_next_bound(r, 5);
  int shape_ellipse_c = 3 + legacy_next_bound(r, 3);
  int is_ellipse = legacy_next_double(r) > 0.7;
  int over_water_height = is_ellipse ? legacy_next_bound(r, 6) + 6 : legacy_next_bound(r, 15) + 3;
  if (!is_ellipse && legacy_next_double(r) > 0.9) over_water_height += legacy_next_bound(r, 19) + 7;
  int under_water_height = t_min_i(over_water_height + legacy_next_bound(r, 11), 18);
  int width = t_min_i(over_water_height + legacy_next_bound(r, 7) - legacy_next_bound(r, 5), 11);
  int a = is_ellipse ? shape_ellipse_a : 11;

  for (int xo = -a; xo < a; xo++) {
    for (int zo = -a; zo < a; zo++) {
      for (int y_off = 0; y_off < over_water_height; y_off++) {
        int radius = is_ellipse ? t_height_dependent_radius_ellipse(y_off, over_water_height, width)
                                : t_height_dependent_radius_round(r, y_off, over_water_height, width);
        if (is_ellipse || xo < radius) {
          t_iceberg_generate_block(fc, r, ox, oy, oz, over_water_height, xo, y_off, zo, radius, a, is_ellipse,
                                   shape_ellipse_c, shape_angle, snow_on_top, (uint16_t)id);
        }
      }
    }
  }

  t_iceberg_smooth(fc, ox, oy, oz, width, over_water_height, is_ellipse, shape_ellipse_a);

  for (int xo = -a; xo < a; xo++) {
    for (int zo = -a; zo < a; zo++) {
      for (int y_off = -1; y_off > -under_water_height; y_off--) {
        int new_a = is_ellipse
                        ? t_ceil_f((float)a * (1.0F - (float)pow((double)y_off, 2.0) /
                                                          ((float)under_water_height * 8.0F)))
                        : a;
        int radius = t_height_dependent_radius_steep(r, -y_off, under_water_height, width);
        if (xo < radius) {
          t_iceberg_generate_block(fc, r, ox, oy, oz, under_water_height, xo, y_off, zo, radius, new_a, is_ellipse,
                                   shape_ellipse_c, shape_angle, snow_on_top, (uint16_t)id);
        }
      }
    }
  }

  int do_cut_out = is_ellipse ? legacy_next_double(r) > 0.1 : legacy_next_double(r) > 0.7;
  if (do_cut_out) {
    t_iceberg_generate_cut_out(fc, r, ox, oy, oz, width, over_water_height, is_ellipse, shape_ellipse_a, shape_angle,
                               shape_ellipse_c);
  }
  return 1;
}
/* Fossil structure templates, extracted from the vanilla server jar
 * (data/minecraft/structure/fossil NBT files) - the only worldgen data the
 * embedded JSON does not carry. Each entry packs the template-relative block
 * position as x | y << 4 | z << 8. */
typedef struct {
  uint8_t sx, sy, sz;
  uint16_t first, count;
  const char *block;
} FossTpl;

static const char *FOSS_TEMPLATE_NAMES[8] = {
  "minecraft:fossil/spine_1",
  "minecraft:fossil/spine_2",
  "minecraft:fossil/spine_3",
  "minecraft:fossil/spine_4",
  "minecraft:fossil/skull_1",
  "minecraft:fossil/skull_2",
  "minecraft:fossil/skull_3",
  "minecraft:fossil/skull_4",
};

static const uint16_t FOSS_POS[1141] = {
  0x100, 0x300, 0x500, 0x700, 0x900, 0xb00, 0x102, 0x302, 0x502, 0x702,
  0x902, 0xb02, 0x110, 0x310, 0x510, 0x710, 0x910, 0xb10, 0x112, 0x312,
  0x512, 0x712, 0x912, 0xb12, 0x021, 0x121, 0x221, 0x321, 0x421, 0x521,
  0x621, 0x721, 0x821, 0x921, 0xa21, 0xb21, 0xc21, 0x101, 0x301, 0x501,
  0x701, 0x901, 0xb01, 0x103, 0x303, 0x503, 0x703, 0x903, 0xb03, 0x110,
  0x310, 0x510, 0x710, 0x910, 0xb10, 0x114, 0x314, 0x514, 0x714, 0x914,
  0xb14, 0x120, 0x320, 0x520, 0x720, 0x920, 0xb20, 0x124, 0x324, 0x524,
  0x724, 0x924, 0xb24, 0x131, 0x331, 0x531, 0x731, 0x931, 0xb31, 0x032,
  0x132, 0x232, 0x332, 0x432, 0x532, 0x632, 0x732, 0x832, 0x932, 0xa32,
  0xb32, 0xc32, 0x133, 0x333, 0x533, 0x733, 0x933, 0xb33, 0x100, 0x300,
  0x500, 0x700, 0x900, 0xb00, 0x101, 0x301, 0x501, 0x701, 0x901, 0xb01,
  0x105, 0x305, 0x505, 0x705, 0x905, 0xb05, 0x106, 0x306, 0x506, 0x706,
  0x906, 0xb06, 0x110, 0x310, 0x510, 0x710, 0x910, 0xb10, 0x116, 0x316,
  0x516, 0x716, 0x916, 0xb16, 0x120, 0x320, 0x520, 0x720, 0x920, 0xb20,
  0x126, 0x326, 0x526, 0x726, 0x926, 0xb26, 0x130, 0x330, 0x530, 0x730,
  0x930, 0xb30, 0x131, 0x331, 0x531, 0x731, 0x931, 0xb31, 0x132, 0x332,
  0x532, 0x732, 0x932, 0xb32, 0x033, 0x133, 0x233, 0x333, 0x433, 0x533,
  0x633, 0x733, 0x833, 0x933, 0xa33, 0xb33, 0xc33, 0x134, 0x334, 0x534,
  0x734, 0x934, 0xb34, 0x135, 0x335, 0x535, 0x735, 0x935, 0xb35, 0x136,
  0x336, 0x536, 0x736, 0x936, 0xb36, 0x101, 0x301, 0x501, 0x701, 0x901,
  0xb01, 0x102, 0x302, 0x502, 0x702, 0x902, 0xb02, 0x106, 0x306, 0x506,
  0x706, 0x906, 0xb06, 0x107, 0x307, 0x507, 0x707, 0x907, 0xb07, 0x110,
  0x310, 0x510, 0x710, 0x910, 0xb10, 0x118, 0x318, 0x518, 0x718, 0x918,
  0xb18, 0x120, 0x320, 0x520, 0x720, 0x920, 0xb20, 0x128, 0x328, 0x528,
  0x728, 0x928, 0xb28, 0x130, 0x330, 0x530, 0x730, 0x930, 0xb30, 0x138,
  0x338, 0x538, 0x738, 0x938, 0xb38, 0x140, 0x340, 0x540, 0x740, 0x940,
  0xb40, 0x141, 0x341, 0x541, 0x741, 0x941, 0xb41, 0x142, 0x342, 0x542,
  0x742, 0x942, 0xb42, 0x143, 0x343, 0x543, 0x743, 0x943, 0xb43, 0x044,
  0x144, 0x244, 0x344, 0x444, 0x544, 0x644, 0x744, 0x844, 0x944, 0xa44,
  0xb44, 0xc44, 0x145, 0x345, 0x545, 0x745, 0x945, 0xb45, 0x146, 0x346,
  0x546, 0x746, 0x946, 0xb46, 0x147, 0x347, 0x547, 0x747, 0x947, 0xb47,
  0x148, 0x348, 0x548, 0x748, 0x948, 0xb48, 0x101, 0x201, 0x301, 0x401,
  0x501, 0x102, 0x202, 0x302, 0x103, 0x203, 0x303, 0x104, 0x204, 0x304,
  0x404, 0x504, 0x110, 0x210, 0x310, 0x410, 0x510, 0x011, 0x611, 0x612,
  0x613, 0x014, 0x614, 0x115, 0x215, 0x315, 0x415, 0x515, 0x120, 0x220,
  0x320, 0x420, 0x520, 0x021, 0x621, 0x022, 0x622, 0x023, 0x623, 0x024,
  0x624, 0x125, 0x225, 0x325, 0x425, 0x525, 0x130, 0x230, 0x330, 0x430,
  0x530, 0x631, 0x032, 0x632, 0x033, 0x633, 0x634, 0x135, 0x235, 0x335,
  0x435, 0x535, 0x141, 0x241, 0x341, 0x441, 0x541, 0x142, 0x242, 0x342,
  0x442, 0x542, 0x143, 0x243, 0x343, 0x443, 0x543, 0x144, 0x244, 0x344,
  0x444, 0x544, 0x101, 0x201, 0x301, 0x102, 0x202, 0x302, 0x103, 0x203,
  0x303, 0x104, 0x204, 0x304, 0x105, 0x205, 0x305, 0x110, 0x210, 0x310,
  0x011, 0x411, 0x412, 0x413, 0x414, 0x015, 0x415, 0x116, 0x216, 0x316,
  0x120, 0x220, 0x320, 0x021, 0x421, 0x022, 0x422, 0x023, 0x423, 0x024,
  0x424, 0x025, 0x425, 0x126, 0x226, 0x326, 0x130, 0x230, 0x330, 0x031,
  0x431, 0x432, 0x033, 0x433, 0x434, 0x035, 0x435, 0x136, 0x236, 0x336,
  0x141, 0x241, 0x341, 0x042, 0x142, 0x242, 0x342, 0x143, 0x243, 0x343,
  0x044, 0x144, 0x244, 0x344, 0x145, 0x245, 0x345, 0x000, 0x101, 0x201,
  0x301, 0x401, 0x102, 0x202, 0x302, 0x402, 0x103, 0x203, 0x303, 0x403,
  0x004, 0x010, 0x110, 0x210, 0x310, 0x410, 0x011, 0x411, 0x012, 0x412,
  0x013, 0x413, 0x014, 0x114, 0x214, 0x314, 0x414, 0x020, 0x120, 0x220,
  0x320, 0x420, 0x421, 0x022, 0x422, 0x423, 0x024, 0x124, 0x224, 0x324,
  0x424, 0x030, 0x031, 0x131, 0x231, 0x331, 0x032, 0x132, 0x232, 0x332,
  0x033, 0x133, 0x233, 0x333, 0x034, 0x000, 0x101, 0x201, 0x102, 0x202,
  0x003, 0x010, 0x110, 0x210, 0x011, 0x311, 0x012, 0x312, 0x013, 0x113,
  0x213, 0x020, 0x120, 0x220, 0x321, 0x322, 0x023, 0x123, 0x223, 0x030,
  0x031, 0x131, 0x231, 0x032, 0x132, 0x232, 0x033, 0x100, 0x300, 0x500,
  0x700, 0x900, 0xb00, 0x102, 0x302, 0x502, 0x702, 0x902, 0xb02, 0x110,
  0x310, 0x510, 0x710, 0x910, 0xb10, 0x112, 0x312, 0x512, 0x712, 0x912,
  0xb12, 0x021, 0x121, 0x221, 0x321, 0x421, 0x521, 0x621, 0x721, 0x821,
  0x921, 0xa21, 0xb21, 0xc21, 0x101, 0x301, 0x501, 0x701, 0x901, 0xb01,
  0x103, 0x303, 0x503, 0x703, 0x903, 0xb03, 0x110, 0x310, 0x510, 0x710,
  0x910, 0xb10, 0x114, 0x314, 0x514, 0x714, 0x914, 0xb14, 0x120, 0x320,
  0x520, 0x720, 0x920, 0xb20, 0x124, 0x324, 0x524, 0x724, 0x924, 0xb24,
  0x131, 0x331, 0x531, 0x731, 0x931, 0xb31, 0x032, 0x132, 0x232, 0x332,
  0x432, 0x532, 0x632, 0x732, 0x832, 0x932, 0xa32, 0xb32, 0xc32, 0x133,
  0x333, 0x533, 0x733, 0x933, 0xb33, 0x100, 0x300, 0x500, 0x700, 0x900,
  0xb00, 0x101, 0x301, 0x501, 0x701, 0x901, 0xb01, 0x102, 0x302, 0x502,
  0x702, 0x902, 0xb02, 0x104, 0x105, 0x305, 0x505, 0x705, 0x905, 0xb05,
  0x106, 0x306, 0x506, 0x706, 0x906, 0xb06, 0x110, 0x310, 0x510, 0x710,
  0x910, 0xb10, 0x116, 0x316, 0x516, 0x716, 0x916, 0xb16, 0x120, 0x320,
  0x520, 0x720, 0x920, 0xb20, 0x126, 0x326, 0x526, 0x726, 0x926, 0xb26,
  0x130, 0x330, 0x530, 0x730, 0x930, 0xb30, 0x131, 0x331, 0x531, 0x731,
  0x931, 0xb31, 0x132, 0x332, 0x532, 0x732, 0x932, 0xb32, 0x033, 0x133,
  0x233, 0x333, 0x433, 0x533, 0x633, 0x733, 0x833, 0x933, 0xa33, 0xb33,
  0xc33, 0x134, 0x334, 0x534, 0x734, 0x934, 0xb34, 0x135, 0x335, 0x535,
  0x735, 0x935, 0xb35, 0x136, 0x336, 0x536, 0x736, 0x936, 0xb36, 0x101,
  0x301, 0x501, 0x701, 0x901, 0xb01, 0x102, 0x302, 0x502, 0x702, 0x902,
  0xb02, 0x106, 0x306, 0x506, 0x706, 0x906, 0xb06, 0x107, 0x307, 0x507,
  0x707, 0x907, 0xb07, 0x110, 0x310, 0x510, 0x710, 0x910, 0xb10, 0x118,
  0x318, 0x518, 0x718, 0x918, 0xb18, 0x120, 0x320, 0x520, 0x720, 0x920,
  0xb20, 0x128, 0x328, 0x528, 0x728, 0x928, 0xb28, 0x130, 0x330, 0x530,
  0x730, 0x930, 0xb30, 0x138, 0x338, 0x538, 0x738, 0x938, 0xb38, 0x140,
  0x340, 0x540, 0x740, 0x940, 0xb40, 0x141, 0x341, 0x541, 0x741, 0x941,
  0xb41, 0x142, 0x342, 0x542, 0x742, 0x942, 0xb42, 0x143, 0x343, 0x543,
  0x743, 0x943, 0xb43, 0x044, 0x144, 0x244, 0x344, 0x444, 0x544, 0x644,
  0x744, 0x844, 0x944, 0xa44, 0xb44, 0xc44, 0x145, 0x345, 0x545, 0x745,
  0x945, 0xb45, 0x146, 0x346, 0x546, 0x746, 0x946, 0xb46, 0x147, 0x347,
  0x547, 0x747, 0x947, 0xb47, 0x148, 0x348, 0x548, 0x748, 0x948, 0xb48,
  0x101, 0x201, 0x301, 0x401, 0x501, 0x102, 0x202, 0x302, 0x103, 0x203,
  0x303, 0x104, 0x204, 0x304, 0x404, 0x504, 0x110, 0x210, 0x310, 0x410,
  0x510, 0x011, 0x611, 0x612, 0x613, 0x014, 0x614, 0x115, 0x215, 0x315,
  0x415, 0x515, 0x120, 0x220, 0x320, 0x420, 0x520, 0x021, 0x621, 0x022,
  0x622, 0x023, 0x623, 0x024, 0x624, 0x125, 0x225, 0x325, 0x425, 0x525,
  0x130, 0x230, 0x330, 0x430, 0x530, 0x631, 0x032, 0x632, 0x033, 0x633,
  0x634, 0x135, 0x235, 0x335, 0x435, 0x535, 0x141, 0x241, 0x341, 0x441,
  0x541, 0x142, 0x242, 0x342, 0x442, 0x542, 0x143, 0x243, 0x343, 0x443,
  0x543, 0x144, 0x244, 0x344, 0x444, 0x544, 0x101, 0x201, 0x301, 0x102,
  0x202, 0x302, 0x103, 0x203, 0x303, 0x104, 0x204, 0x304, 0x105, 0x205,
  0x305, 0x110, 0x210, 0x310, 0x011, 0x411, 0x412, 0x413, 0x414, 0x015,
  0x415, 0x116, 0x216, 0x316, 0x120, 0x220, 0x320, 0x021, 0x421, 0x022,
  0x422, 0x023, 0x423, 0x024, 0x424, 0x025, 0x425, 0x126, 0x226, 0x326,
  0x130, 0x230, 0x330, 0x031, 0x431, 0x432, 0x033, 0x433, 0x434, 0x035,
  0x435, 0x136, 0x236, 0x336, 0x141, 0x241, 0x341, 0x042, 0x142, 0x242,
  0x342, 0x143, 0x243, 0x343, 0x044, 0x144, 0x244, 0x344, 0x145, 0x245,
  0x345, 0x000, 0x101, 0x201, 0x301, 0x401, 0x102, 0x202, 0x302, 0x402,
  0x103, 0x203, 0x303, 0x403, 0x004, 0x010, 0x110, 0x210, 0x310, 0x410,
  0x011, 0x411, 0x012, 0x412, 0x013, 0x413, 0x014, 0x114, 0x214, 0x314,
  0x414, 0x020, 0x120, 0x220, 0x320, 0x420, 0x421, 0x022, 0x422, 0x423,
  0x024, 0x124, 0x224, 0x324, 0x424, 0x030, 0x031, 0x131, 0x231, 0x331,
  0x032, 0x132, 0x232, 0x332, 0x033, 0x133, 0x233, 0x333, 0x034, 0x000,
  0x101, 0x201, 0x102, 0x202, 0x003, 0x010, 0x110, 0x210, 0x011, 0x311,
  0x012, 0x312, 0x013, 0x113, 0x213, 0x020, 0x120, 0x220, 0x321, 0x322,
  0x023, 0x123, 0x223, 0x030, 0x031, 0x131, 0x231, 0x032, 0x132, 0x232,
  0x033,
};

static const FossTpl FOSS_BASE[8] = {
  {3, 3, 13, 0, 37, "minecraft:bone_block"},
  {5, 4, 13, 37, 61, "minecraft:bone_block"},
  {7, 4, 13, 98, 97, "minecraft:bone_block"},
  {9, 5, 13, 195, 121, "minecraft:bone_block"},
  {6, 5, 7, 316, 86, "minecraft:bone_block"},
  {7, 5, 5, 402, 75, "minecraft:bone_block"},
  {5, 4, 5, 477, 58, "minecraft:bone_block"},
  {4, 4, 4, 535, 32, "minecraft:bone_block"},
};

static const FossTpl FOSS_OVERLAY[8] = {
  {3, 3, 13, 567, 37, "minecraft:coal_ore"},
  {5, 4, 13, 604, 61, "minecraft:coal_ore"},
  {7, 4, 13, 665, 104, "minecraft:coal_ore"},
  {9, 5, 13, 769, 121, "minecraft:coal_ore"},
  {6, 5, 7, 890, 86, "minecraft:coal_ore"},
  {7, 5, 5, 976, 75, "minecraft:coal_ore"},
  {5, 4, 5, 1051, 58, "minecraft:coal_ore"},
  {4, 4, 4, 1109, 32, "minecraft:coal_ore"},
};


/* ============================================================== geode ======
 * GeodeFeature: the layer shells around `numPoints` random points, the crack,
 * the budding/crystal placements. The noise term uses a WorldgenRandom over a
 * LegacyRandomSource(level seed) exactly as vanilla does. */

static int t_provider_state(FeatureCtx *fc, Jv *provider, int x, int y, int z, uint16_t *out) {
  return feat_state_provider(fc, provider, x, y, z, out);
}

static int feature_geode(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  Jv *blocks = jget(config, "blocks");
  Jv *layers = jget(config, "layers");
  Jv *crack = jget(config, "crack");
  int min_gen_offset = (int)jnum(jget(config, "min_gen_offset"), -16);
  int max_gen_offset = (int)jnum(jget(config, "max_gen_offset"), 16);
  Jv *outer_wall_distance = jget(config, "outer_wall_distance");
  Jv *point_offset = jget(config, "point_offset");
  Jv *cannot_replace = jget(blocks, "cannot_replace");
  Jv *invalid_blocks = jget(blocks, "invalid_blocks");
  Jv *inner_placements = jget(blocks, "inner_placements");
  /* The 26.2 amethyst_geode JSON omits distribution_points and point_offset; the
   * GeodeConfiguration codec supplies UniformInt.of(3, 4) and UniformInt.of(1, 2),
   * and the sample() draw still happens, so the RNG stream must still advance. */
  Jv *distribution_points = jget(config, "distribution_points");
  int num_points = distribution_points ? feat_int_provider(distribution_points, r)
                                       : 3 + legacy_next_bound(r, 2);
  if (num_points <= 0) return 0;

  /* WorldgenRandom random1 = new WorldgenRandom(new LegacyRandomSource(level.getSeed()));
     NormalNoise noise = NormalNoise.create(random1, -4, 1.0); */
  static const double noise_amps[1] = {1.0};
  LegacyRng noise_rng;
  memset(&noise_rng, 0, sizeof noise_rng); /* LEGACY_RNG_LCG */
  legacy_set_seed(&noise_rng, fc->g->world_seed);
  NormalNoise *noise = normal_noise_create_legacy(&noise_rng, -4, noise_amps, 1);

  double crack_size_adjustment = (double)num_points / (double)t_int_provider_max(outer_wall_distance);
  double filling = jnum(jget(layers, "filling"), 1.7);
  double inner_layer = jnum(jget(layers, "inner_layer"), 2.2);
  double middle_layer = jnum(jget(layers, "middle_layer"), 3.2);
  double outer_layer = jnum(jget(layers, "outer_layer"), 4.2);
  double base_crack_size = jnum(jget(crack, "base_crack_size"), 2.0);
  int crack_point_offset = (int)jnum(jget(crack, "crack_point_offset"), 2);
  double generate_crack_chance = jnum(jget(crack, "generate_crack_chance"), 1.0);
  double use_potential_placements_chance = jnum(jget(config, "use_potential_placements_chance"), 0.35);
  double use_alternate_layer0_chance = jnum(jget(config, "use_alternate_layer0_chance"), 0.0);
  int placements_require_layer0_alternate =
      jbool(jget(config, "placements_require_layer0_alternate"), 1);
  double noise_multiplier = jnum(jget(config, "noise_multiplier"), 0.05);
  int invalid_blocks_threshold = (int)jnum(jget(config, "invalid_blocks_threshold"), 0);

  double inner_air = 1.0 / sqrt(filling);
  double innermost_block_layer = 1.0 / sqrt(inner_layer + crack_size_adjustment);
  double inner_crust = 1.0 / sqrt(middle_layer + crack_size_adjustment);
  double outer_crust = 1.0 / sqrt(outer_layer + crack_size_adjustment);
  double crack_size = 1.0 / sqrt(base_crack_size + legacy_next_double(r) / 2.0 +
                                 (num_points > 3 ? crack_size_adjustment : 0.0));
  int should_generate_crack = legacy_next_float(r) < (float)generate_crack_chance;
  int num_invalid_points = 0;

  int px[21], py[21], pz[21], poff[21];
  int npoints = 0;
  if (num_points > 21) num_points = 21;
  for (int i = 0; i < num_points; i++) {
    int x = feat_int_provider(outer_wall_distance, r);
    int y = feat_int_provider(outer_wall_distance, r);
    int z = feat_int_provider(outer_wall_distance, r);
    int ax = ox + x, ay = oy + y, az = oz + z;
    uint16_t state = feat_get(fc, ax, ay, az);
    if (t_is_air(state) || t_holder_set_contains(invalid_blocks, state)) {
      if (++num_invalid_points > invalid_blocks_threshold) {
        free(noise);
        return 0;
      }
    }
    px[npoints] = ax;
    py[npoints] = ay;
    pz[npoints] = az;
    poff[npoints] = point_offset ? feat_int_provider(point_offset, r) : 1 + legacy_next_bound(r, 2);
    npoints++;
  }

  int cx[3], cy[3], cz[3];
  int ncrack = 0;
  if (should_generate_crack) {
    int offset_index = legacy_next_bound(r, 4);
    int crack_offset = num_points * 2 + 1;
    if (offset_index == 0) {
      cx[0] = ox + crack_offset; cy[0] = oy + 7; cz[0] = oz;
      cx[1] = ox + crack_offset; cy[1] = oy + 5; cz[1] = oz;
      cx[2] = ox + crack_offset; cy[2] = oy + 1; cz[2] = oz;
    } else if (offset_index == 1) {
      cx[0] = ox; cy[0] = oy + 7; cz[0] = oz + crack_offset;
      cx[1] = ox; cy[1] = oy + 5; cz[1] = oz + crack_offset;
      cx[2] = ox; cy[2] = oy + 1; cz[2] = oz + crack_offset;
    } else if (offset_index == 2) {
      cx[0] = ox + crack_offset; cy[0] = oy + 7; cz[0] = oz + crack_offset;
      cx[1] = ox + crack_offset; cy[1] = oy + 5; cz[1] = oz + crack_offset;
      cx[2] = ox + crack_offset; cy[2] = oy + 1; cz[2] = oz + crack_offset;
    } else {
      cx[0] = ox; cy[0] = oy + 7; cz[0] = oz;
      cx[1] = ox; cy[1] = oy + 5; cz[1] = oz;
      cx[2] = ox; cy[2] = oy + 1; cz[2] = oz;
    }
    ncrack = 3;
  }

  int crystal_x[1024], crystal_y[1024], crystal_z[1024];
  int ncrystals = 0;

  /* BlockPos.betweenClosed(origin + min, origin + max): x fastest, then y, then z. */
  for (int z = oz + min_gen_offset; z <= oz + max_gen_offset; z++) {
    for (int y = oy + min_gen_offset; y <= oy + max_gen_offset; y++) {
      for (int x = ox + min_gen_offset; x <= ox + max_gen_offset; x++) {
        double noise_offset = normal_noise_get(noise, (double)x, (double)y, (double)z) * noise_multiplier;
        double dist_sum_shell = 0.0;
        double dist_sum_crack = 0.0;

        for (int i = 0; i < npoints; i++) {
          dist_sum_shell += t_inv_sqrt(t_dist_sqr(x, y, z, px[i], py[i], pz[i]) + (double)poff[i]) + noise_offset;
        }
        for (int i = 0; i < ncrack; i++) {
          dist_sum_crack += t_inv_sqrt(t_dist_sqr(x, y, z, cx[i], cy[i], cz[i]) + (double)crack_point_offset) + noise_offset;
        }

        if (dist_sum_shell < outer_crust) continue;

        uint16_t here = feat_get(fc, x, y, z);
        int can_replace = !t_holder_set_contains(cannot_replace, here);

        if (should_generate_crack && dist_sum_crack >= crack_size && dist_sum_shell < inner_air) {
          if (can_replace) feat_set(fc, x, y, z, B_AIR);
          /* The adjacent fluid scheduleTick calls are post-processing. */
        } else if (dist_sum_shell >= inner_air) {
          uint16_t state = 0;
          if (t_provider_state(fc, jget(blocks, "filling_provider"), x, y, z, &state) && can_replace) {
            feat_set(fc, x, y, z, state);
          }
        } else if (dist_sum_shell >= innermost_block_layer) {
          int use_alternate_layer = legacy_next_float(r) < (float)use_alternate_layer0_chance;
          uint16_t state = 0;
          if (use_alternate_layer) {
            if (t_provider_state(fc, jget(blocks, "alternate_inner_layer_provider"), x, y, z, &state) && can_replace) {
              feat_set(fc, x, y, z, state);
            }
          } else {
            if (t_provider_state(fc, jget(blocks, "inner_layer_provider"), x, y, z, &state) && can_replace) {
              feat_set(fc, x, y, z, state);
            }
          }

          if ((!placements_require_layer0_alternate || use_alternate_layer) &&
              legacy_next_float(r) < (float)use_potential_placements_chance) {
            if (ncrystals < 1024) {
              crystal_x[ncrystals] = x;
              crystal_y[ncrystals] = y;
              crystal_z[ncrystals] = z;
              ncrystals++;
            }
          }
        } else if (dist_sum_shell >= inner_crust) {
          uint16_t state = 0;
          if (t_provider_state(fc, jget(blocks, "middle_layer_provider"), x, y, z, &state) && can_replace) {
            feat_set(fc, x, y, z, state);
          }
        } else if (dist_sum_shell >= outer_crust) {
          uint16_t state = 0;
          if (t_provider_state(fc, jget(blocks, "outer_layer_provider"), x, y, z, &state) && can_replace) {
            feat_set(fc, x, y, z, state);
          }
        }
      }
    }
  }

  int n_inner = jarr_len(inner_placements);
  for (int c = 0; c < ncrystals; c++) {
    if (n_inner <= 0) break;
    int crystal_block = t_state_id(t_get_random_jv(inner_placements, r));
    if (crystal_block < 0) continue;
    for (int d = 0; d < 6; d++) {
      int dir = T_ALL_DIRS[d];
      int dx, dy, dz;
      t_dir_vec(dir, &dx, &dy, &dz);
      int ax = crystal_x[c] + dx, ay = crystal_y[c] + dy, az = crystal_z[c] + dz;
      uint16_t place_state = feat_get(fc, ax, ay, az);
      /* canClusterGrowAtState: isAir() || (is(WATER) && fluidState.isFull()) */
      if (t_is_air(place_state) || place_state == B_WATER) {
        uint16_t here = feat_get(fc, ax, ay, az);
        if (!t_holder_set_contains(cannot_replace, here)) feat_set(fc, ax, ay, az, (uint16_t)crystal_block);
        break;
      }
    }
  }

  free(noise);
  return 1;
}

/* ====================================================== speleothem utils ===
 * SpeleothemUtils: the shared height curve and the base/to-tip column builder.
 * Pointed-dripstone thickness/direction and the waterlogged property are
 * property-level and therefore not representable in Cinder's palette. */

static double t_speleothem_height(double xz_distance, double radius, double scale, double bluntness) {
  if (xz_distance < bluntness) xz_distance = bluntness;
  double cutoff = 0.384;
  double rr = xz_distance / radius * cutoff;
  double part1 = 0.75 * pow(rr, 1.3333333333333333);
  double part2 = pow(rr, 0.6666666666666666);
  double part3 = 0.3333333333333333 * log(rr);
  double height_relative = scale * (part1 - part2 - part3);
  height_relative = t_max_d(height_relative, 0.0);
  return height_relative / cutoff * radius;
}

/* SpeleothemUtils.placeBaseBlockIfPossible */
static int t_place_base_block_if_possible(FeatureCtx *fc, int x, int y, int z, uint16_t base_block,
                                          Jv *replaceable) {
  uint16_t state = feat_get(fc, x, y, z);
  if (t_holder_set_contains(replaceable, state)) {
    feat_set(fc, x, y, z, base_block);
    return 1;
  }
  return 0;
}

/* SpeleothemUtils.buildBaseToTipColumn, name level: `totalLength` copies of the
 * pointed block stepping along tipDir (vanilla's BASE/MIDDLE/FRUSTUM/TIP
 * thickness values are one block in Cinder). */
static void t_grow_speleothem(FeatureCtx *fc, int x, int y, int z, int tip_dir, int height, int merged_tip,
                              uint16_t base_block, uint16_t pointed_block, Jv *replaceable) {
  int dx, dy, dz;
  /* merged_tip selects the TIP_MERGE thickness value, a property Cinder does
     not track; the draw that produces it already happened in the caller. */
  (void)merged_tip;
  t_dir_vec(tip_dir, &dx, &dy, &dz);
  if (!t_is_base(feat_get(fc, x - dx, y - dy, z - dz), base_block, replaceable)) return;
  for (int i = 0; i < height; i++) {
    feat_set(fc, x, y, z, pointed_block);
    x += dx;
    y += dy;
    z += dz;
  }
}

/* ==================================================== speleothem_cluster ===
 * SpeleothemClusterFeature: per-column stalactite/stalagmite pairs inside the
 * floor/ceiling search range. */

static int t_can_be_adjacent_to_water(FeatureCtx *fc, int x, int y, int z) {
  uint16_t state = feat_get(fc, x, y, z);
  return block_tag_contains("minecraft:base_stone_overworld", state) || t_water_fluid(state);
}

static int t_can_place_pool(FeatureCtx *fc, int x, int y, int z, uint16_t base_block, uint16_t pointed_block) {
  uint16_t state = feat_get(fc, x, y, z);
  if (state == B_WATER || state == base_block || state == pointed_block) return 0;
  /* getFluidState().is(FluidTags.WATER) on the block above */
  if (t_water_fluid(feat_get(fc, x, y + 1, z))) return 0;
  for (int i = 0; i < 4; i++) {
    int dx, dy, dz;
    t_dir_vec(T_HORIZONTAL[i], &dx, &dy, &dz);
    if (!t_can_be_adjacent_to_water(fc, x + dx, y, z + dz)) return 0;
  }
  return t_can_be_adjacent_to_water(fc, x, y - 1, z);
}

static void t_replace_blocks_with_base_blocks(FeatureCtx *fc, int x, int y, int z, int max_count, int dir,
                                              uint16_t base_block, Jv *replaceable) {
  int dx, dy, dz;
  t_dir_vec(dir, &dx, &dy, &dz);
  for (int i = 0; i < max_count; i++) {
    if (!t_place_base_block_if_possible(fc, x, y, z, base_block, replaceable)) return;
    x += dx;
    y += dy;
    z += dz;
  }
}

static int t_speleothem_height_sampled(LegacyRng *r, int dx, int dz, float density, int max_height,
                                       Jv *config) {
  if (legacy_next_float(r) > density) return 0;
  int distance_from_center = t_abs_i(dx) + t_abs_i(dz);
  float height_mean = (float)t_clamped_map_d((double)distance_from_center, 0.0,
                                             (double)(int)jnum(jget(config, "max_distance_from_center_affecting_height_bias"), 1),
                                             (double)max_height / 2.0, 0.0);
  float deviation = (float)jnum(jget(config, "height_deviation"), 1);
  float sampled = t_normal(r, height_mean, deviation);
  sampled = t_clamp_f(sampled, 0.0F, (float)max_height);
  return (int)sampled;
}

static void t_speleothem_place_column(FeatureCtx *fc, int x, int y, int z, int dx, int dz, float chance_of_water,
                                      double chance_of_speleothem, int cluster_height, float density,
                                      Jv *config, uint16_t base_block, uint16_t pointed_block,
                                      Jv *replaceable) {
  LegacyRng *r = feat_rng(fc);
  int search_range = (int)jnum(jget(config, "floor_to_ceiling_search_range"), 30) /* codec default */;
  TColumn base;
  if (!t_column_scan_full(fc, x, y, z, search_range, t_pred_empty_or_water, 0,
                          t_pred_neither_empty_nor_water, 0, &base)) {
    return;
  }
  if (!base.has_ceiling && !base.has_floor) return;

  int ceiling_y = base.ceiling_y, has_ceiling = base.has_ceiling;
  int has_floor = base.has_floor, floor_y = base.floor_y;

  int want_pool = legacy_next_float(r) < chance_of_water;
  int use_floor = has_floor;
  int use_floor_y = floor_y;
  if (want_pool && has_floor &&
      t_can_place_pool(fc, x, floor_y, z, base_block, pointed_block)) {
    /* column = baseColumn.withFloor(OptionalInt.of(baseFloorY - 1)) */
    use_floor_y = floor_y - 1;
    feat_set(fc, x, floor_y, z, B_WATER);
  }

  int want_stalactite = legacy_next_double(r) < chance_of_speleothem;
  int stalactite_height = 0;
  if (has_ceiling && want_stalactite && feat_get(fc, x, ceiling_y, z) != B_LAVA) {
    int thickness = feat_int_provider(jget(config, "speleothem_block_layer_thickness"), r);
    t_replace_blocks_with_base_blocks(fc, x, ceiling_y, z, thickness, T_DIR_UP, base_block, replaceable);
    int max_height_for_column = use_floor ? t_min_i(cluster_height, ceiling_y - use_floor_y) : cluster_height;
    stalactite_height = t_speleothem_height_sampled(r, dx, dz, density, max_height_for_column, config);
  }

  int want_stalagmite = legacy_next_double(r) < chance_of_speleothem;
  int stalagmite_height = 0;
  if (use_floor && want_stalagmite && feat_get(fc, x, use_floor_y, z) != B_LAVA) {
    int thickness = feat_int_provider(jget(config, "speleothem_block_layer_thickness"), r);
    t_replace_blocks_with_base_blocks(fc, x, use_floor_y, z, thickness, T_DIR_DOWN, base_block, replaceable);
    if (has_ceiling) {
      int max_diff = (int)jnum(jget(config, "max_stalagmite_stalactite_height_diff"), 1);
      stalagmite_height = t_max_i(0, stalactite_height + t_random_between_i(r, -max_diff, max_diff));
    } else {
      stalagmite_height = t_speleothem_height_sampled(r, dx, dz, density, cluster_height, config);
    }
  }

  int actual_stalagmite_height = stalagmite_height;
  int actual_stalactite_height = stalactite_height;
  if (has_ceiling && use_floor && ceiling_y - stalactite_height <= use_floor_y + stalagmite_height) {
    int lowest_stalactite_bottom = t_max_i(ceiling_y - stalactite_height, use_floor_y + 1);
    int highest_stalagmite_top = t_min_i(use_floor_y + stalagmite_height, ceiling_y - 1);
    int actual_stalactite_bottom = t_random_between_i(r, lowest_stalactite_bottom, highest_stalagmite_top + 1);
    int actual_stalagmite_top = actual_stalactite_bottom - 1;
    actual_stalactite_height = ceiling_y - actual_stalactite_bottom;
    actual_stalagmite_height = actual_stalagmite_top - use_floor_y;
  }

  /* column.getHeight() is present only for a Range (both ends present) */
  int column_height = (has_ceiling && use_floor) ? (use_floor_y - ceiling_y + 1) : 0;
  int has_column_height = has_ceiling && use_floor;
  int merge_tips = legacy_next_bound(r, 2) != 0 && actual_stalactite_height > 0 &&
                   actual_stalagmite_height > 0 && has_column_height &&
                   actual_stalactite_height + actual_stalagmite_height == column_height;

  if (has_ceiling) {
    t_grow_speleothem(fc, x, ceiling_y - 1, z, T_DIR_DOWN, actual_stalactite_height, merge_tips, base_block,
                      pointed_block, replaceable);
  }
  if (use_floor) {
    t_grow_speleothem(fc, x, use_floor_y + 1, z, T_DIR_UP, actual_stalagmite_height, merge_tips, base_block,
                      pointed_block, replaceable);
  }
}

static int feature_speleothem_cluster(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  int base_block = t_state_id(jget(config, "base_block"));
  int pointed_block = t_state_id(jget(config, "pointed_block"));
  Jv *replaceable = jget(config, "replaceable_blocks");
  if (base_block < 0 || pointed_block < 0) return 0;

  if (!t_is_empty_or_water(feat_get(fc, ox, oy, oz))) return 0;

  int height = feat_int_provider(jget(config, "height"), r);
  float wetness = (float)feat_float_provider(jget(config, "wetness"), r);
  float density = (float)feat_float_provider(jget(config, "density"), r);
  int x_radius = feat_int_provider(jget(config, "radius"), r);
  int z_radius = feat_int_provider(jget(config, "radius"), r);

  double chance_at_max =
      jnum(jget(config, "chance_of_speleothem_at_max_distance_from_center"), 0.1);
  int max_from_edge = (int)jnum(jget(config, "max_distance_from_edge_affecting_chance_of_speleothem"), 1);

  for (int dx = -x_radius; dx <= x_radius; dx++) {
    for (int dz = -z_radius; dz <= z_radius; dz++) {
      int x_distance_from_edge = x_radius - t_abs_i(dx);
      int z_distance_from_edge = z_radius - t_abs_i(dz);
      int distance_from_edge = t_min_i(x_distance_from_edge, z_distance_from_edge);
      double chance = (double)t_clamped_map_f((float)distance_from_edge, 0.0F, (float)max_from_edge,
                                              (float)chance_at_max, 1.0F);
      t_speleothem_place_column(fc, ox + dx, oy, oz + dz, dx, dz, wetness, chance, height, density, config,
                                (uint16_t)base_block, (uint16_t)pointed_block, replaceable);
    }
  }
  return 1;
}

/* ====================================================== large_dripstone ====
 * LargeDripstoneFeature + SpeleothemUtils.isCircleMostlyEmbeddedInStone and the
 * wind offsetter. */

static int t_large_dripstone_get_height_at_radius(double bluntness, int radius, double scale, float check_radius) {
  return (int)t_speleothem_height((double)check_radius, (double)radius, scale, bluntness);
}

static int t_circle_mostly_embedded_in_stone(FeatureCtx *fc, int cx, int cy, int cz, int xz_radius) {
  if (t_is_empty_or_water_or_lava(feat_get(fc, cx, cy, cz))) return 0;
  float angle_increment = 6.0F / (float)xz_radius;
  float limit = (float)(M_PI * 2);
  for (float angle = 0.0F; angle < limit; angle += angle_increment) {
    int dx = (int)(t_cos((double)angle) * (float)xz_radius);
    int dz = (int)(t_sin((double)angle) * (float)xz_radius);
    if (t_is_empty_or_water_or_lava(feat_get(fc, cx + dx, cy, cz + dz))) return 0;
  }
  return 1;
}

typedef struct {
  int origin_y;
  int has_wind;
  double wx, wz;
  int max_offset;
} TWind;

static void t_wind_offset(const TWind *w, int x, int y, int z, int *ox, int *oz) {
  *ox = x;
  *oz = z;
  if (!w->has_wind) return;
  int dy = w->origin_y - y;
  double tx = w->wx * (double)dy;
  double tz = w->wz * (double)dy;
  int dx = t_clamp_i(t_floor_d(tx), -w->max_offset, w->max_offset);
  int dz = t_clamp_i(t_floor_d(tz), -w->max_offset, w->max_offset);
  *ox = x + dx;
  *oz = z + dz;
}

typedef struct {
  int rx, ry, rz;       /* root */
  int pointing_up;
  int radius;
  double bluntness;
  double scale;
} TDripstone;

static int t_dripstone_height(const TDripstone *d) {
  return t_large_dripstone_get_height_at_radius(d->bluntness, d->radius, d->scale, 0.0F);
}

static int t_dripstone_suitable_for_wind(const TDripstone *d, Jv *config) {
  return d->radius >= (int)jnum(jget(config, "min_radius_for_wind"), 4) &&
         d->bluntness >= jnum(jget(config, "min_bluntness_for_wind"), 0.6);
}

static int t_dripstone_move_back(TDripstone *d, FeatureCtx *fc, const TWind *wind, uint16_t replaceable) {
  while (d->radius > 1) {
    int nx = d->rx, ny = d->ry, nz = d->rz;
    int max_tries = t_min_i(10, t_dripstone_height(d));
    for (int i = 0; i < max_tries; i++) {
      if (feat_get(fc, nx, ny, nz) == B_LAVA) return 0;
      int wx, wz;
      t_wind_offset(wind, nx, ny, nz, &wx, &wz);
      if (t_circle_mostly_embedded_in_stone(fc, wx, ny, wz, d->radius)) {
        d->rx = nx;
        d->ry = ny;
        d->rz = nz;
        return 1;
      }
      if (d->pointing_up) {
        ny--;
      } else {
        ny++;
      }
    }
    d->radius /= 2;
  }
  return 0;
}

static void t_dripstone_place_blocks(TDripstone *d, FeatureCtx *fc, const TWind *wind) {
  LegacyRng *r = feat_rng(fc);
  for (int dx = -d->radius; dx <= d->radius; dx++) {
    for (int dz = -d->radius; dz <= d->radius; dz++) {
      float current_radius = t_sqrt_f((float)(dx * dx + dz * dz));
      if (current_radius > (float)d->radius) continue;
      int height = t_large_dripstone_get_height_at_radius(d->bluntness, d->radius, d->scale, current_radius);
      if (height <= 0) continue;
      if (legacy_next_float(r) < 0.2F) {
        height = (int)((float)height * t_random_between_f(r, 0.8F, 1.0F));
      }
      int x = d->rx + dx, y = d->ry, z = d->rz + dz;
      int has_been_out_of_stone = 0;
      int max_y = d->pointing_up ? feat_height(fc, x, z, FEAT_HM_WORLD_SURFACE_WG) : INT32_MAX;
      for (int i = 0; i < height && y < max_y; i++) {
        int wx, wz;
        t_wind_offset(wind, x, y, z, &wx, &wz);
        if (t_is_empty_or_water_or_lava(feat_get(fc, wx, y, wz))) {
          has_been_out_of_stone = 1;
          feat_set(fc, wx, y, wz, B_DRIPSTONE);
        } else if (has_been_out_of_stone &&
                   block_tag_contains("minecraft:base_stone_overworld", feat_get(fc, wx, y, wz))) {
          break;
        }
        if (d->pointing_up) {
          y++;
        } else {
          y--;
        }
      }
    }
  }
}

static int feature_large_dripstone(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  Jv *column_radius = jget(config, "column_radius");
  Jv *replaceable = jget(config, "replaceable_blocks");
  int search_range = (int)jnum(jget(config, "floor_to_ceiling_search_range"), 30) /* codec default */;
  int radius_min = t_int_provider_min(column_radius);
  int radius_max = t_int_provider_max(column_radius);
  uint16_t dripstone = B_DRIPSTONE;

  if (!t_is_empty_or_water(feat_get(fc, ox, oy, oz))) return 0;

  TColumn column;
  TColumnEdgeCtx edge_ctx;
  edge_ctx.base_block = dripstone;
  edge_ctx.replaceable = replaceable;
  if (!t_column_scan_full(fc, ox, oy, oz, search_range, t_pred_empty_or_water, 0, t_pred_base_or_lava,
                          &edge_ctx, &column)) {
    return 0;
  }
  if (!column.has_ceiling || !column.has_floor) return 0;

  int range_height = column.ceiling_y - column.floor_y + 1;
  if (range_height < 4) return 0;

  int max_radius_based_on_height =
      (int)((double)range_height * jnum(jget(config, "max_column_radius_to_cave_height_ratio"), 0.33));
  int max_radius = t_clamp_i(max_radius_based_on_height, radius_min, radius_max);
  int radius = t_random_between_i(r, radius_min, max_radius);

  TDripstone stalactite, stalagmite;
  stalactite.pointing_up = 0;
  stalactite.rx = ox;
  stalactite.ry = column.ceiling_y - 1;
  stalactite.rz = oz;
  stalactite.radius = radius;
  stalactite.bluntness = feat_float_provider(jget(config, "stalactite_bluntness"), r);
  stalactite.scale = feat_float_provider(jget(config, "height_scale"), r);
  stalagmite.pointing_up = 1;
  stalagmite.rx = ox;
  stalagmite.ry = column.floor_y + 1;
  stalagmite.rz = oz;
  stalagmite.radius = radius;
  stalagmite.bluntness = feat_float_provider(jget(config, "stalagmite_bluntness"), r);
  stalagmite.scale = feat_float_provider(jget(config, "height_scale"), r);

  TWind wind;
  wind.has_wind = 0;
  wind.origin_y = oy;
  wind.max_offset = 0;
  wind.wx = wind.wz = 0.0;
  if (t_dripstone_suitable_for_wind(&stalactite, config) && t_dripstone_suitable_for_wind(&stalagmite, config)) {
    float speed = (float)feat_float_provider(jget(config, "wind_speed"), r);
    float direction = t_random_between_f(r, 0.0F, (float)M_PI);
    wind.has_wind = 1;
    wind.origin_y = oy;
    wind.max_offset = 16 - radius;
    wind.wx = (double)(t_cos((double)direction) * speed);
    wind.wz = (double)(t_sin((double)direction) * speed);
  }

  int stalactite_ok = t_dripstone_move_back(&stalactite, fc, &wind, dripstone);
  int stalagmite_ok = t_dripstone_move_back(&stalagmite, fc, &wind, dripstone);
  if (stalactite_ok) t_dripstone_place_blocks(&stalactite, fc, &wind);
  if (stalagmite_ok) t_dripstone_place_blocks(&stalagmite, fc, &wind);
  return 1;
}

/* ======================================================== monster_room =====
 * MonsterRoomFeature: a 3-block-tall cobblestone room with a spawner, chests
 * and the mossy/cobweb detail. Chests are placed as plain blocks (no block
 * entity, no facing property) and the spawner's mob id is not stored - both are
 * property/block-entity level and not representable in Cinder's palette. */

static int t_monster_room_can_replace(FeatureCtx *fc, int x, int y, int z) {
  return !block_tag_contains("minecraft:features_cannot_replace", feat_get(fc, x, y, z));
}

static int t_monster_room_safe_set(FeatureCtx *fc, int x, int y, int z, uint16_t block) {
  if (t_monster_room_can_replace(fc, x, y, z)) {
    feat_set(fc, x, y, z, block);
    return 1;
  }
  return 0;
}

static int feature_monster_room(FeatureCtx *fc, Jv *config) {
  (void)config;
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  int xr = legacy_next_bound(r, 2) + 2;
  int min_x = -xr - 1, max_x = xr + 1;
  int zr = legacy_next_bound(r, 2) + 2;
  int min_z = -zr - 1, max_z = zr + 1;
  int hole_count = 0;

  for (int dx = min_x; dx <= max_x; dx++) {
    for (int dy = -1; dy <= 4; dy++) {
      for (int dz = min_z; dz <= max_z; dz++) {
        int hx = ox + dx, hy = oy + dy, hz = oz + dz;
        int solid = t_is_solid(feat_get(fc, hx, hy, hz));
        if (dy == -1 && !solid) return 0;
        if (dy == 4 && !solid) return 0;
        if ((dx == min_x || dx == max_x || dz == min_z || dz == max_z) && dy == 0 &&
            t_is_air(feat_get(fc, hx, hy, hz)) && t_is_air(feat_get(fc, hx, hy + 1, hz))) {
          hole_count++;
        }
      }
    }
  }

  if (hole_count < 1 || hole_count > 5) return 0;

  int chest = block_id_for_name("minecraft:chest");

  for (int dx = min_x; dx <= max_x; dx++) {
    for (int dy = 3; dy >= -1; dy--) {
      for (int dz = min_z; dz <= max_z; dz++) {
        int wx = ox + dx, wy = oy + dy, wz = oz + dz;
        uint16_t wall_state = feat_get(fc, wx, wy, wz);
        if (dx == min_x || dy == -1 || dz == min_z || dx == max_x || dy == 4 || dz == max_z) {
          if (wy >= MIN_Y && !t_is_solid(feat_get(fc, wx, wy - 1, wz))) {
            t_monster_room_safe_set(fc, wx, wy, wz, B_CAVE_AIR);
          } else if (t_is_solid(wall_state) && (int)wall_state != chest) {
            if (dy == -1 && legacy_next_bound(r, 4) != 0) {
              t_monster_room_safe_set(fc, wx, wy, wz, B_MOSSY_COBBLESTONE);
            } else {
              t_monster_room_safe_set(fc, wx, wy, wz, B_COBBLESTONE);
            }
          }
        } else if ((int)wall_state != chest && wall_state != B_SPAWNER) {
          t_monster_room_safe_set(fc, wx, wy, wz, B_CAVE_AIR);
        }
      }
    }
  }

  for (int cc = 0; cc < 2; cc++) {
    for (int i = 0; i < 3; i++) {
      int xc = ox + legacy_next_bound(r, xr * 2 + 1) - xr;
      int yc = oy;
      int zc = oz + legacy_next_bound(r, zr * 2 + 1) - zr;
      if (t_is_air(feat_get(fc, xc, yc, zc))) {
        int wall_count = 0;
        for (int d = 0; d < 4; d++) {
          int dx, dy, dz;
          t_dir_vec(T_HORIZONTAL[d], &dx, &dy, &dz);
          if (t_is_solid(feat_get(fc, xc + dx, yc, zc + dz))) wall_count++;
        }
        if (wall_count == 1) {
          if (chest >= 0 && t_monster_room_safe_set(fc, xc, yc, zc, (uint16_t)chest)) {
            /* RandomizableContainer.setBlockEntityLootTable: the container draws
               random.nextLong() for its loot-table seed. */
            legacy_next_long(r);
          }
          break;
        }
      }
    }
  }

  t_monster_room_safe_set(fc, ox, oy, oz, B_SPAWNER);
  /* SpawnerBlockEntity.setEntityId(randomEntityId(random), random); MOBS is
     {skeleton, zombie, zombie, spider}. */
  legacy_next_bound(r, 4);
  return 1;
}

/* ========================================================= sculk_patch ====
 * SculkPatchFeature + SculkSpreader + SculkBehaviour. Cursor bookkeeping, RNG
 * order and the charge/decay arithmetic follow vanilla. The vein layer is
 * approximate: SCULK_VEIN keeps its six face flags as block state properties,
 * which Cinder's name-level palette cannot hold, so faces are re-derived from
 * the surrounding blocks instead of stored. */

typedef struct {
  int x, y, z;
  int charge;
  int update_delay;
  int decay_delay;
  int has_facings;
} TSculkCursor;

typedef struct {
  TSculkCursor cursors[32];
  int count;
  uint16_t sculk, vein, catalyst, shrieker, sensor;
  int growth_spawn_cost, no_growth_radius, charge_decay_rate, additional_decay_rate;
  const char *replaceable; /* the worldgen spreader's replaceable block tag */
} TSculkSpreader;

#define SCULK_BEHAV_DEFAULT 0
#define SCULK_BEHAV_SCULK 1
#define SCULK_BEHAV_VEIN 2

static int t_sculk_behaviour(const TSculkSpreader *sp, uint16_t b) {
  if (b == sp->sculk) return SCULK_BEHAV_SCULK;
  if (b == sp->vein) return SCULK_BEHAV_VEIN;
  return SCULK_BEHAV_DEFAULT;
}

/* MultifaceBlock.canAttachTo */
static int t_vein_can_attach(FeatureCtx *fc, int x, int y, int z, int dir, const TSculkSpreader *sp) {
  int dx, dy, dz;
  t_dir_vec(dir, &dx, &dy, &dz);
  uint16_t against = feat_get(fc, x + dx, y + dy, z + dz);
  if (against == sp->sculk || against == sp->catalyst) return 0;
  return feat_is_face_sturdy(against, t_dir_opposite(dir));
}

/* Approximated MultifaceBlock.availableFaces: the vein's faces are the ones it
 * could attach to now. */
static int t_vein_faces(FeatureCtx *fc, int x, int y, int z, const TSculkSpreader *sp, int *faces) {
  int n = 0;
  for (int i = 0; i < 6; i++) {
    int dir = T_ALL_DIRS[i];
    if (t_vein_can_attach(fc, x, y, z, dir, sp)) faces[n++] = dir;
  }
  return n;
}

/* SculkVeinSpreaderConfig.stateCanBeReplaced + MultifaceBlock.isValidStateForPlacement
 * for placing a vein face `dir` at (x,y,z); `dist2` is
 * sourcePos.distManhattan(placementPos) == 2 (the WRAP_AROUND case). */
static int t_vein_can_place(FeatureCtx *fc, int x, int y, int z, int dir, int dist2, const TSculkSpreader *sp) {
  uint16_t existing = feat_get(fc, x, y, z);
  if (dist2) {
    int dx, dy, dz;
    t_dir_vec(dir, &dx, &dy, &dz);
    if (feat_is_face_sturdy(feat_get(fc, x - dx, y - dy, z - dz), dir)) return 0;
  }
  if (feat_is_fluid(existing) && existing != B_WATER) return 0;
  if (existing != sp->vein && !feat_can_replace(existing)) return 0;
  return t_vein_can_attach(fc, x, y, z, dir, sp);
}

/* MultifaceSpreader.placeBlock: a vein state at spreadPos. Returns 1 when the
 * block changed (vanilla's setBlock return value). */
static int t_vein_place(FeatureCtx *fc, int x, int y, int z, int dir, int dist2, const TSculkSpreader *sp) {
  if (feat_get(fc, x, y, z) == sp->vein) return 0;
  if (!t_vein_can_place(fc, x, y, z, dir, dist2, sp)) return 0;
  feat_set(fc, x, y, z, sp->vein);
  return 1;
}

/* MultifaceSpreader.getSpreadFromFaceTowardDirection + spreadToFace. */
static int t_vein_spread_one(FeatureCtx *fc, int x, int y, int z, int from_face, int spread_dir, int is_other,
                             const TSculkSpreader *sp) {
  int dx, dy, dz;
  t_dir_vec(spread_dir, &dx, &dy, &dz);
  int fx, fy, fz;
  t_dir_vec(from_face, &fx, &fy, &fz);
  /* SAME_POSITION, then SAME_PLANE, then WRAP_AROUND. */
  if (t_vein_place(fc, x, y, z, spread_dir, 0, sp)) return 1;
  (void)is_other;
  if (t_vein_place(fc, x + dx, y + dy, z + dz, from_face, 0, sp)) return 1;
  if (t_vein_place(fc, x + dx + fx, y + dy + fy, z + dz + fz, t_dir_opposite(spread_dir), 1, sp)) return 1;
  return 0;
}

/* MultifaceSpreader.spreadAll (veinSpreader = all three spread types,
 * sameSpaceSpreader = SAME_POSITION only). */
static int t_vein_spread_all(FeatureCtx *fc, int x, int y, int z, int vein_state, int *faces, int nfaces,
                            int same_position_only, const TSculkSpreader *sp) {
  int count = 0;
  for (int i = 0; i < 6; i++) {
    int from_face = T_ALL_DIRS[i];
    int can_spread_from = !vein_state;
    if (vein_state) {
      can_spread_from = 0;
      for (int k = 0; k < nfaces; k++) {
        if (faces[k] == from_face) can_spread_from = 1;
      }
    }
    if (!can_spread_from) continue;
    for (int j = 0; j < 6; j++) {
      int spread_dir = T_ALL_DIRS[j];
      if ((spread_dir >> 1) == (from_face >> 1)) continue; /* same axis */
      if (vein_state) {
        int has_spread_face = 0;
        for (int k = 0; k < nfaces; k++) {
          if (faces[k] == spread_dir) has_spread_face = 1;
        }
        if (has_spread_face) continue;
      }
      if (same_position_only) {
        if (t_vein_place(fc, x, y, z, spread_dir, 0, sp)) count++;
        continue;
      }
      count += t_vein_spread_one(fc, x, y, z, from_face, spread_dir, !vein_state, sp);
    }
  }
  return count;
}

/* SculkBehaviour.DEFAULT.attemptSpreadVein (facings == null -> the same-space
 * spreader; facings non-empty -> regrow on the recorded faces). */
static int t_sculk_default_spread(FeatureCtx *fc, int x, int y, int z, int has_facings, int *faces, int nfaces,
                                  const TSculkSpreader *sp) {
  uint16_t state = feat_get(fc, x, y, z);
  if (!has_facings) {
    int is_vein = (state == sp->vein);
    int dfaces[6];
    int dnfaces = is_vein ? t_vein_faces(fc, x, y, z, sp, dfaces) : 0;
    return t_vein_spread_all(fc, x, y, z, is_vein, dfaces, dnfaces, 1, sp) > 0;
  }
  if (nfaces > 0) {
    if (!t_is_air(state) && !t_water_fluid(state)) return 0;
    int placed = 0;
    for (int k = 0; k < nfaces; k++) {
      if (t_vein_can_place(fc, x, y, z, faces[k], 0, sp)) placed = 1;
    }
    if (!placed) return 0;
    if (state != sp->vein) feat_set(fc, x, y, z, sp->vein);
    return 1;
  }
  return 0;
}

/* SculkBlock.canPlaceGrowth */
static int t_sculk_can_place_growth(FeatureCtx *fc, int x, int y, int z, const TSculkSpreader *sp) {
  uint16_t above = feat_get(fc, x, y + 1, z);
  if (!(t_is_air(above) || (above == B_WATER && t_water_fluid(above)))) return 0;
  int growth_count = 0;
  for (int dz = -4; dz <= 4; dz++) {
    for (int dy = 0; dy <= 2; dy++) {
      for (int dx = -4; dx <= 4; dx++) {
        uint16_t s = feat_get(fc, x + dx, y + dy, z + dz);
        if (s == sp->sensor || s == sp->shrieker) growth_count++;
        if (growth_count > 2) return 0;
      }
    }
  }
  return 1;
}

/* SculkBlock.getDecayPenalty */
static int t_sculk_decay_penalty(const TSculkSpreader *sp, int x, int y, int z, int ox, int oy, int oz, int charge) {
  float outer_distance_squared =
      (float)((float)sqrt(t_dist_sqr(x, y, z, ox, oy, oz)) - (float)sp->no_growth_radius);
  outer_distance_squared = outer_distance_squared * outer_distance_squared;
  int max_reach_squared = (24 - sp->no_growth_radius) * (24 - sp->no_growth_radius);
  float distance_factor = t_min_f(1.0F, outer_distance_squared / (float)max_reach_squared);
  int penalty = (int)((float)charge * distance_factor * 0.5F);
  return penalty < 1 ? 1 : penalty;
}

/* SculkBlock.getRandomGrowthState */
static uint16_t t_sculk_growth_state(LegacyRng *r, const TSculkSpreader *sp) {
  return legacy_next_bound(r, 11) == 0 ? sp->shrieker : sp->sensor;
}

/* SculkVeinBlock.onDischarged */
static void t_vein_on_discharged(FeatureCtx *fc, int x, int y, int z, const TSculkSpreader *sp) {
  int faces[6];
  int nfaces = t_vein_faces(fc, x, y, z, sp, faces);
  int remaining = 0;
  for (int k = 0; k < nfaces; k++) {
    int dx, dy, dz;
    t_dir_vec(faces[k], &dx, &dy, &dz);
    if (feat_get(fc, x + dx, y + dy, z + dz) != sp->sculk) remaining++;
  }
  if (remaining > 0) return;
  /* Every face pointed at sculk: vanilla drops the faces and, with no face left,
     replaces the block with air (or water when it was waterlogged - the
     waterlogged flag is not representable here). */
  feat_set(fc, x, y, z, B_AIR);
}

/* SculkVeinBlock.attemptPlaceSculk */
static int t_vein_attempt_place_sculk(FeatureCtx *fc, int x, int y, int z, LegacyRng *r, const TSculkSpreader *sp) {
  int order[6];
  t_all_dirs_shuffled(r, order);
  for (int i = 0; i < 6; i++) {
    int support = order[i];
    int faces[6];
    int nfaces = t_vein_faces(fc, x, y, z, sp, faces);
    int has_face = 0;
    for (int k = 0; k < nfaces; k++) {
      if (faces[k] == support) has_face = 1;
    }
    if (!has_face) continue;
    int dx, dy, dz;
    t_dir_vec(support, &dx, &dy, &dz);
    int sx = x + dx, sy = y + dy, sz = z + dz;
    uint16_t support_state = feat_get(fc, sx, sy, sz);
    if (block_tag_contains(sp->replaceable, support_state)) {
      feat_set(fc, sx, sy, sz, sp->sculk);
      t_vein_spread_all(fc, sx, sy, sz, 0, 0, 0, 0, sp);
      int skip = t_dir_opposite(support);
      for (int d = 0; d < 6; d++) {
        int vdir = T_ALL_DIRS[d];
        if (vdir == skip) continue;
        int vx, vy, vz;
        t_dir_vec(vdir, &vx, &vy, &vz);
        if (feat_get(fc, sx + vx, sy + vy, sz + vz) == sp->vein) {
          t_vein_on_discharged(fc, sx + vx, sy + vy, sz + vz, sp);
        }
      }
      return 1;
    }
  }
  return 0;
}

/* SculkBehaviour.attemptUseCharge dispatch. Returns the new charge. */
static int t_sculk_use_charge(TSculkSpreader *sp, FeatureCtx *fc, TSculkCursor *c, int behaviour, int ox, int oy,
                              int oz, int spread_veins) {
  LegacyRng *r = feat_rng(fc);
  if (behaviour == SCULK_BEHAV_SCULK) {
    /* SculkBlock.attemptUseCharge */
    int charge = c->charge;
    if (charge == 0 || legacy_next_bound(r, sp->charge_decay_rate) != 0) return charge;
    int close_to_catalyst = t_closer_than(c->x, c->y, c->z, ox, oy, oz, (double)sp->no_growth_radius);
    if (!close_to_catalyst && t_sculk_can_place_growth(fc, c->x, c->y, c->z, sp)) {
      if (legacy_next_bound(r, sp->growth_spawn_cost) < charge) {
        feat_set(fc, c->x, c->y + 1, c->z, t_sculk_growth_state(r, sp));
      }
      return t_max_i(0, charge - sp->growth_spawn_cost);
    }
    if (legacy_next_bound(r, sp->additional_decay_rate) != 0) return charge;
    return charge - (close_to_catalyst ? 1 : t_sculk_decay_penalty(sp, c->x, c->y, c->z, ox, oy, oz, charge));
  }
  if (behaviour == SCULK_BEHAV_VEIN) {
    /* SculkVeinBlock.attemptUseCharge */
    if (spread_veins && t_vein_attempt_place_sculk(fc, c->x, c->y, c->z, r, sp)) return c->charge - 1;
    return legacy_next_bound(r, sp->charge_decay_rate) == 0 ? t_floor_f(c->charge * 0.5F) : c->charge;
  }
  /* SculkBehaviour.DEFAULT.attemptUseCharge */
  return c->decay_delay > 0 ? c->charge : 0;
}

static int t_sculk_update_decay_delay(int behaviour, int age) {
  if (behaviour == SCULK_BEHAV_DEFAULT) return t_max_i(age - 1, 0);
  return 1;
}

static int t_sculk_can_change_state_on_spread(int behaviour) { return behaviour != SCULK_BEHAV_SCULK; }

/* SculkSpreader.ChargeCursor.getValidMovementPos */
static int t_sculk_is_unobstructed(FeatureCtx *fc, int x, int y, int z, int dir) {
  int dx, dy, dz;
  t_dir_vec(dir, &dx, &dy, &dz);
  int nx = x + dx, ny = y + dy, nz = z + dz;
  return !feat_is_face_sturdy(feat_get(fc, nx, ny, nz), t_dir_opposite(dir));
}

static int t_sculk_movement_unobstructed(FeatureCtx *fc, int fx, int fy, int fz, int tx, int ty, int tz) {
  int dx = tx - fx, dy = ty - fy, dz = tz - fz;
  int manhattan = t_abs_i(dx) + t_abs_i(dy) + t_abs_i(dz);
  int dir_x = dx < 0 ? T_DIR_WEST : T_DIR_EAST;
  int dir_y = dy < 0 ? T_DIR_DOWN : T_DIR_UP;
  int dir_z = dz < 0 ? T_DIR_NORTH : T_DIR_SOUTH;
  if (manhattan == 1) return 1;
  if (dx == 0) return t_sculk_is_unobstructed(fc, fx, fy, fz, dir_y) || t_sculk_is_unobstructed(fc, fx, fy, fz, dir_z);
  if (dy == 0) return t_sculk_is_unobstructed(fc, fx, fy, fz, dir_x) || t_sculk_is_unobstructed(fc, fx, fy, fz, dir_z);
  return t_sculk_is_unobstructed(fc, fx, fy, fz, dir_x) || t_sculk_is_unobstructed(fc, fx, fy, fz, dir_y);
}

static int t_sculk_has_substrate_access(FeatureCtx *fc, int x, int y, int z, const TSculkSpreader *sp) {
  if (feat_get(fc, x, y, z) != sp->vein) return 0;
  int faces[6];
  int nfaces = t_vein_faces(fc, x, y, z, sp, faces);
  for (int k = 0; k < nfaces; k++) {
    int dx, dy, dz;
    t_dir_vec(faces[k], &dx, &dy, &dz);
    if (block_tag_contains(sp->replaceable, feat_get(fc, x + dx, y + dy, z + dz))) return 1;
  }
  return 0;
}

/* SculkSpreader.ChargeCursor.update */
static void t_sculk_cursor_update(TSculkSpreader *sp, FeatureCtx *fc, TSculkCursor *c, int ox, int oy, int oz,
                                  int spread_veins) {
  LegacyRng *r = feat_rng(fc);
  if (c->charge <= 0) return; /* shouldUpdate: worldgen always updates */
  if (c->update_delay > 0) {
    c->update_delay--;
    return;
  }

  uint16_t current_state = feat_get(fc, c->x, c->y, c->z);
  int behaviour = t_sculk_behaviour(sp, current_state);

  if (spread_veins) {
    int spread;
    if (behaviour == SCULK_BEHAV_SCULK) {
      /* SculkBehaviour's interface default: the vein spreader on the sculk
         state, which ignores the cursor's own faces. */
      spread = t_vein_spread_all(fc, c->x, c->y, c->z, 0, 0, 0, 0, sp) > 0;
    } else if (behaviour == SCULK_BEHAV_VEIN) {
      int faces[6];
      int nfaces = t_vein_faces(fc, c->x, c->y, c->z, sp, faces);
      spread = t_vein_spread_all(fc, c->x, c->y, c->z, 1, faces, nfaces, 0, sp) > 0;
    } else {
      int faces[6];
      int nfaces = c->has_facings ? t_vein_faces(fc, c->x, c->y, c->z, sp, faces) : 0;
      spread = t_sculk_default_spread(fc, c->x, c->y, c->z, c->has_facings, faces, nfaces, sp);
    }
    if (spread && t_sculk_can_change_state_on_spread(behaviour)) {
      current_state = feat_get(fc, c->x, c->y, c->z);
      behaviour = t_sculk_behaviour(sp, current_state);
    }
  }

  c->charge = t_sculk_use_charge(sp, fc, c, behaviour, ox, oy, oz, spread_veins);
  if (c->charge <= 0) {
    if (behaviour == SCULK_BEHAV_VEIN) t_vein_on_discharged(fc, c->x, c->y, c->z, sp);
    return;
  }

  /* getValidMovementPos: Util.shuffledCopy(NON_CORNER_NEIGHBOURS, random), the
     18 non-corner neighbours in BlockPos.betweenClosed(z outer, y, x inner)
     order. */
  int offsets[18][3];
  int n = 0;
  for (int z = -1; z <= 1; z++) {
    for (int y = -1; y <= 1; y++) {
      for (int x = -1; x <= 1; x++) {
        if ((x == 0 || y == 0 || z == 0) && !(x == 0 && y == 0 && z == 0)) {
          offsets[n][0] = x;
          offsets[n][1] = y;
          offsets[n][2] = z;
          n++;
        }
      }
    }
  }
  for (int i = n; i > 1; i--) {
    int swap_to = legacy_next_bound(r, i);
    int tmp[3];
    tmp[0] = offsets[i - 1][0]; tmp[1] = offsets[i - 1][1]; tmp[2] = offsets[i - 1][2];
    offsets[i - 1][0] = offsets[swap_to][0]; offsets[i - 1][1] = offsets[swap_to][1]; offsets[i - 1][2] = offsets[swap_to][2];
    offsets[swap_to][0] = tmp[0]; offsets[swap_to][1] = tmp[1]; offsets[swap_to][2] = tmp[2];
  }

  int move_x = c->x, move_y = c->y, move_z = c->z;
  for (int i = 0; i < n; i++) {
    int nx = c->x + offsets[i][0], ny = c->y + offsets[i][1], nz = c->z + offsets[i][2];
    uint16_t transferee = feat_get(fc, nx, ny, nz);
    if (t_sculk_behaviour(sp, transferee) != SCULK_BEHAV_DEFAULT &&
        t_sculk_movement_unobstructed(fc, c->x, c->y, c->z, nx, ny, nz)) {
      move_x = nx; move_y = ny; move_z = nz;
      if (t_sculk_has_substrate_access(fc, nx, ny, nz, sp)) break;
    }
  }

  int moved = !(move_x == c->x && move_y == c->y && move_z == c->z);
  if (moved) {
    if (behaviour == SCULK_BEHAV_VEIN) t_vein_on_discharged(fc, c->x, c->y, c->z, sp);
    c->x = move_x; c->y = move_y; c->z = move_z;
    /* worldgen: !closerThan((originX, y, originZ), 15.0) -> charge = 0, return */
    if (!t_closer_than(c->x, c->y, c->z, ox, c->y, oz, 15.0)) {
      c->charge = 0;
      return;
    }
    current_state = feat_get(fc, c->x, c->y, c->z);
  }

  if (t_sculk_behaviour(sp, current_state) != SCULK_BEHAV_DEFAULT) {
    /* MultifaceBlock.availableFaces(currentState): null for SCULK, the face set
       for SCULK_VEIN. */
    c->has_facings = (current_state == sp->vein);
  }

  c->decay_delay = t_sculk_update_decay_delay(behaviour, c->decay_delay);
  c->update_delay = 1; /* getSculkSpreadDelay */
}

static void t_sculk_add_cursors(TSculkSpreader *sp, int x, int y, int z, int charge) {
  while (charge > 0) {
    int current_charge = t_min_i(charge, 1000);
    if (sp->count < 32) {
      TSculkCursor *c = &sp->cursors[sp->count++];
      memset(c, 0, sizeof *c);
      c->x = x; c->y = y; c->z = z;
      c->charge = current_charge;
      c->update_delay = 0;
      c->decay_delay = 1;
      c->has_facings = 0;
    }
    charge -= current_charge;
  }
}

static void t_sculk_update_cursors(TSculkSpreader *sp, FeatureCtx *fc, int ox, int oy, int oz, int spread_veins) {
  if (sp->count == 0) return;
  int kept = 0;
  for (int i = 0; i < sp->count; i++) {
    TSculkCursor *c = &sp->cursors[i];
    int chessboard = t_max_i(t_max_i(t_abs_i(c->x - ox), t_abs_i(c->y - oy)), t_abs_i(c->z - oz));
    if (chessboard > 1024) continue; /* isPosUnreasonable */
    t_sculk_cursor_update(sp, fc, c, ox, oy, oz, spread_veins);
    if (c->charge <= 0) continue;
    if (kept != i) sp->cursors[kept] = *c;
    kept++;
  }
  sp->count = kept;
}

static int t_sculk_can_spread_from(FeatureCtx *fc, int x, int y, int z, const TSculkSpreader *sp) {
  uint16_t start = feat_get(fc, x, y, z);
  if (t_sculk_behaviour(sp, start) != SCULK_BEHAV_DEFAULT) return 1;
  if (!t_is_air(start) && !(start == B_WATER)) return 0; /* water is a source */
  for (int i = 0; i < 6; i++) {
    int dx, dy, dz;
    t_dir_vec(T_ALL_DIRS[i], &dx, &dy, &dz);
    if (feat_is_collision_full(feat_get(fc, x + dx, y + dy, z + dz))) return 1;
  }
  return 0;
}

static int feature_sculk_patch(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  TSculkSpreader sp;
  memset(&sp, 0, sizeof sp);
  {
    int a = block_id_for_name("minecraft:sculk");
    int b = block_id_for_name("minecraft:sculk_vein");
    int c = block_id_for_name("minecraft:sculk_catalyst");
    int d = block_id_for_name("minecraft:sculk_shrieker");
    int e = block_id_for_name("minecraft:sculk_sensor");
    if (a < 0 || b < 0 || c < 0 || d < 0 || e < 0) return 0; /* palette gap */
    sp.sculk = (uint16_t)a;
    sp.vein = (uint16_t)b;
    sp.catalyst = (uint16_t)c;
    sp.shrieker = (uint16_t)d;
    sp.sensor = (uint16_t)e;
  }
  /* createWorldGenSpreader: SCULK_REPLACEABLE_WORLD_GEN, cost 50, radius 1,
     decay 5, additional decay 10. */
  sp.growth_spawn_cost = 50;
  sp.no_growth_radius = 1;
  sp.charge_decay_rate = 5;
  sp.additional_decay_rate = 10;
  sp.replaceable = "minecraft:sculk_replaceable_world_gen";

  if (!t_sculk_can_spread_from(fc, ox, oy, oz, &sp)) return 0;

  int charge_count = (int)jnum(jget(config, "charge_count"), 10);
  int amount_per_charge = (int)jnum(jget(config, "amount_per_charge"), 32);
  int spread_attempts = (int)jnum(jget(config, "spread_attempts"), 64);
  int growth_rounds = (int)jnum(jget(config, "growth_rounds"), 0);
  int spread_rounds = (int)jnum(jget(config, "spread_rounds"), 1);
  float catalyst_chance = (float)jnum(jget(config, "catalyst_chance"), 0.5);
  int total_rounds = spread_rounds + growth_rounds;

  for (int round = 0; round < total_rounds; round++) {
    for (int i = 0; i < charge_count; i++) t_sculk_add_cursors(&sp, ox, oy, oz, amount_per_charge);
    int spread_veins = round < spread_rounds;
    for (int i = 0; i < spread_attempts; i++) t_sculk_update_cursors(&sp, fc, ox, oy, oz, spread_veins);
    sp.count = 0;
  }

  if (legacy_next_float(r) <= catalyst_chance && feat_is_collision_full(feat_get(fc, ox, oy - 1, oz))) {
    feat_set(fc, ox, oy, oz, sp.catalyst);
  }

  int extra_growths = feat_int_provider(jget(config, "extra_rare_growths"), r);
  for (int i = 0; i < extra_growths; i++) {
    int cx = ox + legacy_next_bound(r, 5) - 2;
    int cz = oz + legacy_next_bound(r, 5) - 2;
    if (t_is_air(feat_get(fc, cx, oy, cz)) && feat_is_face_sturdy(feat_get(fc, cx, oy - 1, cz), T_DIR_UP)) {
      feat_set(fc, cx, oy, cz, sp.shrieker);
    }
  }
  return 1;
}

/* ============================================================ sequence =====
 * SequenceFeature: every entry must place, and the whole feature reports that
 * OR (vanilla returns false as soon as one entry fails). */
static int feature_sequence(FeatureCtx *fc, Jv *config) {
  Jv *features = jget(config, "features");
  for (Jv *e = features ? features->child : 0; e; e = e->next) {
    if (!feat_apply_placed(fc, e, fc->origin_x, fc->origin_y, fc->origin_z)) return 0;
  }
  return 1;
}

/* ============================================== random_boolean_selector ===
 * RandomBooleanSelectorFeature: one nextBoolean draw, then that placed feature. */
static int feature_random_boolean_selector(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int result = legacy_next(r, 1) != 0;
  Jv *chosen = jget(config, result ? "feature_true" : "feature_false");
  return feat_apply_placed(fc, chosen, fc->origin_x, fc->origin_y, fc->origin_z);
}

/* ============================================================== fossil =====
 * FossilFeature: rotation/template pick, the OCEAN_FLOOR_WG surface scan, the
 * empty-corner test and the two template passes with their processors. The
 * template block lists come from FOSS_POS above (extracted from the vanilla
 * server jar); the bone_block `axis` and `Rotation`-rotated state properties are
 * property level and therefore dropped. */

#define FOSS_ROT_CW90 1
#define FOSS_ROT_CW180 2
#define FOSS_ROT_CCW90 3

static int t_foss_transform_x(int x, int z, int rotation) {
  switch (rotation) {
    case FOSS_ROT_CW90: return -z;
    case FOSS_ROT_CW180: return -x;
    case FOSS_ROT_CCW90: return z;
    default: return x;
  }
}

static int t_foss_transform_z(int x, int z, int rotation) {
  switch (rotation) {
    case FOSS_ROT_CW90: return x;
    case FOSS_ROT_CW180: return -z;
    case FOSS_ROT_CCW90: return -x;
    default: return z;
  }
}

static int t_foss_template_index(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < 8; i++) {
    if (!strcmp(name, FOSS_TEMPLATE_NAMES[i])) return i;
  }
  return -1;
}

/* BoundingBox.forAllCorners + FossilFeature.countEmptyCorners */
static int t_foss_count_empty_corners(FeatureCtx *fc, int x0, int y0, int z0, int x1, int y1, int z1) {
  static const int corners[8][3] = {{1, 1, 1}, {0, 1, 1}, {1, 0, 1}, {0, 0, 1},
                                    {1, 1, 0}, {0, 1, 0}, {1, 0, 0}, {0, 0, 0}};
  int count = 0;
  for (int i = 0; i < 8; i++) {
    int x = corners[i][0] ? x1 : x0;
    int y = corners[i][1] ? y1 : y0;
    int z = corners[i][2] ? z1 : z0;
    uint16_t state = feat_get(fc, x, y, z);
    if (t_is_air(state) || state == B_LAVA || state == B_WATER) count++;
  }
  return count;
}

static void t_foss_place_template(FeatureCtx *fc, LegacyRng *r, const FossTpl *tpl, int rotation, int target_x,
                                  int target_y, int target_z, int chunk_x0, int chunk_x1, int chunk_z0,
                                  int chunk_z1, int max_y, int block, int diamonds_as_diamond_ore,
                                  float integrity) {
  if (block < 0) return;
  int diamond = block_id_for_name("minecraft:deepslate_diamond_ore");
  for (int i = 0; i < tpl->count; i++) {
    int packed = FOSS_POS[tpl->first + i];
    int bx = packed & 15, by = (packed >> 4) & 7, bz = (packed >> 8) & 15;
    int wx = target_x + t_foss_transform_x(bx, bz, rotation);
    int wy = target_y + by;
    int wz = target_z + t_foss_transform_z(bx, bz, rotation);
    if (wx < chunk_x0 || wx > chunk_x1 || wz < chunk_z0 || wz > chunk_z1 || wy < MIN_Y || wy > max_y) continue;
    /* StructureTemplate.processBlockInfos runs the processors in list order:
       block_rot, then (diamonds only) the rule, then protected_blocks. */
    if (!(legacy_next_float(r) <= integrity)) continue;
    int place = block;
    if (diamonds_as_diamond_ore && block == (int)B_COAL && diamond >= 0) place = diamond;
    if (block_tag_contains("minecraft:features_cannot_replace", feat_get(fc, wx, wy, wz))) continue;
    feat_set(fc, wx, wy, wz, (uint16_t)place);
  }
}

static int feature_fossil(FeatureCtx *fc, Jv *config) {
  LegacyRng *r = feat_rng(fc);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  int rotation = legacy_next_bound(r, 4); /* Rotation.getRandom: NONE, CW90, CW180, CCW90 */
  Jv *fossil_structures = jget(config, "fossil_structures");
  Jv *overlay_structures = jget(config, "overlay_structures");
  int nstruct = jarr_len(fossil_structures);
  if (nstruct <= 0) return 0;
  int fossil_index = legacy_next_bound(r, nstruct);
  const char *base_name = jstr(jget_idx(fossil_structures, fossil_index));
  const char *overlay_name = jstr(jget_idx(overlay_structures, fossil_index));
  int ti = t_foss_template_index(base_name);
  if (ti < 0 || !overlay_name) return 0;
  const FossTpl *base = &FOSS_BASE[ti];
  const FossTpl *overlay = &FOSS_OVERLAY[ti];
  int sx = base->sx, sy = base->sy, sz = base->sz;
  int rotated = (rotation == FOSS_ROT_CW90 || rotation == FOSS_ROT_CCW90);
  int rsx = rotated ? sz : sx;
  int rsz = rotated ? sx : sz;

  int low_x = ox - rsx / 2, low_z = oz - rsz / 2;
  int lowest_surface = oy;
  for (int xs = 0; xs < rsx; xs++) {
    for (int zs = 0; zs < rsz; zs++) {
      int h = feat_height(fc, low_x + xs, low_z + zs, FEAT_HM_OCEAN_FLOOR_WG);
      if (h < lowest_surface) lowest_surface = h;
    }
  }
  int target_y = t_max_i(lowest_surface - 15 - legacy_next_bound(r, 10), MIN_Y + 10);

  /* StructureTemplate.getZeroPositionWithTransform(lowCorner.atY(targetY), NONE,
     rotation) uses the UNROTATED size. */
  int zx = 0, zz = 0;
  switch (rotation) {
    case FOSS_ROT_CW90: zx = sz - 1; break;
    case FOSS_ROT_CW180: zx = sx - 1; zz = sz - 1; break;
    case FOSS_ROT_CCW90: zz = sx - 1; break;
    default: break;
  }
  int target_x = low_x + zx, target_z = low_z + zz;

  int c1x = t_foss_transform_x(0, 0, rotation), c1z = t_foss_transform_z(0, 0, rotation);
  int c2x = t_foss_transform_x(sx - 1, sz - 1, rotation), c2z = t_foss_transform_z(sx - 1, sz - 1, rotation);
  int box_x0 = target_x + t_min_i(c1x, c2x), box_x1 = target_x + t_max_i(c1x, c2x);
  int box_z0 = target_z + t_min_i(c1z, c2z), box_z1 = target_z + t_max_i(c1z, c2z);
  int box_y0 = target_y, box_y1 = target_y + sy - 1;
  int max_empty = (int)jnum(jget(config, "max_empty_corners_allowed"), 4);
  if (t_foss_count_empty_corners(fc, box_x0, box_y0, box_z0, box_x1, box_y1, box_z1) > max_empty) return 0;

  /* settings.boundingBox: the generating chunk +- 16 blocks, full build height. */
  int chunk_x0 = jfloor_div(ox, 16) * 16 - 16;
  int chunk_x1 = jfloor_div(ox, 16) * 16 + 31;
  int chunk_z0 = jfloor_div(oz, 16) * 16 - 16;
  int chunk_z1 = jfloor_div(oz, 16) * 16 + 31;
  int max_y = MIN_Y + HEIGHT - 1;

  const char *overlay_proc = jstr(jget(config, "overlay_processors"));
  int diamonds_as_diamond_ore = overlay_proc && strstr(overlay_proc, "fossil_diamonds") != 0;

  /* fossil_rot: block_rot integrity 0.9 + protected_blocks.
     fossil_coal / fossil_diamonds: block_rot integrity 0.1 + [rule] + protected. */
  t_foss_place_template(fc, r, base, rotation, target_x, target_y, target_z, chunk_x0, chunk_x1, chunk_z0, chunk_z1,
                        max_y, block_id_for_name("minecraft:bone_block"), 0, 0.9F);
  t_foss_place_template(fc, r, overlay, rotation, target_x, target_y, target_z, chunk_x0, chunk_x1, chunk_z0,
                        chunk_z1, max_y, block_id_for_name("minecraft:coal_ore"), diamonds_as_diamond_ore, 0.1F);
  return 1;
}

/* ======================================================= registration ====== */

typedef struct {
  const char *type;
  FeatureFn fn;
} TerrainFeatureEntry;

FeatureFn feature_terrain_lookup(const char *type) {
  static const TerrainFeatureEntry entries[] = {
      {"ore", feature_ore},
      {"disk", feature_disk},
      {"spring_feature", feature_spring},
      {"lake", feature_lake},
      {"underwater_magma", feature_underwater_magma},
      {"block_blob", feature_block_blob},
      {"geode", feature_geode},
      {"large_dripstone", feature_large_dripstone},
      {"speleothem_cluster", feature_speleothem_cluster},
      {"sculk_patch", feature_sculk_patch},
      {"iceberg", feature_iceberg},
      {"blue_ice", feature_blue_ice},
      {"fossil", feature_fossil},
      {"monster_room", feature_monster_room},
      {"desert_well", feature_desert_well},
      {"spike", feature_spike},
      {"sequence", feature_sequence},
      {"random_boolean_selector", feature_random_boolean_selector},
  };
  if (!type) return 0;
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  for (size_t i = 0; i < sizeof entries / sizeof entries[0]; i++) {
    if (!strcmp(type, entries[i].type)) return entries[i].fn;
  }
  return 0;
}
