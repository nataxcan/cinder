// Feature stage driver: the decoration loop, the 3x3 window and the accessors.
//
// Mirrors net.minecraft.world.level.chunk.ChunkGenerator.applyBiomeDecoration
// (plus ChunkStatusTasks.generateFeatures and FeatureSorter), with a 3x3 chunk
// window standing in for vanilla's WorldGenRegion: the FEATURES step is declared
// with blockStateWriteRadius(1) and requires CARVERS at radius 1, so features may
// and do write into the eight neighbouring chunks.
//
// RNG-order contract (every feature consumes the same WorldgenRandom, so this has
// to be exact):
//   * FeatureSorter assigns each placed feature a global index in first-encounter
//     order over the biome source's possible biomes (for the overworld that is
//     the multi-noise parameter list order) x steps x list order, then a DFS over
//     the "next feature in the same biome step" edges produces the per-step order.
//   * Per step, the features reachable from the window's biomes are placed in
//     ascending STEP-LOCAL index order with
//     WorldgenRandom.setFeatureSeed(decorationSeed, indexInStepList, step).
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H

#ifndef CINDER_INC_VANILLA_FEATURE_COMMON_H
#define CINDER_INC_VANILLA_FEATURE_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_common.h"
#endif
#include CINDER_INC_VANILLA_FEATURE_COMMON_H

#ifndef CINDER_INC_VANILLA_BIOMES_H
#define CINDER_INC_VANILLA_BIOMES_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_biomes.h"
#endif
#include CINDER_INC_VANILLA_BIOMES_H

/* ------------------------------------------------------- block predicates -- */

int feat_can_replace(uint16_t b) {
  switch (b) {
    case B_AIR:
    case B_WATER:
    case B_LAVA:
    case B_SHORT_GRASS:
    case B_FERN:
    case B_TALL_GRASS:
    case B_LARGE_FERN:
    case B_DEAD_BUSH:
    case B_BUSH:
    case B_FIREFLY_BUSH:
    case B_SHORT_DRY_GRASS:
    case B_TALL_DRY_GRASS:
    case B_KELP:
    case B_SEAGRASS:
    case B_LILY:
    case B_GLOW_LICHEN:
    case B_SUGAR_CANE:
    case B_SNOW:
    case B_MOSS_CARPET:
    case B_PINK_PETALS:
    case B_CAVE_VINES:
    case B_CAVE_VINES_PLANT:
    case B_HANGING_ROOTS:
    case B_HANGING_MOSS:
    case B_GLOW_BERRIES:
    case B_SWEET_BERRY_BUSH:
    case B_LEAF_LITTER:
    case B_COBWEB:
      return 1;
    default:
      return 0;
  }
}

int feat_is_fluid(uint16_t b) { return b == B_WATER || b == B_LAVA; }

/* BlockBehaviour.BlockStateBase.isCollisionShapeFullBlock: full cubes only. */
int feat_is_collision_full(uint16_t b) {
  switch (b) {
    case B_AIR:
    case B_WATER:
    case B_LAVA:
    case B_SNOW:
    case B_SHORT_GRASS:
    case B_FERN:
    case B_TALL_GRASS:
    case B_LARGE_FERN:
    case B_DEAD_BUSH:
    case B_BUSH:
    case B_FIREFLY_BUSH:
    case B_SHORT_DRY_GRASS:
    case B_TALL_DRY_GRASS:
    case B_KELP:
    case B_SEAGRASS:
    case B_LILY:
    case B_GLOW_LICHEN:
    case B_SUGAR_CANE:
    case B_MOSS_CARPET:
    case B_PINK_PETALS:
    case B_CAVE_VINES:
    case B_CAVE_VINES_PLANT:
    case B_HANGING_ROOTS:
    case B_HANGING_MOSS:
    case B_GLOW_BERRIES:
    case B_SWEET_BERRY_BUSH:
    case B_LEAF_LITTER:
    case B_COBWEB:
    case B_SANDSTONE_SLAB:
    case B_POINTED_DRIPSTONE:
    case B_SPORE_BLOSSOM:
    case B_ALLIUM:
    case B_AZURE_BLUET:
    case B_BLUE_ORCHID:
    case B_CORNFLOWER:
    case B_DANDELION:
    case B_LILY_OF_THE_VALLEY:
    case B_ORANGE_TULIP:
    case B_OXEYE_DAISY:
    case B_PINK_TULIP:
    case B_POPPY:
    case B_RED_TULIP:
    case B_CLOSED_EYEBLOSSOM:
    case B_SUNFLOWER:
    case B_LILAC:
    case B_PEONY:
    case B_ROSE_BUSH:
      return 0;
    default:
      return 1;
  }
}

int feat_is_face_sturdy(uint16_t b, int dir) {
  (void)dir;
  return feat_is_collision_full(b);
}

int feat_occludes_face(uint16_t b, int dir) {
  (void)dir;
  return feat_is_collision_full(b);
}

int feat_is_solid_ground(FeatureCtx *fc, int x, int y, int z) {
  uint16_t b = feat_get(fc, x, y, z);
  return b != B_AIR && !feat_is_fluid(b);
}

/* ------------------------------------------------------------ window I/O -- */

static FeatureChunk *feat_slot(FeatureCtx *fc, int x, int z) {
  int dx = (x >> 4) - fc->chunk_cx, dz = (z >> 4) - fc->chunk_cz;
  if (dx < -1 || dx > 1 || dz < -1 || dz > 1) return 0;
  FeatureChunk *slot = fc->win[dz + 1][dx + 1];
  if (!slot || !slot->live) return 0;
  return slot;
}

int feat_in_window(FeatureCtx *fc, int x, int y, int z) {
  if (y < MIN_Y || y >= MIN_Y + HEIGHT) return 0;
  return feat_slot(fc, x, z) != 0;
}

uint16_t feat_get(FeatureCtx *fc, int x, int y, int z) {
  if (y < MIN_Y || y >= MIN_Y + HEIGHT) return B_AIR;
  FeatureChunk *slot = feat_slot(fc, x, z);
  if (!slot) return B_AIR;
  return slot->b[idx(x & 15, y, z & 15)];
}

static int feat_hm_predicate(uint16_t b, int hm) {
  switch (hm) {
    case FEAT_HM_WORLD_SURFACE_WG:
    case FEAT_HM_WORLD_SURFACE:
      return b != B_AIR;
    case FEAT_HM_OCEAN_FLOOR_WG:
    case FEAT_HM_OCEAN_FLOOR:
    case FEAT_HM_MOTION_BLOCKING:
      return blocks_motion(b);
    default:
      return blocks_motion(b) && !is_leaves(b);
  }
}

/* ProtoChunk.setBlockState updates every heightmap primed at this status. */
static void feat_hm_update(FeatureChunk *slot, int lx, int ly, int lz, uint16_t block, int hm) {
  int32_t *map = &slot->hm[(size_t)hm * 256];
  int i = lz * 16 + lx;
  int opaque = feat_hm_predicate(block, hm);
  int first_available = map[i];
  if (ly <= first_available - 2) return;
  if (opaque) {
    if (ly >= first_available) map[i] = ly + 1;
    return;
  }
  if (first_available - 1 == ly) {
    for (int y = ly - 1; y >= MIN_Y; y--) {
      if (feat_hm_predicate(slot->b[idx(lx, y, lz)], hm)) {
        map[i] = y + 1;
        return;
      }
    }
    map[i] = MIN_Y;
  }
}

int feat_set(FeatureCtx *fc, int x, int y, int z, uint16_t block) {
  if (y < MIN_Y || y >= MIN_Y + HEIGHT) return 0;
  FeatureChunk *slot = feat_slot(fc, x, z);
  if (!slot) return 0;
  int lx = x & 15, lz = z & 15;
  int i = idx(lx, y, lz);
  if (slot->b[i] == block) return 0;
  slot->b[i] = block;
  for (int hm = 0; hm < FEAT_HM_COUNT; hm++) {
    if (slot->hm_primed[hm]) feat_hm_update(slot, lx, y, lz, block, hm);
  }
  return 1;
}

static void feat_hm_prime(FeatureChunk *slot, int hm) {
  int32_t *map = &slot->hm[(size_t)hm * 256];
  for (int lz = 0; lz < 16; lz++) {
    for (int lx = 0; lx < 16; lx++) {
      int first = MIN_Y;
      for (int y = MIN_Y + HEIGHT - 1; y >= MIN_Y; y--) {
        if (feat_hm_predicate(slot->b[idx(lx, y, lz)], hm)) {
          first = y + 1;
          break;
        }
      }
      map[lz * 16 + lx] = first;
    }
  }
  slot->hm_primed[hm] = 1;
}

int feat_height(FeatureCtx *fc, int x, int z, int hm) {
  FeatureChunk *slot = feat_slot(fc, x, z);
  if (!slot) return MIN_Y;
  if (hm < 0 || hm >= FEAT_HM_COUNT) hm = FEAT_HM_WORLD_SURFACE_WG;
  if (!slot->hm_primed[hm]) feat_hm_prime(slot, hm);
  /* the maps are stored one per type, [hm][256]; reading without the type offset
   * always answered with OCEAN_FLOOR_WG (which counts leaves), so every
   * MOTION_BLOCKING_NO_LEAVES / WORLD_SURFACE query saw the wrong column. */
  return slot->hm[(size_t)hm * 256 + (z & 15) * 16 + (x & 15)];
}

int feat_biome(FeatureCtx *fc, int x, int y, int z) {
  FeatureChunk *slot = feat_slot(fc, x, z);
  if (!slot) return BIO_PLAINS;
  /* level.getBiome(pos) answers for any Y: ChunkAccess.getNoiseBiome clamps the
   * quart Y to the sections the chunk actually has. A placement modifier may
   * legally ask about y outside the build range (below_top anchors resolve
   * relative to the dimension), so clamp here rather than index out of bounds. */
  if (y < MIN_Y) y = MIN_Y;
  if (y >= MIN_Y + HEIGHT) y = MIN_Y + HEIGHT - 1;
  return slot->biome[biome_idx(x & 15, y, z & 15)];
}

LegacyRng *feat_rng(FeatureCtx *fc) { return &fc->rng; }
int feat_min_y(FeatureCtx *fc) { (void)fc; return MIN_Y; }
int feat_gen_depth(FeatureCtx *fc) { (void)fc; return HEIGHT; }

/* ------------------------------------------------------------- RNG seeds -- */

uint64_t feature_decoration_seed(Gen *g, int origin_x, int origin_z) {
  LegacyRng r;
  memset(&r, 0, sizeof r);
  r.kind = LEGACY_RNG_XORO;
  legacy_set_seed(&r, g->world_seed);
  int64_t x_scale = (int64_t)legacy_next_long(&r) | 1LL;
  int64_t z_scale = (int64_t)legacy_next_long(&r) | 1LL;
  return (uint64_t)((int64_t)origin_x * x_scale + (int64_t)origin_z * z_scale) ^ g->world_seed;
}

static void feature_set_seed(LegacyRng *r, uint64_t seed, int index, int step) {
  r->kind = LEGACY_RNG_XORO;
  legacy_set_seed(r, seed + (uint64_t)index + (uint64_t)(10000 * step));
}

#ifndef CINDER_INC_VANILLA_FEATURE_TERRAIN_C
#define CINDER_INC_VANILLA_FEATURE_TERRAIN_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_terrain.c"
#endif
#ifndef CINDER_INC_VANILLA_FEATURE_VEGETATION_C
#define CINDER_INC_VANILLA_FEATURE_VEGETATION_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_vegetation.c"
#endif

/* ------------------------------------------------------- feature dispatch -- */

/* Weak fallbacks: the family files are included above when present; the weak
 * definitions keep the TU linkable while a family is still being written. */
#if defined(__has_include)
#if !__has_include(CINDER_INC_VANILLA_FEATURE_TERRAIN_C)
__attribute__((weak)) FeatureFn feature_terrain_lookup(const char *type) {
  (void)type;
  return 0;
}
#endif
#if !__has_include(CINDER_INC_VANILLA_FEATURE_VEGETATION_C)
__attribute__((weak)) FeatureFn feature_vegetation_lookup(const char *type) {
  (void)type;
  return 0;
}
#endif
#endif

FeatureFn feature_lookup(const char *type) {
  FeatureFn fn = feature_terrain_lookup(type);
  if (fn) return fn;
  return feature_vegetation_lookup(type);
}

/* --------------------------------------------------------------- sorter -- */

// FeatureSorter.buildFeaturesPerStep over the biome source's possible biomes.
// The overworld source's possible biomes are exactly the parameter-list entries,
// in list order, so the global index assignment is deterministic.
typedef struct {
  char name[96]; /* embed key "pf:<name>" */
  int global_index;
  int step;
} FeatureOrderEnt;

typedef struct SorterAdj {
  int *to;
  int n;
  int cap;
} SorterAdj;

typedef struct {
  FeatureOrderEnt *all;
  int nall;
  /* per step: the ordered feature list (step-local index = position) */
  FeatureOrderEnt **steps;
  int *step_counts;
  int nsteps;
  struct SorterAdj *adj; /* per-node successor set, TreeSet-ordered */
} FeatureSorter;

static FeatureSorter SORT;

static int sorter_index_of(const char *name) {
  for (int i = 0; i < SORT.nall; i++) {
    if (!strcmp(SORT.all[i].name, name)) return i;
  }
  return -1;
}

static void sorter_assign(const char *name, int step, int *next_index) {
  if (sorter_index_of(name) >= 0) return;
  if (SORT.nall >= 8192) return;
  snprintf(SORT.all[SORT.nall].name, sizeof SORT.all[0].name, "%s", name);
  SORT.all[SORT.nall].global_index = (*next_index)++;
  SORT.all[SORT.nall].step = step;
  SORT.nall++;
}

/* FeatureSorter.buildFeaturesPerStep, ported literally.
 *
 *   Map<FeatureData, Set<FeatureData>> edges = new TreeMap<>(byStepThenIndex);
 *   for each biome, for each step, for each feature in that step's list:
 *     data = new FeatureData(globalIndex.computeIfAbsent(feature, k -> next++), step, feature)
 *     allFeatures.add(data)                       // one list per biome
 *   for j in 0..allFeatures.size()-1:             // consecutive-in-biome edges
 *     edges.computeIfAbsent(allFeatures[j]).add(allFeatures[j+1])
 *   for node in edges.keySet() (sorted):          // DFS
 *     if !visited: depthFirstSearch(..., result::add, node)
 *   Collections.reverse(result);
 *   per step: result.filter(step == s).map(feature)
 *
 * A node can have SEVERAL successors: a feature that appears in two biomes may
 * be followed by a different feature in each, and both edges are in the set.
 * That is what makes the DFS order (and therefore every feature's index within
 * its step, and therefore its setFeatureSeed) what vanilla computes. */

static void sorter_adj_add(SorterAdj *a, int to) {
  for (int i = 0; i < a->n; i++) {
    if (a->to[i] == to) return; /* Set semantics */
  }
  if (a->n == a->cap) {
    a->cap = a->cap ? a->cap * 2 : 4;
    a->to = (int *)realloc(a->to, (size_t)a->cap * sizeof(int));
  }
  a->to[a->n++] = to;
}

/* TreeSet order: by step, then by global index. */
static int sorter_cmp(const void *pa, const void *pb) {
  int a = *(const int *)pa, b = *(const int *)pb;
  if (SORT.all[a].step != SORT.all[b].step) return SORT.all[a].step - SORT.all[b].step;
  return SORT.all[a].global_index - SORT.all[b].global_index;
}

/* Graph.depthFirstSearch over the adjacency, successors in TreeSet order. */
static void sorter_dfs(int node, unsigned char *visited, unsigned char *visiting, int *out, int *nout) {
  if (visited[node] || visiting[node]) return;
  visiting[node] = 1;
  SorterAdj *adj = &SORT.adj[node];
  /* the adjacency is kept in TreeSet order */
  for (int i = 0; i < adj->n; i++) {
    int next = adj->to[i];
    if (visited[next]) continue;
    if (visiting[next]) continue; /* cycle: vanilla throws; Cinder skips */
    sorter_dfs(next, visited, visiting, out, nout);
  }
  visiting[node] = 0;
  visited[node] = 1;
  out[(*nout)++] = node;
}

/* Built once, before any concurrent decoration: sorter_build allocates and
 * parses, and its lazy guard is not thread-safe. */
void feat_sorter_init(Gen *g);

static void sorter_build(Gen *g) {
  static int ready;
  if (ready) return;
  ready = 1;
  memset(&SORT, 0, sizeof SORT);
  SORT.all = (FeatureOrderEnt *)calloc(8192, sizeof(FeatureOrderEnt));
  SORT.steps = (FeatureOrderEnt **)calloc(64, sizeof(FeatureOrderEnt *));
  SORT.step_counts = (int *)calloc(64, sizeof(int));
  SORT.adj = (SorterAdj *)calloc(8192, sizeof(SorterAdj));

  int next_index = 0;
  int *all_features = (int *)calloc(8192, sizeof(int)); /* one biome's list */
  int max_steps = 0;

  /* possibleBiomes() = the parameter list's values, in list order */
  for (int p = 0; p < BIOME_PARAM_COUNT; p++) {
    int biome = BIOME_PARAMS[p].biome;
    const char *bname = biome_name(biome);
    const char *colon = strchr(bname, ':');
    char key[128];
    snprintf(key, sizeof key, "biome:%s", colon ? colon + 1 : bname);
    Jv *body = embed_json(g, key);
    if (!body) continue;
    Jv *steps = jget(body, "features");
    int nall = 0;
    int step_index = 0;
    for (Jv *step_list = steps ? steps->child : 0; step_list; step_list = step_list->next) {
      for (Jv *ref = step_list->child; ref; ref = ref->next) {
        if (ref->kind != J_STR) continue;
        const char *name = ref->str;
        if (!strncmp(name, "minecraft:", 10)) name += 10;
        char pf_key[160];
        snprintf(pf_key, sizeof pf_key, "pf:%s", name);
        sorter_assign(pf_key, step_index, &next_index);
        int idx = sorter_index_of(pf_key);
        if (idx >= 0 && nall < 8192) all_features[nall++] = idx;
      }
      step_index++;
    }
    if (step_index > max_steps) max_steps = step_index;
    /* consecutive entries within this biome's list become edges */
    for (int j = 0; j + 1 < nall; j++) {
      sorter_adj_add(&SORT.adj[all_features[j]], all_features[j + 1]);
    }
  }
  free(all_features);

  SORT.nsteps = max_steps > 64 ? 64 : max_steps;

  /* the adjacency lists are TreeSets: sort each by (step, global index) */
  for (int i = 0; i < SORT.nall; i++) {
    if (SORT.adj[i].n > 1) {
      qsort(SORT.adj[i].to, (size_t)SORT.adj[i].n, sizeof(int), sorter_cmp);
    }
  }

  /* DFS over the keys in TreeMap order, then reverse */
  unsigned char *visited = (unsigned char *)calloc((size_t)SORT.nall, 1);
  unsigned char *visiting = (unsigned char *)calloc((size_t)SORT.nall, 1);
  int *order = (int *)calloc((size_t)SORT.nall, sizeof(int));
  int norder = 0;
  int *keys = (int *)calloc((size_t)SORT.nall, sizeof(int));
  for (int i = 0; i < SORT.nall; i++) keys[i] = i;
  qsort(keys, (size_t)SORT.nall, sizeof(int), sorter_cmp);
  for (int k = 0; k < SORT.nall; k++) {
    if (!visited[keys[k]]) sorter_dfs(keys[k], visited, visiting, order, &norder);
  }
  for (int a = 0, b = norder - 1; a < b; a++, b--) {
    int t = order[a];
    order[a] = order[b];
    order[b] = t;
  }

  for (int i = 0; i < SORT.nsteps; i++) {
    SORT.steps[i] = (FeatureOrderEnt *)calloc((size_t)(norder ? norder : 1), sizeof(FeatureOrderEnt));
    SORT.step_counts[i] = 0;
  }
  /* per step, in the reversed DFS order */
  for (int i = 0; i < norder; i++) {
    int node = order[i];
    int step = SORT.all[node].step;
    if (step < 0 || step >= SORT.nsteps) continue;
    SORT.steps[step][SORT.step_counts[step]++] = SORT.all[node];
  }
  free(keys);
  free(order);
  free(visited);
  free(visiting);
}

/* BiomeFilter: the placed feature being applied must appear in the biome's
 * feature lists (vanilla's BiomeGenerationSettings.hasFeature). */
int feat_biome_has_feature(FeatureCtx *fc, int biome) {
  if (!fc->placed || !fc->placed_name) return 1;
  const char *bname = biome_name(biome);
  const char *colon = strchr(bname, ':');
  char key[128];
  snprintf(key, sizeof key, "biome:%s", colon ? colon + 1 : bname);
  Jv *body = embed_json(fc->g, key);
  if (!body) return 0;
  Jv *steps = jget(body, "features");
  for (Jv *step_list = steps ? steps->child : 0; step_list; step_list = step_list->next) {
    for (Jv *ref = step_list->child; ref; ref = ref->next) {
      if (ref->kind != J_STR) continue;
      const char *name = ref->str;
      if (!strncmp(name, "minecraft:", 10)) name += 10;
      char pf_key[160];
      snprintf(pf_key, sizeof pf_key, "pf:%s", name);
      if (!strcmp(pf_key, fc->placed_name)) return 1;
    }
  }
  return 0;
}

/* --------------------------------------------------------------- driver -- */

typedef struct {
  int biome_seen[BIO_COUNT];
} BiomeSet;

static int window_has_biome(FeatureChunk *win[3][3], const BiomeSet *set, int biome) {
  (void)win;
  return set->biome_seen[biome];
}

static void collect_window_biomes(FeatureChunk *win[3][3], BiomeSet *set) {
  memset(set, 0, sizeof *set);
  for (int dz = 0; dz < 3; dz++) {
    for (int dx = 0; dx < 3; dx++) {
      FeatureChunk *slot = win[dz][dx];
      if (!slot || !slot->live) continue;
      for (int i = 0; i < BIOME_CELLS; i++) set->biome_seen[slot->biome[i]] = 1;
    }
  }
}

static int feat_debug = -1;

/* PlacedFeature.placeWithBiomeCheck with emit = apply the configured feature. */
static int place_emit(FeatureCtx *fc, int x, int y, int z) {
  /* Feature.place: `level.ensureCanWrite(origin) ? place(...) : false` - a
   * non-writable origin returns false WITHOUT consuming any RNG. */
  if (!feat_in_window(fc, x, y, z)) return 0;
  Jv *feature = jget(fc->placed, "feature");
  if (!feature) return 0;
  Jv *cf = feature;
  if (feature->kind == J_STR) {
    char key[160];
    const char *name = feature->str;
    if (!strncmp(name, "minecraft:", 10)) name += 10;
    snprintf(key, sizeof key, "cf:%s", name);
    cf = embed_json(fc->g, key);
    if (!cf) return 0;
  }
  const char *type = jstr(jget(cf, "type"));
  if (feat_debug) fprintf(stderr, "  call %s at %d %d %d\n", type ? type : "(null)", x, y, z);
  if (!type) return 0;
  FeatureFn fn = feature_lookup(type);
  if (!fn) {
    if (feat_debug) fprintf(stderr, "  no impl for %s\n", type);
    return 0;
  }
  fc->origin_x = x;
  fc->origin_y = y;
  fc->origin_z = z;
  int placed = fn(fc, jget(cf, "config"));
  if (feat_debug) fprintf(stderr, "  emit %s at %d %d %d -> %d\n", type, x, y, z, placed);
  return placed;
}

/* The emit callback feat_apply_placed uses for nested placed features: applies
 * the configured feature and records whether anything was placed, so the caller
 * can implement "try the next entry on failure" (random_selector). */
int feat_nested_emit(FeatureCtx *fc, int x, int y, int z) {
  int ok = place_emit(fc, x, y, z);
  if (ok) fc->nested_placed_any = 1;
  return ok;
}

void gen_features_window(Gen *g, ChunkEval *ce, FeatureChunk *win[3][3], FeatureChunk *centre) {
  sorter_build(g);
  if (feat_debug < 0) feat_debug = getenv("CINDER_FEAT_DEBUG") != 0;

  FeatureCtx fc;
  memset(&fc, 0, sizeof fc);
  fc.g = g;
  fc.ce = ce;
  for (int dz = 0; dz < 3; dz++) {
    for (int dx = 0; dx < 3; dx++) fc.win[dz][dx] = win[dz][dx];
  }
  fc.chunk_cx = centre->cx;
  fc.chunk_cz = centre->cz;

  /* ChunkStatusTasks.generateFeatures primes these four on the centre chunk. */
  for (int hm = FEAT_HM_MOTION_BLOCKING; hm <= FEAT_HM_WORLD_SURFACE; hm++) {
    if (!centre->hm_primed[hm]) feat_hm_prime(centre, hm);
  }

  BiomeSet window_biomes;
  collect_window_biomes(win, &window_biomes);

  int origin_x = centre->cx * 16, origin_z = centre->cz * 16;
  uint64_t decoration_seed = feature_decoration_seed(g, origin_x, origin_z);

  for (int step = 0; step < SORT.nsteps; step++) {
    for (int i = 0; i < SORT.step_counts[step]; i++) {
      FeatureOrderEnt *ent = &SORT.steps[step][i];
      /* the feature must be referenced by one of the window's biomes */
      Jv *placed = embed_json(g, ent->name);
      if (!placed) continue;
      /* biome check: at least one window biome lists this feature in this step */
      int referenced = 0;
      for (int b = 0; b < BIO_COUNT && !referenced; b++) {
        if (!window_has_biome(win, &window_biomes, b)) continue;
        const char *bname = biome_name(b);
        const char *colon = strchr(bname, ':');
        char key[128];
        snprintf(key, sizeof key, "biome:%s", colon ? colon + 1 : bname);
        Jv *body = embed_json(g, key);
        if (!body) continue;
        Jv *steps = jget(body, "features");
        Jv *step_list = jget_idx(steps, step);
        for (Jv *ref = step_list ? step_list->child : 0; ref; ref = ref->next) {
          if (ref->kind != J_STR) continue;
          const char *name = ref->str;
          if (!strncmp(name, "minecraft:", 10)) name += 10;
          char pf_key[160];
          snprintf(pf_key, sizeof pf_key, "pf:%s", name);
          if (!strcmp(pf_key, ent->name)) {
            referenced = 1;
            break;
          }
        }
      }
      if (!referenced) continue;
      if (feat_debug) fprintf(stderr, "dispatch step=%d i=%d %s\n", step, i, ent->name);
      feature_set_seed(&fc.rng, decoration_seed, i, step);
      fc.global_index = ent->global_index;
      fc.step = step;
      fc.placed = placed;
      fc.placed_name = ent->name;
      Jv *placement = jget(placed, "placement");
      place_chain(&fc, placement ? placement->child : 0, origin_x, MIN_Y, origin_z, place_emit);
    }
  }
}

/* Feature-family implementations (included last so they see the whole driver).
 * __has_include keeps the TU buildable while a family file is being written. */
#if defined(__has_include)
#if __has_include(CINDER_INC_VANILLA_FEATURE_TERRAIN_C)
#include CINDER_INC_VANILLA_FEATURE_TERRAIN_C
#endif
#if __has_include(CINDER_INC_VANILLA_FEATURE_VEGETATION_C)
#include CINDER_INC_VANILLA_FEATURE_VEGETATION_C
#endif
#else
#include CINDER_INC_VANILLA_FEATURE_TERRAIN_C
#include CINDER_INC_VANILLA_FEATURE_VEGETATION_C
#endif

/* See the declaration above sorter_build. */
void feat_sorter_init(Gen *g) { sorter_build(g); }
