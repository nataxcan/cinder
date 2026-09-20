// The noise stage: biomes, density, aquifer, ore veins, heightmaps.
//
// Mirrors NoiseBasedChunkGenerator.doFill + NoiseChunk + Aquifer +
// OreVeinifier. Order matters and is kept: aquifer first, ore veins only where
// the aquifer reports solid, default block (stone) last.
/* Bend inlines this file into a temporary directory, so sibling includes are
 * absolute - the same convention the generated embed.h include already uses. */
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H


#define WAY_BELOW_MIN_Y (-32512) /* DimensionType.WAY_BELOW_MIN_Y */

/* ------------------------------------------------------- preliminary surface */

static int prelim_lookup(ChunkEval *ce, uint64_t key, int *value) {
  PrelimCache *c = &ce->prelim;
  if (!c->cap) return 0;
  unsigned long h = ((unsigned long)key * 0x9E3779B97F4A7C15UL) >> 40;
  for (int probe = 0; probe < 8; probe++) {
    unsigned long slot = (h + (unsigned long)probe) & (unsigned long)(c->cap - 1);
    if (!c->used[slot]) return 0;
    if (c->keys[slot] == key) {
      *value = c->vals[slot];
      return 1;
    }
  }
  return 0;
}

static void prelim_insert(PrelimCache *c, uint64_t key, int value) {
  unsigned long h = ((unsigned long)key * 0x9E3779B97F4A7C15UL) >> 40;
  for (int probe = 0; probe < 32; probe++) {
    unsigned long slot = (h + (unsigned long)probe) & (unsigned long)(c->cap - 1);
    if (!c->used[slot]) {
      c->used[slot] = 1;
      c->keys[slot] = key;
      c->vals[slot] = value;
      c->count++;
      return;
    }
    if (c->keys[slot] == key) {
      c->vals[slot] = value;
      return;
    }
  }
}

static void prelim_grow(PrelimCache *c) {
  int old_cap = c->cap;
  uint64_t *old_keys = c->keys;
  int *old_vals = c->vals;
  unsigned char *old_used = c->used;
  c->cap = old_cap * 2;
  c->keys = (uint64_t *)malloc((size_t)c->cap * sizeof(uint64_t));
  c->vals = (int *)malloc((size_t)c->cap * sizeof(int));
  c->used = (unsigned char *)calloc((size_t)c->cap, 1);
  c->count = 0;
  for (int i = 0; i < old_cap; i++) {
    if (old_used[i]) prelim_insert(c, old_keys[i], old_vals[i]);
  }
  free(old_keys);
  free(old_vals);
  free(old_used);
}

static void prelim_store(ChunkEval *ce, uint64_t key, int value) {
  PrelimCache *c = &ce->prelim;
  if (!c->cap) {
    c->cap = 2048;
    c->keys = (uint64_t *)malloc((size_t)c->cap * sizeof(uint64_t));
    c->vals = (int *)malloc((size_t)c->cap * sizeof(int));
    c->used = (unsigned char *)calloc((size_t)c->cap, 1);
    c->count = 0;
  } else if ((c->count + 1) * 10 >= c->cap * 7) {
    prelim_grow(c);
  }
  prelim_insert(c, key, value);
}

// NoiseChunk.preliminarySurfaceLevel: quantized to 4-block columns and cached.
int prelim_surface_level(Gen *g, ChunkEval *ce, int block_x, int block_z) {
  /* Java's QuartPos.fromBlock/toBlock: quantize to 4-block columns.
   * ColumnPos.asLong(x, z) = (x & 0xffffffff) | ((z & 0xffffffff) << 32), in
   * unsigned arithmetic so the shift is defined. */
  int qx = (block_x >> 2) * 4;
  int qz = (block_z >> 2) * 4;
  uint64_t key = (uint64_t)(uint32_t)qx | ((uint64_t)(uint32_t)qz << 32);
  int cached;
  if (prelim_lookup(ce, key, &cached)) return cached;
  int value = jfloor(df_eval_at(g, ce, g->prelim, qx, 0, qz));
  prelim_store(ce, key, value);
  return value;
}

/* ------------------------------------------------------------------ aquifer */

typedef struct {
  int fluid_level;
  uint16_t fluid_type; /* B_WATER or B_LAVA */
} FluidStatus;

struct Aquifer {
  Gen *g;
  ChunkEval *ce;
  DfNode *barrier, *flood, *spread, *lava, *eros, *depth;
  XoroFactory random_factory;
  int min_grid_x, min_grid_y, min_grid_z, grid_size_x, grid_size_y, grid_size_z;
  int skip_sampling_above_y;
  FluidStatus *cache;
  int *loc_x, *loc_y, *loc_z;
  unsigned char *loc_set;
  int should_update;
};

static FluidStatus global_fluid(int y) {
  FluidStatus s;
  if (y < -54) {
    s.fluid_level = -54;
    s.fluid_type = B_LAVA;
  } else {
    s.fluid_level = SEA;
    s.fluid_type = B_WATER;
  }
  return s;
}

static inline uint16_t fluid_at(FluidStatus s, int y) { return y < s.fluid_level ? s.fluid_type : B_AIR; }

Aquifer *aquifer_create(Gen *g, ChunkEval *ce) {
  Aquifer *a = (Aquifer *)calloc(1, sizeof(Aquifer));
  a->g = g;
  a->ce = ce;
  a->barrier = g->barrier;
  a->flood = g->flood;
  a->spread = g->spread;
  a->lava = g->lava;
  a->eros = g->eros;
  a->depth = g->depth_d;
  a->random_factory = g->aquifer_random;

  int chunk_min_x = ce->cx * 16, chunk_min_z = ce->cz * 16;
  int chunk_max_x = chunk_min_x + 15, chunk_max_z = chunk_min_z + 15;
  a->min_grid_x = ((chunk_min_x - 5) >> 4) + 0;
  int max_grid_x = ((chunk_max_x - 5) >> 4) + 1;
  a->grid_size_x = max_grid_x - a->min_grid_x + 1;
  a->min_grid_y = jfloor_div(MIN_Y + 1, 12) + (-1);
  int max_grid_y = jfloor_div(MIN_Y + HEIGHT + 1, 12) + 1;
  a->grid_size_y = max_grid_y - a->min_grid_y + 1;
  a->min_grid_z = ((chunk_min_z - 5) >> 4) + 0;
  int max_grid_z = ((chunk_max_z - 5) >> 4) + 1;
  a->grid_size_z = max_grid_z - a->min_grid_z + 1;

  int total = a->grid_size_x * a->grid_size_y * a->grid_size_z;
  a->cache = (FluidStatus *)calloc((size_t)total, sizeof(FluidStatus));
  a->loc_x = (int *)calloc((size_t)total, sizeof(int));
  a->loc_y = (int *)calloc((size_t)total, sizeof(int));
  a->loc_z = (int *)calloc((size_t)total, sizeof(int));
  a->loc_set = (unsigned char *)calloc((size_t)total, 1);

  int from_grid_x_min = a->min_grid_x * 16;
  int from_grid_z_min = a->min_grid_z * 16;
  int from_grid_x_max = max_grid_x * 16 + 9;
  int from_grid_z_max = max_grid_z * 16 + 9;
  int max_prelim = INT32_MIN;
  for (int bz = from_grid_z_min; bz <= from_grid_z_max; bz += 4) {
    for (int bx = from_grid_x_min; bx <= from_grid_x_max; bx += 4) {
      int level = prelim_surface_level(g, ce, bx, bz);
      if (level > max_prelim) max_prelim = level;
    }
  }
  int max_adjusted = max_prelim + 8;
  int skip_grid_y = jfloor_div(max_adjusted + 12, 12) - (-1);
  a->skip_sampling_above_y = skip_grid_y * 12 + 11 - 1;
  return a;
}

void aquifer_free(Aquifer *a) {
  if (!a) return;
  free(a->cache);
  free(a->loc_x);
  free(a->loc_y);
  free(a->loc_z);
  free(a->loc_set);
  free(a);
}

int aquifer_schedule_update(Aquifer *a) { return a ? a->should_update : 0; }

static inline int aq_index(Aquifer *a, int gx, int gy, int gz) {
  int x = gx - a->min_grid_x;
  int y = gy - a->min_grid_y;
  int z = gz - a->min_grid_z;
  return (y * a->grid_size_z + z) * a->grid_size_x + x;
}

static FluidStatus compute_aquifer_fluid(Aquifer *a, int x, int y, int z);

static FluidStatus get_aquifer_status(Aquifer *a, int index) {
  /* fluid_type is B_WATER or B_LAVA for every computed status, so 0 means
   * "not materialised yet". */
  if (a->cache[index].fluid_type) return a->cache[index];
  FluidStatus s = compute_aquifer_fluid(a, a->loc_x[index], a->loc_y[index], a->loc_z[index]);
  a->cache[index] = s;
  return s;
}

static const int SURFACE_SAMPLING_OFFSETS[13][2] = {
    {0, 0}, {-2, -1}, {-1, -1}, {0, -1}, {1, -1}, {-3, 0}, {-2, 0},
    {-1, 0}, {1, 0}, {-2, 1}, {-1, 1}, {0, 1}, {1, 1}};

static int compute_randomized_fluid_surface_level(Aquifer *a, int x, int y, int z, int lowest_prelim) {
  int cell_x = jfloor_div(x, 16);
  int cell_y = jfloor_div(y, 40);
  int cell_z = jfloor_div(z, 16);
  int cell_middle_y = cell_y * 40 + 20;
  double spread = df_eval_at(a->g, a->ce, a->spread, cell_x, cell_y, cell_z) * 10.0;
  int quantized = jfloor(spread / 3.0) * 3;
  int target = cell_middle_y + quantized;
  return lowest_prelim < target ? lowest_prelim : target;
}

static uint16_t compute_fluid_type(Aquifer *a, int x, int y, int z, FluidStatus global, int level) {
  uint16_t type = global.fluid_type;
  if (level <= -10 && level != WAY_BELOW_MIN_Y && global.fluid_type != B_LAVA) {
    int cell_x = jfloor_div(x, 64);
    int cell_y = jfloor_div(y, 40);
    int cell_z = jfloor_div(z, 64);
    double lava_noise = df_eval_at(a->g, a->ce, a->lava, cell_x, cell_y, cell_z);
    if (fabs(lava_noise) > 0.3) type = B_LAVA;
  }
  return type;
}

static int compute_surface_level(Aquifer *a, int x, int y, int z, FluidStatus global,
                                 int lowest_prelim, int center_under_fluid) {
  double partially, fully;
  int deep_dark = df_eval_at(a->g, a->ce, a->eros, x, y, z) < -0.225 && 
                  df_eval_at(a->g, a->ce, a->depth, x, y, z) > 0.9;
  if (deep_dark) {
    partially = -1.0;
    fully = -1.0;
  } else {
    int distance_below_surface = lowest_prelim + 8 - y;
    double floodedness_factor =
        center_under_fluid ? jclamped_map((double)distance_below_surface, 0.0, 64.0, 1.0, 0.0) : 0.0;
    double floodedness = jclamp(df_eval_at(a->g, a->ce, a->flood, x, y, z), -1.0, 1.0);
    double fully_threshold = jlerp(jinverse_lerp(floodedness_factor, 1.0, 0.0), -0.3, 0.8);
    double partially_threshold = jlerp(jinverse_lerp(floodedness_factor, 1.0, 0.0), -0.8, 0.4);
    partially = floodedness - partially_threshold;
    fully = floodedness - fully_threshold;
  }
  if (fully > 0.0) return global.fluid_level;
  if (partially > 0.0) return compute_randomized_fluid_surface_level(a, x, y, z, lowest_prelim);
  return WAY_BELOW_MIN_Y;
}

static FluidStatus compute_aquifer_fluid(Aquifer *a, int x, int y, int z) {
  FluidStatus global = global_fluid(y);
  int lowest_prelim = INT32_MAX;
  int top_of_cell = y + 12;
  int bottom_of_cell = y - 12;
  int center_under_fluid = 0;
  for (int i = 0; i < 13; i++) {
    int sx = x + SURFACE_SAMPLING_OFFSETS[i][0] * 16;
    int sz = z + SURFACE_SAMPLING_OFFSETS[i][1] * 16;
    int prelim = prelim_surface_level(a->g, a->ce, sx, sz);
    int adjusted = prelim + 8;
    int is_start = SURFACE_SAMPLING_OFFSETS[i][0] == 0 && SURFACE_SAMPLING_OFFSETS[i][1] == 0;
    if (is_start && bottom_of_cell > adjusted) return global;
    int pokes_above = top_of_cell > adjusted;
    if (pokes_above || is_start) {
      FluidStatus at_surface = global_fluid(adjusted);
      if (fluid_at(at_surface, adjusted) != B_AIR) {
        if (is_start) center_under_fluid = 1;
        if (pokes_above) return at_surface;
      }
    }
    if (prelim < lowest_prelim) lowest_prelim = prelim;
  }
  int level = compute_surface_level(a, x, y, z, global, lowest_prelim, center_under_fluid);
  FluidStatus out;
  out.fluid_level = level;
  out.fluid_type = compute_fluid_type(a, x, y, z, global, level);
  return out;
}

static inline double similarity(int d1, int d2) { return 1.0 - (double)(d2 - d1) / 25.0; }

static double calculate_pressure(Aquifer *a, int x, int y, int z, double *barrier_cache,
                                 FluidStatus s1, FluidStatus s2) {
  uint16_t type1 = fluid_at(s1, y);
  uint16_t type2 = fluid_at(s2, y);
  int lava_water = (type1 == B_LAVA && type2 == B_WATER) || (type1 == B_WATER && type2 == B_LAVA);
  if (lava_water) return 2.0;
  int fluid_y_diff = abs(s1.fluid_level - s2.fluid_level);
  if (fluid_y_diff == 0) return 0.0;
  double average_fluid_y = 0.5 * (double)(s1.fluid_level + s2.fluid_level);
  double how_far_above = (double)y + 0.5 - average_fluid_y;
  double base_value = (double)fluid_y_diff / 2.0;
  double distance_from_edge = base_value - fabs(how_far_above);
  double gradient;
  if (how_far_above > 0.0) {
    gradient = distance_from_edge > 0.0 ? distance_from_edge / 1.5 : distance_from_edge / 2.5;
  } else {
    double center_point = 3.0 + distance_from_edge;
    gradient = center_point > 0.0 ? center_point / 3.0 : center_point / 10.0;
  }
  double noise_value;
  if (gradient < -2.0 || gradient > 2.0) {
    noise_value = 0.0;
  } else {
    if (isnan(*barrier_cache)) {
      *barrier_cache = df_eval_at(a->g, a->ce, a->barrier, x, y, z);
    }
    noise_value = *barrier_cache;
  }
  return 2.0 * (noise_value + gradient);
}

int aquifer_substance(Aquifer *a, ChunkEval *ce, int pos_x, int pos_y, int pos_z, double density,
                      uint16_t *out) {
  (void)ce;
  if (density > 0.0) {
    a->should_update = 0;
    return 0;
  }
  FluidStatus global = global_fluid(pos_y);
  if (pos_y > a->skip_sampling_above_y) {
    a->should_update = 0;
    *out = fluid_at(global, pos_y);
    return 1;
  }
  if (fluid_at(global, pos_y) == B_LAVA) {
    a->should_update = 0;
    *out = B_LAVA;
    return 1;
  }

  int x_anchor = (pos_x - 5) >> 4;
  int y_anchor = jfloor_div(pos_y + 1, 12);
  int z_anchor = (pos_z - 5) >> 4;
  int d1 = INT32_MAX, d2 = INT32_MAX, d3 = INT32_MAX, d4 = INT32_MAX;
  int i1 = 0, i2 = 0, i3 = 0, i4 = 0;
  for (int x1 = 0; x1 <= 1; x1++) {
    for (int y1 = -1; y1 <= 1; y1++) {
      for (int z1 = 0; z1 <= 1; z1++) {
        int gx = x_anchor + x1, gy = y_anchor + y1, gz = z_anchor + z1;
        int index = aq_index(a, gx, gy, gz);
        int lx, ly, lz;
        if (a->loc_set[index]) {
          lx = a->loc_x[index];
          ly = a->loc_y[index];
          lz = a->loc_z[index];
        } else {
          Xoro r;
          xoro_at(a->random_factory, gx, gy, gz, &r);
          lx = gx * 16 + xoro_next_bound(&r, 10);
          ly = gy * 12 + xoro_next_bound(&r, 9);
          lz = gz * 16 + xoro_next_bound(&r, 10);
          a->loc_set[index] = 1;
          a->loc_x[index] = lx;
          a->loc_y[index] = ly;
          a->loc_z[index] = lz;
        }
        int dx = lx - pos_x, dy = ly - pos_y, dz = lz - pos_z;
        int new_distance = dx * dx + dy * dy + dz * dz;
        if (d1 >= new_distance) {
          i4 = i3; i3 = i2; i2 = i1; i1 = index;
          d4 = d3; d3 = d2; d2 = d1; d1 = new_distance;
        } else if (d2 >= new_distance) {
          i4 = i3; i3 = i2; i2 = index;
          d4 = d3; d3 = d2; d2 = new_distance;
        } else if (d3 >= new_distance) {
          i4 = i3; i3 = index;
          d4 = d3; d3 = new_distance;
        } else if (d4 >= new_distance) {
          i4 = index;
          d4 = new_distance;
        }
      }
    }
  }

  FluidStatus s1 = get_aquifer_status(a, i1);
  double sim12 = similarity(d1, d2);
  uint16_t fluid_state = fluid_at(s1, pos_y);
  if (sim12 <= 0.0) {
    if (sim12 >= similarity(100, 144)) { /* FLOWING_UPDATE_SIMULARITY */
      FluidStatus s2 = get_aquifer_status(a, i2);
      a->should_update = !(s1.fluid_level == s2.fluid_level && s1.fluid_type == s2.fluid_type);
    } else {
      a->should_update = 0;
    }
    *out = fluid_state;
    return 1;
  }

  if (fluid_state == B_WATER && fluid_at(global_fluid(pos_y - 1), pos_y - 1) == B_LAVA) {
    a->should_update = 1;
    *out = fluid_state;
    return 1;
  }

  double barrier_cache = NAN;
  FluidStatus s2 = get_aquifer_status(a, i2);
  double barrier12 = sim12 * calculate_pressure(a, pos_x, pos_y, pos_z, &barrier_cache, s1, s2);
  if (density + barrier12 > 0.0) {
    a->should_update = 0;
    return 0;
  }
  FluidStatus s3 = get_aquifer_status(a, i3);
  double sim13 = similarity(d1, d3);
  if (sim13 > 0.0) {
    double barrier13 = sim12 * sim13 * calculate_pressure(a, pos_x, pos_y, pos_z, &barrier_cache, s1, s3);
    if (density + barrier13 > 0.0) {
      a->should_update = 0;
      return 0;
    }
  }
  double sim23 = similarity(d2, d3);
  if (sim23 > 0.0) {
    double barrier23 = sim12 * sim23 * calculate_pressure(a, pos_x, pos_y, pos_z, &barrier_cache, s2, s3);
    if (density + barrier23 > 0.0) {
      a->should_update = 0;
      return 0;
    }
  }
  int may_flow12 = !(s1.fluid_level == s2.fluid_level && s1.fluid_type == s2.fluid_type);
  int may_flow23 = sim23 >= similarity(100, 144) && !(s2.fluid_level == s3.fluid_level && s2.fluid_type == s3.fluid_type);
  int may_flow13 = sim13 >= similarity(100, 144) && !(s1.fluid_level == s3.fluid_level && s1.fluid_type == s3.fluid_type);
  if (!may_flow12 && !may_flow23 && !may_flow13) {
    FluidStatus s4 = get_aquifer_status(a, i4);
    a->should_update = sim13 >= similarity(100, 144) && similarity(d1, d4) >= similarity(100, 144) &&
                       !(s1.fluid_level == s4.fluid_level && s1.fluid_type == s4.fluid_type);
  } else {
    a->should_update = 1;
  }
  *out = fluid_state;
  return 1;
}

/* ------------------------------------------------------------ ore veinifier */

#define VEIN_COPPER 0
#define VEIN_IRON 1

static uint16_t vein_ore(int type) { return type == VEIN_COPPER ? B_COPPER : B_DS_IRON; }
static uint16_t vein_raw(int type) { return type == VEIN_COPPER ? B_RAW_COPPER : B_RAW_IRON; }
static uint16_t vein_filler(int type) { return type == VEIN_COPPER ? B_GRANITE : B_TUFF; }
static int vein_min_y(int type) { return type == VEIN_COPPER ? 0 : -60; }
static int vein_max_y(int type) { return type == VEIN_COPPER ? 50 : -8; }

/* 1 = wrote a block (ore/filler/raw), 0 = no vein here (default block). */
static int ore_veinifier(Gen *g, ChunkEval *ce, int x, int y, int z, uint16_t *out) {
  double toggle = df_eval_at(g, ce, g->vein_toggle, x, y, z);
  int type = toggle > 0.0 ? VEIN_COPPER : VEIN_IRON;
  double ridged_abs = fabs(toggle);
  int distance_from_top = vein_max_y(type) - y;
  int distance_from_bottom = y - vein_min_y(type);
  if (distance_from_bottom < 0 || distance_from_top < 0) return 0;
  int distance_from_edge = distance_from_top < distance_from_bottom ? distance_from_top : distance_from_bottom;
  double roundoff = jclamped_map((double)distance_from_edge, 0.0, 20.0, -0.2, 0.0);
  if (ridged_abs + roundoff < 0.4) return 0;
  Xoro r;
  xoro_at(g->ore_random, x, y, z, &r);
  if (xoro_next_float(&r) > 0.7f) return 0;
  if (df_eval_at(g, ce, g->vein_ridged, x, y, z) >= 0.0) return 0;
  double richness = jclamped_map(ridged_abs, 0.4, 0.6, 0.1, 0.3);
  if (xoro_next_float(&r) < richness && df_eval_at(g, ce, g->vein_gap, x, y, z) > -0.3) {
    *out = xoro_next_float(&r) < 0.02f ? vein_raw(type) : vein_ore(type);
    return 1;
  }
  *out = vein_filler(type);
  return 1;
}

/* --------------------------------------------------------------- heightmaps */

/* Heightmap.update: `hm` stores the first available Y (top + 1), starting at MIN_Y.
 * `kind` selects the vanilla predicate: HM_NOT_AIR or HM_MOTION_BLOCKING. */
void hm_update(int32_t *hm, Chunk *c, int lx, int ly, int lz, int kind) {
  int i = lz * 16 + lx;
  int first_available = hm[i];
  if (ly <= first_available - 2) return;
  if (kind == HM_NOT_AIR ? getb(c, lx, ly, lz) != B_AIR : blocks_motion(getb(c, lx, ly, lz))) {
    if (ly >= first_available) hm[i] = ly + 1;
    return;
  }
  if (first_available - 1 == ly) {
    for (int y = ly - 1; y >= MIN_Y; y--) {
      uint16_t b = getb(c, lx, y, lz);
      if (kind == HM_NOT_AIR ? b != B_AIR : blocks_motion(b)) {
        hm[i] = y + 1;
        return;
      }
    }
    hm[i] = MIN_Y;
  }
}

/* ------------------------------------------------------------------- stage */

static void fill_chunk_blocks(Gen *g, Chunk *c, ChunkEval *ce) {
  Aquifer *a = ce->aquifer;
  for (int cell_x = 0; cell_x < 4; cell_x++) {
    for (int cell_z = 0; cell_z < 4; cell_z++) {
      for (int cell_y = 47; cell_y >= 0; cell_y--) {
        for (int y_in_cell = 7; y_in_cell >= 0; y_in_cell--) {
          int y = MIN_Y + cell_y * CELL_Y + y_in_cell;
          for (int x_in_cell = 0; x_in_cell < 4; x_in_cell++) {
            int lx = cell_x * 4 + x_in_cell;
            int x = c->cx * 16 + lx;
            for (int z_in_cell = 0; z_in_cell < 4; z_in_cell++) {
              int lz = cell_z * 4 + z_in_cell;
              int z = c->cz * 16 + lz;
              double density = df_eval_at(g, ce, g->final_d, x, y, z);
              uint16_t block = B_AIR;
              int solid = 1;
              if (a) solid = !aquifer_substance(a, ce, x, y, z, density, &block);
              if (solid) {
                block = B_STONE;
                if (g->ore_veins) {
                  uint16_t vein = 0;
                  if (ore_veinifier(g, ce, x, y, z, &vein)) block = vein;
                }
              }
              c->b[idx(lx, y, lz)] = block;
              if (block != B_AIR) {
                hm_update(c->hm_ocean_floor, c, lx, y, lz, HM_MOTION_BLOCKING);
                hm_update(c->hm_world_surface, c, lx, y, lz, HM_NOT_AIR);
              }
            }
          }
        }
      }
    }
  }
}

void gen_fill_noise(Gen *g, Chunk *c, ChunkEval *ce) {
  biome_fill_chunk(g, c, ce);
  if (ce->aquifer) {
    aquifer_free(ce->aquifer);
    ce->aquifer = 0;
  }
  ce->aquifer = aquifer_create(g, ce);
  fill_chunk_blocks(g, c, ce);
}
