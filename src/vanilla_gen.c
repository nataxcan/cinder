// Cinder vanilla 26.2 overworld Full-chunk generator.
//
// Stage order mirrors the vanilla chunk pipeline:
//   biomes -> noise + aquifers + ore veins -> surface -> carvers -> features
//   -> lighting
// Lighting is Cinder's own and deliberately outside block-level parity (vanilla
// recomputes light on chunk load and never writes it to disk).
//
// Everything is one translation unit; the stage implementations live in the
// files included at the bottom of this file.
#include "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla/embed.h"

#ifndef CINDER_INC_VANILLA_EMBED_CACHE_C
#define CINDER_INC_VANILLA_EMBED_CACHE_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_embed_cache.c"
#endif
#include CINDER_INC_VANILLA_EMBED_CACHE_C

/* Bend inlines this file into a temporary directory, so sibling includes are
 * absolute - the same convention the generated embed.h include already uses. */
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H


// The whole translation unit must be free of FP contraction (FMA): vanilla
// evaluates every lerp as separate multiply/add, and contracting changes the
// low bits of terrain heights.
#pragma STDC FP_CONTRACT OFF

/* ------------------------------------------------------------- block names */

const char *block_name(int block) {
  static const char *names[B_COUNT] = {
      "minecraft:air",
      "minecraft:stone",
      "minecraft:dirt",
      "minecraft:grass_block",
      "minecraft:bedrock",
      "minecraft:water",
      "minecraft:lava",
      "minecraft:sand",
      "minecraft:red_sand",
      "minecraft:sandstone",
      "minecraft:gravel",
      "minecraft:clay",
      "minecraft:coal_ore",
      "minecraft:iron_ore",
      "minecraft:copper_ore",
      "minecraft:gold_ore",
      "minecraft:redstone_ore",
      "minecraft:lapis_ore",
      "minecraft:diamond_ore",
      "minecraft:emerald_ore",
      "minecraft:deepslate",
      "minecraft:deepslate_coal_ore",
      "minecraft:deepslate_iron_ore",
      "minecraft:deepslate_copper_ore",
      "minecraft:deepslate_gold_ore",
      "minecraft:deepslate_redstone_ore",
      "minecraft:deepslate_lapis_ore",
      "minecraft:deepslate_diamond_ore",
      "minecraft:granite",
      "minecraft:diorite",
      "minecraft:andesite",
      "minecraft:tuff",
      "minecraft:calcite",
      "minecraft:smooth_basalt",
      "minecraft:amethyst_block",
      "minecraft:dripstone_block",
      "minecraft:moss_block",
      "minecraft:mud",
      "minecraft:packed_ice",
      "minecraft:ice",
      "minecraft:snow",
      "minecraft:powder_snow",
      "minecraft:terracotta",
      "minecraft:oak_log",
      "minecraft:oak_leaves",
      "minecraft:birch_log",
      "minecraft:birch_leaves",
      "minecraft:spruce_log",
      "minecraft:spruce_leaves",
      "minecraft:jungle_log",
      "minecraft:jungle_leaves",
      "minecraft:acacia_log",
      "minecraft:acacia_leaves",
      "minecraft:dark_oak_log",
      "minecraft:dark_oak_leaves",
      "minecraft:cherry_log",
      "minecraft:cherry_leaves",
      "minecraft:mangrove_log",
      "minecraft:mangrove_leaves",
      "minecraft:cactus",
      "minecraft:sugar_cane",
      "minecraft:pumpkin",
      "minecraft:kelp",
      "minecraft:seagrass",
      "minecraft:glow_lichen",
      "minecraft:podzol",
      "minecraft:mycelium",
      "minecraft:rooted_dirt",
      "minecraft:snow_block",
      "minecraft:magma_block",
      "minecraft:obsidian",
      "minecraft:infested_stone",
      "minecraft:short_grass",
      "minecraft:fern",
      "minecraft:dead_bush",
      "minecraft:lily_pad",
      "minecraft:raw_copper_block",
      "minecraft:raw_iron_block",
      "minecraft:allium",
      "minecraft:amethyst_cluster",
      "minecraft:azure_bluet",
      "minecraft:blue_ice",
      "minecraft:blue_orchid",
      "minecraft:brown_mushroom",
      "minecraft:budding_amethyst",
      "minecraft:bush",
      "minecraft:cactus_flower",
      "minecraft:cave_vines",
      "minecraft:cave_vines_plant",
      "minecraft:closed_eyeblossom",
      "minecraft:cornflower",
      "minecraft:dandelion",
      "minecraft:deepslate_emerald_ore",
      "minecraft:firefly_bush",
      "minecraft:hanging_roots",
      "minecraft:infested_deepslate",
      "minecraft:large_amethyst_bud",
      "minecraft:large_fern",
      "minecraft:leaf_litter",
      "minecraft:lilac",
      "minecraft:lily_of_the_valley",
      "minecraft:medium_amethyst_bud",
      "minecraft:melon",
      "minecraft:mossy_cobblestone",
      "minecraft:orange_tulip",
      "minecraft:oxeye_daisy",
      "minecraft:pale_moss_block",
      "minecraft:peony",
      "minecraft:pink_petals",
      "minecraft:pink_tulip",
      "minecraft:pointed_dripstone",
      "minecraft:poppy",
      "minecraft:potent_sulfur",
      "minecraft:red_mushroom",
      "minecraft:red_tulip",
      "minecraft:rose_bush",
      "minecraft:short_dry_grass",
      "minecraft:small_amethyst_bud",
      "minecraft:spore_blossom",
      "minecraft:sulfur",
      "minecraft:sulfur_spike",
      "minecraft:sunflower",
      "minecraft:sweet_berry_bush",
      "minecraft:tall_dry_grass",
      "minecraft:tall_grass",
      "minecraft:cobweb",
      "minecraft:spawner",
      "minecraft:cobblestone",
      "minecraft:moss_carpet",
      "minecraft:hanging_moss",
      "minecraft:glow_berries",
      "minecraft:cave_air",
      "minecraft:white_terracotta",
      "minecraft:orange_terracotta",
      "minecraft:yellow_terracotta",
      "minecraft:brown_terracotta",
      "minecraft:red_terracotta",
      "minecraft:light_gray_terracotta",
      "minecraft:coarse_dirt",
      "minecraft:red_sandstone",
      "minecraft:cinnabar",
      "minecraft:soul_soil",
      "minecraft:muddy_mangrove_roots",
      "minecraft:sculk",
      "minecraft:sculk_vein",
      "minecraft:sculk_catalyst",
      "minecraft:sculk_shrieker",
      "minecraft:suspicious_sand",
      "minecraft:sandstone_slab",
      "minecraft:bone_block",
      "minecraft:sculk_sensor",
      "minecraft:chest",
      "minecraft:farmland",
      "minecraft:tall_seagrass",
      "minecraft:kelp_plant",
      "minecraft:sea_pickle",
      "minecraft:bamboo",
      "minecraft:vine",
      "minecraft:cocoa",
      "minecraft:bee_nest",
      "minecraft:mangrove_roots",
      "minecraft:mangrove_propagule",
      "minecraft:azalea",
      "minecraft:azalea_leaves",
      "minecraft:flowering_azalea",
      "minecraft:flowering_azalea_leaves",
      "minecraft:big_dripleaf",
      "minecraft:big_dripleaf_stem",
      "minecraft:small_dripleaf",
      "minecraft:pale_oak_log",
      "minecraft:pale_oak_leaves",
      "minecraft:pale_moss_carpet",
      "minecraft:pale_hanging_moss",
      "minecraft:creaking_heart",
      "minecraft:crimson_roots",
      "minecraft:fire",
      "minecraft:soul_fire",
      "minecraft:white_tulip",
      "minecraft:wildflowers",
      "minecraft:void_air",
      "minecraft:oak_sapling",
      "minecraft:spruce_sapling",
      "minecraft:birch_sapling",
      "minecraft:jungle_sapling",
      "minecraft:acacia_sapling",
      "minecraft:dark_oak_sapling",
      "minecraft:cherry_sapling",
      "minecraft:pale_oak_sapling"
};
  if (block < 0 || block >= B_COUNT) return "minecraft:air";
  return names[block];
}

/* -------------------------------------------------------------------- json */

// The arena never moves (parsed Jv nodes stay live), so growth happens by
// chaining fixed-size blocks instead of reallocating.
#define ARENA_BLOCK (16u << 20)

void *agrow(Arena *a, size_t n) {
  // Keep every allocation 8-byte aligned so returned pointers are well-aligned.
  n = (n + 7u) & ~(size_t)7u;
  if (!a->arena || a->used + n > a->cap) {
    size_t block = n > ARENA_BLOCK ? n + 64 : ARENA_BLOCK;
    char *fresh = (char *)malloc(block);
    if (!fresh) {
      fprintf(stderr, "cinder: worldgen arena allocation failed (%zu bytes)\n", block);
      abort();
    }
    // link the new block behind the current one so older allocations stay valid
    if (a->arena) memcpy(fresh, a->arena, sizeof(char *));
    a->arena = fresh;
    a->cap = block;
    a->used = sizeof(char *);
  }
  void *p = a->arena + a->used;
  a->used += n;
  return p;
}

Jv *jnew(Arena *a) {
  Jv *v = (Jv *)agrow(a, sizeof(Jv));
  memset(v, 0, sizeof(*v));
  return v;
}

static const char *jskip(const char *s) {
  while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
  return s;
}

static const char *jparse_rec(Arena *a, const char *s, Jv **out) {
  s = jskip(s);
  Jv *v = jnew(a);
  *out = v;
  if (*s == '{') {
    v->kind = J_OBJ;
    s = jskip(s + 1);
    Jv **tail = &v->child;
    while (*s && *s != '}') {
      s = jskip(s);
      if (*s != '"') break;
      Jv *field;
      s = jparse_rec(a, s, &field);
      const char *key = field->str;
      int klen = field->slen;
      s = jskip(s);
      if (*s == ':') s++;
      Jv *val;
      s = jparse_rec(a, s, &val);
      val->key = key;
      val->klen = klen;
      *tail = val;
      tail = &val->next;
      s = jskip(s);
      if (*s == ',') s++;
    }
    if (*s == '}') s++;
    return s;
  }
  if (*s == '[') {
    v->kind = J_ARR;
    s = jskip(s + 1);
    Jv **tail = &v->child;
    while (*s && *s != ']') {
      Jv *el;
      s = jparse_rec(a, s, &el);
      *tail = el;
      tail = &el->next;
      s = jskip(s);
      if (*s == ',') s++;
    }
    if (*s == ']') s++;
    return s;
  }
  if (*s == '"') {
    v->kind = J_STR;
    s++;
    const char *b = s;
    while (*s && *s != '"') {
      if (*s == '\\') s++;
      s++;
    }
    int len = (int)(s - b);
    char *cpy = (char *)agrow(a, (size_t)len + 1);
    memcpy(cpy, b, (size_t)len);
    cpy[len] = 0;
    int w = 0;
    for (int r = 0; r < len; r++) {
      if (cpy[r] == '\\' && r + 1 < len) {
        r++;
        char e = cpy[r];
        cpy[w++] = (e == 'n') ? '\n' : (e == 't' ? '\t' : e);
      } else {
        cpy[w++] = cpy[r];
      }
    }
    cpy[w] = 0;
    v->str = cpy;
    v->slen = w;
    if (*s == '"') s++;
    return s;
  }
  if (!strncmp(s, "true", 4)) {
    v->kind = J_BOOL;
    v->num = 1;
    return s + 4;
  }
  if (!strncmp(s, "false", 5)) {
    v->kind = J_BOOL;
    v->num = 0;
    return s + 5;
  }
  if (!strncmp(s, "null", 4)) {
    v->kind = J_NULL;
    return s + 4;
  }
  v->kind = J_NUM;
  char *end = 0;
  v->num = strtod(s, &end);
  return end ? end : s + 1;
}

Jv *jparse(Arena *a, const char *s) {
  Jv *v = 0;
  jparse_rec(a, s, &v);
  return v;
}

Jv *jget(Jv *o, const char *k) {
  if (!o || o->kind != J_OBJ) return 0;
  size_t n = strlen(k);
  for (Jv *c = o->child; c; c = c->next) {
    if (c->klen == (int)n && memcmp(c->key, k, n) == 0) return c;
  }
  return 0;
}

const char *jstr(Jv *v) { return v && v->kind == J_STR ? v->str : 0; }

double jnum(Jv *v, double d) {
  if (!v) return d;
  if (v->kind == J_NUM) return v->num;
  if (v->kind == J_BOOL) return v->num;
  return d;
}

int jbool(Jv *v, int d) {
  if (!v) return d;
  if (v->kind == J_BOOL) return v->num != 0;
  if (v->kind == J_NUM) return v->num != 0;
  return d;
}

int jarr_len(Jv *v) {
  int n = 0;
  for (Jv *c = v && v->kind == J_ARR ? v->child : 0; c; c = c->next) n++;
  return n;
}

Jv *jget_idx(Jv *arr, int i) {
  if (!arr || arr->kind != J_ARR) return 0;
  int k = 0;
  for (Jv *c = arr->child; c; c = c->next, k++) {
    if (k == i) return c;
  }
  return 0;
}

/* ------------------------------------------------------------- stage files */

#ifndef CINDER_INC_VANILLA_RAND_NOISE_C
#define CINDER_INC_VANILLA_RAND_NOISE_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_rand_noise.c"
#endif
#include CINDER_INC_VANILLA_RAND_NOISE_C
#ifndef CINDER_INC_VANILLA_DF_C
#define CINDER_INC_VANILLA_DF_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_df.c"
#endif
#include CINDER_INC_VANILLA_DF_C
#ifndef CINDER_INC_VANILLA_BIOMES_C
#define CINDER_INC_VANILLA_BIOMES_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_biomes.c"
#endif
#include CINDER_INC_VANILLA_BIOMES_C

#ifndef CINDER_INC_VANILLA_BIOME_MANAGER_C
#define CINDER_INC_VANILLA_BIOME_MANAGER_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_biome_manager.c"
#endif
#include CINDER_INC_VANILLA_BIOME_MANAGER_C
#ifndef CINDER_INC_VANILLA_SURFACE_C
#define CINDER_INC_VANILLA_SURFACE_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_surface.c"
#endif
#include CINDER_INC_VANILLA_SURFACE_C
#ifndef CINDER_INC_VANILLA_CARVERS_C
#define CINDER_INC_VANILLA_CARVERS_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_carvers.c"
#endif
#include CINDER_INC_VANILLA_CARVERS_C
#ifndef CINDER_INC_VANILLA_FEATURE_HELPERS_C
#define CINDER_INC_VANILLA_FEATURE_HELPERS_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_helpers.c"
#endif
#ifndef CINDER_INC_VANILLA_PLACEMENTS_C
#define CINDER_INC_VANILLA_PLACEMENTS_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_placements.c"
#endif
#ifndef CINDER_INC_VANILLA_FEATURES_C
#define CINDER_INC_VANILLA_FEATURES_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_features.c"
#endif
/* helpers first (the placement layer uses them), then the placement chain, then
 * the driver with the feature families at its end */
#include CINDER_INC_VANILLA_FEATURE_HELPERS_C
#include CINDER_INC_VANILLA_PLACEMENTS_C
#include CINDER_INC_VANILLA_FEATURES_C
#ifndef CINDER_INC_VANILLA_FEATURE_DRIVER_C
#define CINDER_INC_VANILLA_FEATURE_DRIVER_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_driver.c"
#endif
#include CINDER_INC_VANILLA_FEATURE_DRIVER_C
#ifndef CINDER_INC_VANILLA_FILL_C
#define CINDER_INC_VANILLA_FILL_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_fill.c"
#endif
#include CINDER_INC_VANILLA_FILL_C

/* ------------------------------------------------------------------- setup */

static Gen *G;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;

static void gen_init(uint32_t seed) {
  if (G) return;
  fprintf(stderr, "cinder: compiling vanilla 26.2 overworld density graph\n");
  fflush(stderr);
  G = (Gen *)calloc(1, sizeof(Gen));
  G->world_seed = seed;
  G->biome_zoom_seed = biome_obfuscate_seed(seed);
  G->sea_level = SEA;
  G->ore_veins = 1;
  G->aquifers = 1;

  /* RandomState: settings.getRandomSource().newInstance(seed).forkPositional() */
  Xoro world;
  xoro_from_seed(&world, seed);
  G->world_random = xoro_fork_positional(&world);
  Xoro aquifer_src, ore_src;
  xoro_from_hash(G->world_random, "minecraft:aquifer", &aquifer_src);
  G->aquifer_random = xoro_fork_positional(&aquifer_src);
  xoro_from_hash(G->world_random, "minecraft:ore", &ore_src);
  G->ore_random = xoro_fork_positional(&ore_src);

  embed_cache_build(G);
  const char *ow = embed_str("settings:overworld");
  if (!ow) {
    fprintf(stderr, "cinder: missing overworld noise_settings\n");
    abort();
  }
  Jv *root = jparse(&G->arena, ow);
  G->overworld = root;
  G->sea_level = (int)jnum(jget(root, "sea_level"), SEA);
  G->ore_veins = jbool(jget(root, "ore_veins_enabled"), 1);
  G->aquifers = jbool(jget(root, "aquifers_enabled"), 1);
  Jv *router = jget(root, "noise_router");
  if (!router) {
    fprintf(stderr, "cinder: overworld noise_settings has no noise_router\n");
    abort();
  }
  /* NoiseRouter entries, compiled in the same order the JSON lists them. */
  G->barrier = df_compile(G, jget(router, "barrier"));
  G->flood = df_compile(G, jget(router, "fluid_level_floodedness"));
  G->spread = df_compile(G, jget(router, "fluid_level_spread"));
  G->lava = df_compile(G, jget(router, "lava"));
  G->temp = df_compile(G, jget(router, "temperature"));
  G->veg = df_compile(G, jget(router, "vegetation"));
  G->cont = df_compile(G, jget(router, "continents"));
  G->eros = df_compile(G, jget(router, "erosion"));
  G->depth_d = df_compile(G, jget(router, "depth"));
  G->ridges = df_compile(G, jget(router, "ridges"));
  G->prelim = df_compile(G, jget(router, "preliminary_surface_level"));
  G->final_d = df_compile(G, jget(router, "final_density"));
  G->vein_toggle = df_compile(G, jget(router, "vein_toggle"));
  G->vein_ridged = df_compile(G, jget(router, "vein_ridged"));
  G->vein_gap = df_compile(G, jget(router, "vein_gap"));
  G->surface_rule = jget(root, "surface_rule");

  DfNode *roots[16];
  int nroots = 0;
  roots[nroots++] = G->barrier;
  roots[nroots++] = G->flood;
  roots[nroots++] = G->spread;
  roots[nroots++] = G->lava;
  roots[nroots++] = G->temp;
  roots[nroots++] = G->veg;
  roots[nroots++] = G->cont;
  roots[nroots++] = G->eros;
  roots[nroots++] = G->depth_d;
  roots[nroots++] = G->ridges;
  roots[nroots++] = G->prelim;
  roots[nroots++] = G->final_d;
  roots[nroots++] = G->vein_toggle;
  roots[nroots++] = G->vein_ridged;
  roots[nroots++] = G->vein_gap;
  df_register_roots(G, roots, nroots);

  biome_init(G);
  fprintf(stderr, "cinder: noise graph ready (interp=%d flat=%d)\n", G->n_interp, G->n_flat);
  fflush(stderr);
}

/* ------------------------------------------------------------------ chunks */

static void chunk_init(Chunk *c, int cx, int cz) {
  c->cx = cx;
  c->cz = cz;
  memset(c->b, 0, sizeof(uint16_t) * (size_t)(16 * 384 * 16));
  memset(c->biome, 0, sizeof c->biome);
  for (int i = 0; i < 256; i++) {
    c->hm_ocean_floor[i] = MIN_Y;
    c->hm_world_surface[i] = MIN_Y;
  }
}

static uint32_t checksum_chunk(const Chunk *c) {
  uint32_t s = (uint32_t)(c->cx * 131 + c->cz);
  for (int i = 0; i < 16 * 384 * 16; i++) s += c->b[i];
  for (int i = 0; i < BIOME_CELLS; i++) s += c->biome[i] * 17u;
  return s;
}

typedef struct {
  uint32_t seed, grid, ox, oz;
  int threads;
  const char *dump;
  int skip_features, skip_surface, skip_carvers;
} Options;

static void gen_chunk(Gen *g, int cx, int cz, Chunk *c, ChunkEval *ce, uint32_t *out,
                      const Options *o) {
  chunk_init(c, cx, cz);
  chunk_eval_begin(g, ce, cx, cz);
  gen_fill_noise(g, c, ce);
  if (!o->skip_surface) gen_surface(g, c, ce);
  if (!o->skip_carvers) gen_carvers(g, c, ce);
  gen_light(g, c);
  *out = checksum_chunk(c);
}

/* ------------------------------------------------------------------- light */

// Cinder's own lighting: sky light per column plus a short horizontal spread.
// Vanilla does not persist light in region files (it is recomputed on load), so
// this stage is deliberately outside block-level parity.
void gen_light(Gen *g, Chunk *c) {
  (void)g;
  static const int passes = 2;
  uint8_t *sky = (uint8_t *)calloc(16 * 384 * 16, 1);
  for (int lz = 0; lz < 16; lz++) {
    for (int lx = 0; lx < 16; lx++) {
      int sl = 15;
      for (int y = MIN_Y + HEIGHT - 1; y >= MIN_Y; y--) {
        uint16_t b = getb(c, lx, y, lz);
        if (blocks_motion(b) && !is_leaves(b)) {
          sl = 0;
        } else if (b == B_WATER && sl > 0) {
          sl--;
        }
        sky[idx(lx, y, lz)] = (uint8_t)sl;
      }
    }
  }
  for (int pass = 0; pass < passes; pass++) {
    for (int y = MIN_Y; y < MIN_Y + HEIGHT; y++) {
      for (int lz = 0; lz < 16; lz++) {
        for (int lx = 0; lx < 16; lx++) {
          int i = idx(lx, y, lz);
          int s = sky[i];
          if (lx > 0 && sky[idx(lx - 1, y, lz)] > s + 1) s = sky[idx(lx - 1, y, lz)] - 1;
          if (lx < 15 && sky[idx(lx + 1, y, lz)] > s + 1) s = sky[idx(lx + 1, y, lz)] - 1;
          if (lz > 0 && sky[idx(lx, y, lz - 1)] > s + 1) s = sky[idx(lx, y, lz - 1)] - 1;
          if (lz < 15 && sky[idx(lx, y, lz + 1)] > s + 1) s = sky[idx(lx, y, lz + 1)] - 1;
          sky[i] = (uint8_t)s;
        }
      }
    }
  }
  free(sky);
}

/* ------------------------------------------------------------------- driver */


static Options parse_options(void) {
  Options o;
  o.seed = 1;
  o.grid = 32;
  o.ox = 0;
  o.oz = 0;
  o.threads = 1;
  o.dump = 0;

  long nproc = sysconf(_SC_NPROCESSORS_ONLN);
  o.threads = nproc > 0 ? (int)nproc : 1;
  if (o.threads > 32) o.threads = 32;

  const char *env;
  if ((env = getenv("CINDER_GRID")) && atoi(env) > 0) o.grid = (uint32_t)atoi(env);
  if ((env = getenv("CINDER_SEED")) && *env) o.seed = (uint32_t)strtoul(env, 0, 10);
  if ((env = getenv("CINDER_ORIGIN_X")) && *env) o.ox = (uint32_t)strtoul(env, 0, 10);
  if ((env = getenv("CINDER_ORIGIN_Z")) && *env) o.oz = (uint32_t)strtoul(env, 0, 10);
  if ((env = getenv("CINDER_THREADS")) && atoi(env) > 0) o.threads = atoi(env);
  o.dump = getenv("CINDER_DUMP");
  o.skip_features = 0;
  o.skip_surface = 0;
  o.skip_carvers = 0;
  if ((env = getenv("CINDER_SKIP_STAGES")) && *env) {
    o.skip_features = strstr(env, "features") != 0;
    o.skip_surface = strstr(env, "surface") != 0;
    o.skip_carvers = strstr(env, "carvers") != 0;
  }

  FILE *f = fopen("/proc/self/cmdline", "rb");
  if (!f) return o;
  char buf[8192];
  size_t m = fread(buf, 1, sizeof buf - 1, f);
  fclose(f);
  buf[m] = 0;
  const char *args[16];
  int nargs = 0;
  for (size_t i = 0; i < m && nargs < 16;) {
    size_t len = strlen(buf + i);
    if (len == 0) break;
    args[nargs++] = buf + i;
    i += len + 1;
  }
  for (int i = 0; i < nargs; i++) {
    const char *a = args[i];
    const char *v = i + 1 < nargs ? args[i + 1] : 0;
    if (!strcmp(a, "--threads") && v) o.threads = atoi(v);
    else if (!strcmp(a, "--grid") && v) o.grid = (uint32_t)atoi(v);
    else if (!strcmp(a, "--seed") && v) o.seed = (uint32_t)strtoul(v, 0, 10);
    else if (!strcmp(a, "--dump") && v) o.dump = v;
    else {
      const char *eq = strchr(a, '=');
      if (eq && !strncmp(a, "--threads=", 10)) o.threads = atoi(eq + 1);
      else if (eq && !strncmp(a, "--grid=", 7)) o.grid = (uint32_t)atoi(eq + 1);
      else if (eq && !strncmp(a, "--seed=", 7)) o.seed = (uint32_t)strtoul(eq + 1, 0, 10);
      else if (eq && !strncmp(a, "--dump=", 7)) o.dump = eq + 1;
    }
  }
  if (o.threads < 1) o.threads = 1;
  if (o.threads > 32) o.threads = 32;
  return o;
}

/* Writes the dump in chunk order: header, palette, biomes, then one record per
 * chunk (cx, cz, blocks, biomes). */
static void dump_write_slots(const Options *o, FeatureChunk **slots, int n, int grid) {
  char path[1024];
  snprintf(path, sizeof path, "%s/dump.bin", o->dump);
  FILE *out = fopen(path, "wb");
  if (!out) {
    fprintf(stderr, "cinder: cannot write %s\n", path);
    exit(1);
  }
  uint32_t header[5];
  header[0] = 0x52444E43u; /* "CNDR" */
  header[1] = 1;
  header[2] = o->seed;
  header[3] = o->grid;
  header[4] = 16;
  fwrite(header, sizeof header, 1, out);
  uint32_t nblocks = B_COUNT;
  fwrite(&nblocks, 4, 1, out);
  for (int i = 0; i < B_COUNT; i++) {
    const char *name = block_name(i);
    uint16_t len = (uint16_t)strlen(name);
    fwrite(&len, 2, 1, out);
    fwrite(name, 1, len, out);
  }
  uint32_t nbiomes = BIO_COUNT;
  fwrite(&nbiomes, 4, 1, out);
  for (int i = 0; i < BIO_COUNT; i++) {
    const char *name = biome_name(i);
    uint16_t len = (uint16_t)strlen(name);
    fwrite(&len, 2, 1, out);
    fwrite(name, 1, len, out);
  }
  for (int i = 0; i < n; i++) {
    FeatureChunk *slot = slots[i];
    int32_t hdr[2] = {slot->cx, slot->cz};
    fwrite(hdr, sizeof hdr, 1, out);
    fwrite(slot->b, sizeof(uint16_t), 16 * 384 * 16, out);
    fwrite(slot->biome, 1, BIOME_CELLS, out);
  }
  fclose(out);
  fprintf(stderr, "cinder: dump written to %s (%d chunks)\n", path, n);
  (void)grid;
}

static uint32_t generate_world(const Options *o) {
  pthread_mutex_lock(&g_mu);
  gen_init(o->seed);
  pthread_mutex_unlock(&g_mu);
  const int grid = (int)o->grid;
  const int n = grid * grid;

  FeatureChunk **slots = (FeatureChunk **)calloc((size_t)n, sizeof(FeatureChunk *));
  for (int i = 0; i < n; i++) {
    slots[i] = feat_chunk_new((int)o->ox + i % grid, (int)o->oz + i / grid);
  }

  if (o->skip_features) {
    /* the stage-by-stage path: terrain in parallel, no feature halo */
    feat_terrain_parallel_all(G, slots, n, o->threads, o->skip_surface, o->skip_carvers);
    for (int i = 0; i < n; i++) {
      Chunk c;
      memset(&c, 0, sizeof c);
      c.cx = slots[i]->cx;
      c.cz = slots[i]->cz;
      c.b = slots[i]->b;
      gen_light(G, &c);
    }
  } else {
    feat_run_batch(G, grid, (int)o->ox, (int)o->oz, slots, o->threads);
  }

  uint32_t acc = 0;
  for (int i = 0; i < n; i++) {
    acc += (uint32_t)(slots[i]->cx * 131 + slots[i]->cz);
    for (int k = 0; k < 16 * 384 * 16; k++) acc += slots[i]->b[k];
    for (int k = 0; k < BIOME_CELLS; k++) acc += slots[i]->biome[k] * 17u;
  }
  if (o->dump) dump_write_slots(o, slots, n, grid);
  for (int i = 0; i < n; i++) feat_chunk_free(slots[i]);
  free(slots);
  return acc;
}

#ifdef CID_WORLD_GENERATE
Term world_generate_run(Env e, Term *f, IoWork *w) {
  (void)w;
  Options o = parse_options();
  o.seed = (uint32_t)(uint64_t)f[0];
  uint32_t fp = generate_world(&o);
  return (Term)(uint64_t)fp;
}

static void __attribute__((constructor)) world_generate_use(void) {
  io_eff(CID_WORLD_GENERATE, world_generate_run, 0);
}
#endif

#ifdef CINDER_STANDALONE_MAIN
int main(void) {
  Options o = parse_options();
  fprintf(stderr, "cinder-gen: seed=%u grid=%u threads=%d%s%s\n", o.seed, o.grid, o.threads,
          o.dump ? " dump=" : "", o.dump ? o.dump : "");
  uint32_t fp = generate_world(&o);
  printf("checksum=%u\n", fp);
  return 0;
}
#endif
