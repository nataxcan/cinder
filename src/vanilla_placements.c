// Placement modifiers, providers and block predicates for the feature stage.
//
// Mirrors net.minecraft.world.level.levelgen.placement.* (the 15 modifier types
// the overworld uses), util/valueproviders IntProviders, the height providers
// and the block predicates the overworld's placed features reference.
//
// The chaining contract is vanilla's lazy stream: for each position produced by
// modifier i, modifier i+1 runs to completion before modifier i produces its
// next position. That is what keeps the RNG draw order identical, so the chain
// is implemented as a recursive walk instead of a materialised position list.
#ifndef CINDER_INC_VANILLA_FEATURE_COMMON_H
#define CINDER_INC_VANILLA_FEATURE_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_common.h"
#endif
#include CINDER_INC_VANILLA_FEATURE_COMMON_H

/* ------------------------------------------------------------- providers -- */

int feat_min_y(FeatureCtx *fc);
int feat_gen_depth(FeatureCtx *fc);
double biome_info_noise_value(Gen *g, double x, double z);
int placed_feature_matches_biome(FeatureCtx *fc, int biome);

// VerticalAnchor.resolveY. The JSON form is either a bare number (offset from
// the bottom) or an object with exactly one of absolute / above_bottom /
// below_top - the type tag is implicit in the key, not a "type" field.
static int anchor_resolve_y(Jv *anchor, int min_gen_y, int gen_depth) {
  if (!anchor) return min_gen_y;
  if (anchor->kind == J_NUM) return (int)jnum(anchor, 0) + min_gen_y;
  Jv *v;
  if ((v = jget(anchor, "absolute"))) return (int)jnum(v, 0);
  if ((v = jget(anchor, "above_bottom"))) return min_gen_y + (int)jnum(v, 0);
  if ((v = jget(anchor, "below_top"))) return min_gen_y + gen_depth - 1 - (int)jnum(v, 0);
  const char *type = jstr(jget(anchor, "type"));
  double value = jnum(jget(anchor, "value"), 0);
  if (!type) return (int)value + min_gen_y;
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  if (!strcmp(type, "absolute")) return (int)value;
  if (!strcmp(type, "above_bottom")) return min_gen_y + (int)value;
  if (!strcmp(type, "below_top")) return min_gen_y + gen_depth - 1 - (int)value;
  return (int)value;
}

static int height_provider_sample(Jv *h, LegacyRng *r, int min_gen_y, int gen_depth) {
  if (!h) return min_gen_y;
  const char *type = jstr(jget(h, "type"));
  if (!type) return (int)jnum(h, 0) + min_gen_y;
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  if (!strcmp(type, "constant")) return anchor_resolve_y(jget(h, "value"), min_gen_y, gen_depth);
  if (!strcmp(type, "uniform")) {
    int lo = anchor_resolve_y(jget(h, "min_inclusive"), min_gen_y, gen_depth);
    int hi = anchor_resolve_y(jget(h, "max_inclusive"), min_gen_y, gen_depth);
    if (lo > hi) return lo;
    return lo + legacy_next_bound(r, hi - lo + 1);
  }
  if (!strcmp(type, "trapezoid")) {
    int lo = anchor_resolve_y(jget(h, "min_inclusive"), min_gen_y, gen_depth);
    int hi = anchor_resolve_y(jget(h, "max_inclusive"), min_gen_y, gen_depth);
    if (lo > hi) return lo;
    int range = hi - lo;
    int plateau = (int)jnum(jget(h, "plateau"), 0);
    if (plateau >= range) return lo + legacy_next_bound(r, range + 1);
    int plateau_start = (range - plateau) / 2;
    int plateau_end = range - plateau_start;
    return lo + legacy_next_bound(r, plateau_end + 1) + legacy_next_bound(r, plateau_start + 1);
  }
  if (!strcmp(type, "very_biased_to_bottom")) {
    int lo = anchor_resolve_y(jget(h, "min_inclusive"), min_gen_y, gen_depth);
    int hi = anchor_resolve_y(jget(h, "max_inclusive"), min_gen_y, gen_depth);
    int inner = (int)jnum(jget(h, "inner"), 1);
    if (hi - lo - inner + 1 <= 0) return lo;
    int upper = lo + inner + legacy_next_bound(r, hi - (lo + inner) + 1);
    int biased_upper = lo + legacy_next_bound(r, upper - 1 - lo + 1);
    return lo + legacy_next_bound(r, biased_upper - 1 + inner - lo + 1);
  }
  if (!strcmp(type, "biased_to_bottom")) {
    int lo = anchor_resolve_y(jget(h, "min_inclusive"), min_gen_y, gen_depth);
    int hi = anchor_resolve_y(jget(h, "max_inclusive"), min_gen_y, gen_depth);
    int inner = (int)jnum(jget(h, "inner"), 1);
    if (hi - lo - inner + 1 <= 0) return lo;
    int limit = legacy_next_bound(r, hi - lo - inner + 1);
    return legacy_next_bound(r, limit + inner) + lo;
  }
  if (!strcmp(type, "weighted_list")) {
    Jv *dist = jget(h, "distribution");
    int total = 0;
    for (Jv *e = dist ? dist->child : 0; e; e = e->next) total += (int)jnum(jget(e, "weight"), 1);
    int pick = legacy_next_bound(r, total);
    for (Jv *e = dist ? dist->child : 0; e; e = e->next) {
      int w = (int)jnum(jget(e, "weight"), 1);
      if (pick < w) return height_provider_sample(jget(e, "data"), r, min_gen_y, gen_depth);
      pick -= w;
    }
    return min_gen_y;
  }
  return (int)jnum(h, 0) + min_gen_y;
}

static int int_provider_sample(Jv *p, LegacyRng *r, int min_gen_y, int gen_depth) {
  if (!p) return 1;
  if (p->kind == J_NUM) return (int)p->num;
  const char *type = jstr(jget(p, "type"));
  if (!type) return (int)jnum(p, 1);
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  if (!strcmp(type, "constant")) return (int)jnum(jget(p, "value"), 0);
  if (!strcmp(type, "uniform")) {
    int lo = (int)jnum(jget(p, "min_inclusive"), 0);
    int hi = (int)jnum(jget(p, "max_inclusive"), 0);
    if (lo > hi) return lo;
    return lo + legacy_next_bound(r, hi - lo + 1);
  }
  if (!strcmp(type, "biased_to_bottom")) {
    int lo = (int)jnum(jget(p, "min_inclusive"), 0);
    int hi = (int)jnum(jget(p, "max_inclusive"), 0);
    return lo + legacy_next_bound(r, legacy_next_bound(r, hi - lo + 1) + 1);
  }
  if (!strcmp(type, "trapezoid")) {
    int lo = (int)jnum(jget(p, "min_inclusive"), 0);
    int hi = (int)jnum(jget(p, "max_inclusive"), 0);
    int plateau = (int)jnum(jget(p, "plateau"), 0);
    if (plateau == 0 && hi == -lo) {
      return legacy_next_bound(r, hi + 1) - legacy_next_bound(r, hi + 1);
    }
    int range = hi - lo;
    if (plateau == range) return lo + legacy_next_bound(r, range + 1);
    int plateau_start = (range - plateau) / 2;
    int plateau_end = range - plateau_start;
    return lo + legacy_next_bound(r, plateau_end + 1) + legacy_next_bound(r, plateau_start + 1);
  }
  if (!strcmp(type, "clamped")) {
    int inner = int_provider_sample(jget(p, "source"), r, min_gen_y, gen_depth);
    int lo = (int)jnum(jget(p, "min_inclusive"), 0);
    int hi = (int)jnum(jget(p, "max_inclusive"), 0);
    if (inner < lo) return lo;
    return inner > hi ? hi : inner;
  }
  if (!strcmp(type, "clamped_normal")) {
    float mean = (float)jnum(jget(p, "mean"), 0);
    float dev = (float)jnum(jget(p, "deviation"), 1);
    float lo = (float)jnum(jget(p, "min_inclusive"), 0);
    float hi = (float)jnum(jget(p, "max_inclusive"), 0);
    float normal = mean + (float)legacy_next_gaussian(r) * dev;
    float clamped = normal < lo ? lo : (normal > hi ? hi : normal);
    return (int)clamped;
  }
  if (!strcmp(type, "weighted_list")) {
    Jv *dist = jget(p, "distribution");
    int total = 0;
    for (Jv *e = dist ? dist->child : 0; e; e = e->next) total += (int)jnum(jget(e, "weight"), 1);
    int pick = legacy_next_bound(r, total);
    for (Jv *e = dist ? dist->child : 0; e; e = e->next) {
      int w = (int)jnum(jget(e, "weight"), 1);
      if (pick < w) return int_provider_sample(jget(e, "data"), r, min_gen_y, gen_depth);
      pick -= w;
    }
    return 0;
  }
  return (int)jnum(p, 1);
}

/* -------------------------------------------------------- block predicates */

int feat_block_predicate(FeatureCtx *fc, Jv *p, int x, int y, int z) {
  if (!p) return 1;
  const char *type = jstr(jget(p, "type"));
  if (!type) return 1;
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  if (!strcmp(type, "true")) return 1;
  if (!strcmp(type, "all_of")) {
    Jv *list = jget(p, "predicates");
    for (Jv *c = list ? list->child : 0; c; c = c->next) {
      if (!feat_block_predicate(fc, c, x, y, z)) return 0;
    }
    return 1;
  }
  if (!strcmp(type, "any_of")) {
    Jv *list = jget(p, "predicates");
    for (Jv *c = list ? list->child : 0; c; c = c->next) {
      if (feat_block_predicate(fc, c, x, y, z)) return 1;
    }
    return 0;
  }
  if (!strcmp(type, "not")) return !feat_block_predicate(fc, jget(p, "predicate"), x, y, z);
  if (!strcmp(type, "inside_world_bounds")) return 1;
  if (!strcmp(type, "unobstructed")) return 1;

  /* StateTestingPredicate subclasses apply "offset" (Vec3i.offsetCodec, default
   * [0,0,0]) before reading the state - e.g. cf:disk_grass's {"type":"solid",
   * "offset":[0,1,0]} tests the block ABOVE the origin. */
  int ox = 0, oy = 0, oz = 0;
  Jv *offset = jget(p, "offset");
  if (offset && offset->kind == J_ARR) {
    ox = (int)jnum(jget_idx(offset, 0), 0);
    oy = (int)jnum(jget_idx(offset, 1), 0);
    oz = (int)jnum(jget_idx(offset, 2), 0);
  }
  int px = x + ox, py = y + oy, pz = z + oz;

  /* solid: BlockStateBase.isSolid() (legacySolid). Over Cinder's palette the
   * only blocks where it differs from "collision shape is a full cube" are the
   * sandstone slab and the snow layer, and both are already non-full. */
  if (!strcmp(type, "solid")) return feat_is_collision_full(feat_get(fc, px, py, pz));
  if (!strcmp(type, "replaceable")) return feat_can_replace(feat_get(fc, px, py, pz));
  if (!strcmp(type, "matching_blocks")) {
    uint16_t here = feat_get(fc, px, py, pz);
    Jv *list = jget(p, "blocks");
    for (Jv *c = list ? list->child : 0; c; c = c->next) {
      const char *name = c->kind == J_STR ? c->str : jstr(jget(c, "Name"));
      int id = block_id_for_name(name);
      if (id >= 0 && id == here) return 1;
    }
    return 0;
  }
  if (!strcmp(type, "matching_block_tag")) {
    const char *tag = jstr(jget(p, "tag"));
    return tag && block_tag_contains(tag, feat_get(fc, px, py, pz));
  }
  if (!strcmp(type, "matching_fluids")) {
    uint16_t here = feat_get(fc, px, py, pz);
    Jv *list = jget(p, "fluids");
    for (Jv *c = list ? list->child : 0; c; c = c->next) {
      const char *name = c->kind == J_STR ? c->str : jstr(jget(c, "Name"));
      int id = block_id_for_name(name);
      if (id >= 0 && id == here) return 1;
    }
    return 0;
  }
  if (!strcmp(type, "would_survive")) {
    Jv *state = jget(p, "state");
    const char *name = state ? (state->kind == J_STR ? state->str : jstr(jget(state, "Name"))) : 0;
    int id = block_id_for_name(name);
    return id >= 0 && feat_would_survive(fc, (uint16_t)id, px, py, pz);
  }
  return 1;
}


/* ------------------------------------------------ public helper wrappers -- */

int feat_int_provider(Jv *p, LegacyRng *r) { return int_provider_sample(p, r, MIN_Y, HEIGHT); }

/* FloatProvider.sample (uniform / trapezoid / clamped_normal / constant) */
double feat_float_provider(Jv *p, LegacyRng *r) {
  if (!p) return 0.0;
  if (p->kind == J_NUM) return p->num;
  const char *type = jstr(jget(p, "type"));
  if (!type) return jnum(p, 0.0);
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  if (!strcmp(type, "constant")) return jnum(jget(p, "value"), 0.0);
  if (!strcmp(type, "uniform")) {
    float lo = (float)jnum(jget(p, "min_inclusive"), 0.0);
    float hi = (float)jnum(jget(p, "max_exclusive"), 1.0);
    return (double)(lo + legacy_next_float(r) * (hi - lo));
  }
  if (!strcmp(type, "trapezoid")) {
    float lo = (float)jnum(jget(p, "min"), 0.0);
    float hi = (float)jnum(jget(p, "max"), 1.0);
    float plateau = (float)jnum(jget(p, "plateau"), 0.0);
    float range = hi - lo;
    if (range <= 0.0f) return (double)lo;
    if (plateau >= range) return (double)(lo + legacy_next_float(r) * range);
    float plateau_start = (range - plateau) / 2.0f;
    float plateau_end = range - plateau_start;
    return (double)(lo + legacy_next_float(r) * plateau_end + legacy_next_float(r) * plateau_start);
  }
  if (!strcmp(type, "clamped_normal")) {
    float mean = (float)jnum(jget(p, "mean"), 0.0);
    float dev = (float)jnum(jget(p, "deviation"), 1.0);
    float lo = (float)jnum(jget(p, "min"), 0.0);
    float hi = (float)jnum(jget(p, "max"), 1.0);
    float normal = mean + (float)legacy_next_gaussian(r) * dev;
    if (normal < lo) normal = lo;
    if (normal > hi) normal = hi;
    return (double)normal;
  }
  return jnum(p, 0.0);
}

/* PlacedFeature.place(level, generator, random, pos) for a nested placed
 * feature: `placed_json` is {"feature": <inline or cf: name>, "placement": []}. */
int feat_apply_placed(FeatureCtx *fc, Jv *placed_json, int x, int y, int z) {
  if (!placed_json) return 0;
  Jv *saved = fc->placed;
  int placed_any = 0;
  fc->placed = placed_json;
  Jv *placement = jget(placed_json, "placement");
  /* the emit callback (defined in vanilla_features.c) applies the configured
   * feature and records success through fc->nested_placed_any */
  fc->nested_placed_any = 0;
  place_chain(fc, placement ? placement->child : 0, x, y, z, feat_nested_emit);
  placed_any = fc->nested_placed_any;
  fc->placed = saved;
  return placed_any;
}

/* ----------------------------------------------------- placement modifiers */

static void place_chain_inner(FeatureCtx *fc, Jv *mod, int x, int y, int z, PlaceEmit emit);

// CountOnEveryLayerPlacement.findOnGroundYPosition
static int count_on_every_layer_y(FeatureCtx *fc, int x, int y_start, int z, int layer_to_place_on) {
  int current_layer = 0;
  uint16_t current = feat_get(fc, x, y_start, z);
  for (int y = y_start; y >= feat_min_y(fc) + 1; y--) {
    uint16_t below = feat_get(fc, x, y - 1, z);
    int below_empty = below == B_AIR || below == B_WATER || below == B_LAVA;
    int current_empty = current == B_AIR || current == B_WATER || current == B_LAVA;
    if (!below_empty && current_empty && below != B_BEDROCK) {
      if (current_layer == layer_to_place_on) return y;
      current_layer++;
    }
    current = below;
  }
  return INT32_MAX;
}

static int hm_kind(const char *name) {
  if (!name) return FEAT_HM_WORLD_SURFACE_WG;
  if (!strncmp(name, "minecraft:", 10)) name += 10;
  if (!strcmp(name, "ocean_floor_wg")) return FEAT_HM_OCEAN_FLOOR_WG;
  if (!strcmp(name, "world_surface_wg")) return FEAT_HM_WORLD_SURFACE_WG;
  if (!strcmp(name, "motion_blocking")) return FEAT_HM_MOTION_BLOCKING;
  if (!strcmp(name, "motion_blocking_no_leaves")) return FEAT_HM_MOTION_BLOCKING_NO_LEAVES;
  if (!strcmp(name, "world_surface")) return FEAT_HM_WORLD_SURFACE;
  if (!strcmp(name, "ocean_floor")) return FEAT_HM_OCEAN_FLOOR;
  return FEAT_HM_WORLD_SURFACE_WG;
}

static void dir_vec(const char *name, int *dx, int *dy, int *dz) {
  *dx = *dy = *dz = 0;
  if (!name) return;
  if (!strncmp(name, "minecraft:", 10)) name += 10;
  if (!strcmp(name, "down")) *dy = -1;
  else if (!strcmp(name, "up")) *dy = 1;
  else if (!strcmp(name, "north")) *dz = -1;
  else if (!strcmp(name, "south")) *dz = 1;
  else if (!strcmp(name, "west")) *dx = -1;
  else if (!strcmp(name, "east")) *dx = 1;
}

static void place_apply(FeatureCtx *fc, Jv *mod, int x, int y, int z, PlaceEmit emit) {
  LegacyRng *r = feat_rng(fc);
  const char *type = jstr(jget(mod, "type"));
  if (type && !strncmp(type, "minecraft:", 10)) type += 10;
  Jv *next = mod->next;
  const int min_y = feat_min_y(fc);
  const int depth = feat_gen_depth(fc);
  if (!type) {
    place_chain_inner(fc, next, x, y, z, emit);
    return;
  }
  if (!strcmp(type, "count")) {
    int n = int_provider_sample(jget(mod, "count"), r, min_y, depth);
    for (int i = 0; i < n; i++) place_chain_inner(fc, next, x, y, z, emit);
    return;
  }
  if (!strcmp(type, "count_on_every_layer")) {
    int n = int_provider_sample(jget(mod, "count"), r, min_y, depth);
    for (int layer = 0; layer < n; layer++) {
      int found = count_on_every_layer_y(fc, x, y, z, layer);
      if (found != INT32_MAX) place_chain_inner(fc, next, x, found, z, emit);
    }
    return;
  }
  if (!strcmp(type, "noise_threshold_count")) {
    double level = jnum(jget(mod, "noise_level"), 0);
    int below = (int)jnum(jget(mod, "below_noise"), 0);
    int above = (int)jnum(jget(mod, "above_noise"), 0);
    double n = biome_info_noise_value(fc->g, (double)x / 200.0, (double)z / 200.0);
    int count = n < level ? below : above;
    for (int i = 0; i < count; i++) place_chain_inner(fc, next, x, y, z, emit);
    return;
  }
  if (!strcmp(type, "noise_based_count")) {
    int ratio = (int)jnum(jget(mod, "noise_to_count_ratio"), 1);
    double factor = jnum(jget(mod, "noise_factor"), 1);
    double offset = jnum(jget(mod, "noise_offset"), 0);
    double n = biome_info_noise_value(fc->g, (double)x / factor, (double)z / factor);
    int count = (int)ceil((n + offset) * (double)ratio);
    for (int i = 0; i < count; i++) place_chain_inner(fc, next, x, y, z, emit);
    return;
  }
  if (!strcmp(type, "in_square")) {
    /* InSquarePlacement.modifier: pos.offset(rng.nextInt(16), 0, rng.nextInt(16)) -
     * relative to the position the chain is currently at, which for the first
     * modifier is the chunk origin. */
    int nx = x + legacy_next_bound(r, 16);
    int nz = z + legacy_next_bound(r, 16);
    if (getenv("CINDER_SQUARE_DEBUG")) fprintf(stderr, "  square %d %d\n", nx, nz);
    place_chain_inner(fc, next, nx, y, nz, emit);
    return;
  }
  if (!strcmp(type, "height_range")) {
    int ny = height_provider_sample(jget(mod, "height"), r, min_y, depth);
    place_chain_inner(fc, next, x, ny, z, emit);
    return;
  }
  if (!strcmp(type, "heightmap")) {
    int h = feat_height(fc, x, z, hm_kind(jstr(jget(mod, "heightmap"))));
    if (h > min_y) place_chain_inner(fc, next, x, h, z, emit);
    return;
  }
  if (!strcmp(type, "random_offset")) {
    int dx = int_provider_sample(jget(mod, "xz_spread"), r, min_y, depth);
    int dy = int_provider_sample(jget(mod, "y_spread"), r, min_y, depth);
    int dz = int_provider_sample(jget(mod, "xz_spread"), r, min_y, depth);
    place_chain_inner(fc, next, x + dx, y + dy, z + dz, emit);
    return;
  }
  if (!strcmp(type, "rarity_filter")) {
    int chance = (int)jnum(jget(mod, "chance"), 1);
    if (legacy_next_float(r) < 1.0F / (float)chance) place_chain_inner(fc, next, x, y, z, emit);
    return;
  }
  if (!strcmp(type, "biome")) {
    /* BiomeFilter: the top feature must be listed by the biome at the position. */
    if (feat_biome_has_feature(fc, feat_biome(fc, x, y, z))) {
      place_chain_inner(fc, next, x, y, z, emit);
    }
    return;
  }
  if (!strcmp(type, "block_predicate_filter")) {
    if (feat_block_predicate(fc, jget(mod, "predicate"), x, y, z)) place_chain_inner(fc, next, x, y, z, emit);
    return;
  }
  if (!strcmp(type, "surface_relative_threshold_filter")) {
    int surface = feat_height(fc, x, z, hm_kind(jstr(jget(mod, "heightmap"))));
    int lo = surface + (int)jnum(jget(mod, "min_inclusive"), INT32_MIN);
    int hi = surface + (int)jnum(jget(mod, "max_inclusive"), INT32_MAX);
    if (lo <= y && y <= hi) place_chain_inner(fc, next, x, y, z, emit);
    return;
  }
  if (!strcmp(type, "surface_water_depth_filter")) {
    int max_depth = (int)jnum(jget(mod, "max_water_depth"), 0);
    int ocean_floor = feat_height(fc, x, z, FEAT_HM_OCEAN_FLOOR);
    int world_surface = feat_height(fc, x, z, FEAT_HM_WORLD_SURFACE);
    if (world_surface - ocean_floor <= max_depth) place_chain_inner(fc, next, x, y, z, emit);
    return;
  }
  if (!strcmp(type, "environment_scan")) {
    int dx, dy, dz;
    dir_vec(jstr(jget(mod, "direction_of_search")), &dx, &dy, &dz);
    int max_steps = (int)jnum(jget(mod, "max_steps"), 1);
    Jv *target = jget(mod, "target_condition");
    Jv *allowed = jget(mod, "allowed_search_condition");
    int cx = x, cy = y, cz = z;
    if (allowed && !feat_block_predicate(fc, allowed, cx, cy, cz)) return;
    for (int i = 0; i < max_steps; i++) {
      if (feat_block_predicate(fc, target, cx, cy, cz)) {
        place_chain_inner(fc, next, cx, cy, cz, emit);
        return;
      }
      cx += dx;
      cy += dy;
      cz += dz;
      if (cy < min_y || cy >= min_y + depth) return;
      if (allowed && !feat_block_predicate(fc, allowed, cx, cy, cz)) break;
    }
    if (feat_block_predicate(fc, target, cx, cy, cz)) place_chain_inner(fc, next, cx, cy, cz, emit);
    return;
  }
  if (!strcmp(type, "fixed_placement")) {
    Jv *ys = jget(mod, "height");
    for (Jv *e = ys ? ys->child : 0; e; e = e->next) {
      place_chain_inner(fc, next, x, (int)jnum(e, min_y), z, emit);
    }
    return;
  }
  place_chain_inner(fc, next, x, y, z, emit);
}

static void place_chain_inner(FeatureCtx *fc, Jv *mod, int x, int y, int z, PlaceEmit emit) {
  if (!mod) {
    emit(fc, x, y, z);
    return;
  }
  place_apply(fc, mod, x, y, z, emit);
}

void place_chain(FeatureCtx *fc, Jv *mod, int x, int y, int z, PlaceEmit emit) {
  place_chain_inner(fc, mod, x, y, z, emit);
}
