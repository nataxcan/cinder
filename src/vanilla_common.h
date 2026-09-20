// Shared declarations for Cinder's vanilla worldgen port.
//
// The generator is one translation unit: src/vanilla_gen.c defines the shared
// state and includes this header plus the stage files (vanilla_rand_noise.c,
// vanilla_surface.c, vanilla_carvers.c, vanilla_features.c). Stage files must
// also compile standalone for syntax checks:
//   clang -fsyntax-only -std=gnu11 -I src src/vanilla_surface.c
//
// Semantics mirror vanilla 26.2 exactly (net.minecraft.* in the decompiled
// sources at /mnt/c/Users/nataxcan/Documents/dev/mc-src-26.2). Java double/float
// arithmetic is preserved, not "cleaned up": the port is meant to be bit-equal,
// so every lerp/clamp/cast must keep Java's evaluation order and types.
#ifndef CINDER_VANILLA_COMMON_H
#define CINDER_VANILLA_COMMON_H

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* worldgen data extracted from the vanilla jar (src/vanilla/embed.h) */
const char *cinder_embed_get(const char *key);

/* worldgen data extracted from the vanilla jar (src/vanilla/embed.h) */
const char *cinder_embed_get(const char *key);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ blocks */

enum {
  B_AIR = 0,
  B_STONE,
  B_DIRT,
  B_GRASS,
  B_BEDROCK,
  B_WATER,
  B_LAVA,
  B_SAND,
  B_RED_SAND,
  B_SANDSTONE,
  B_GRAVEL,
  B_CLAY,
  B_COAL,
  B_IRON,
  B_COPPER,
  B_GOLD,
  B_REDSTONE,
  B_LAPIS,
  B_DIAMOND,
  B_EMERALD,
  B_DEEPSLATE,
  B_DS_COAL,
  B_DS_IRON,
  B_DS_COPPER,
  B_DS_GOLD,
  B_DS_REDSTONE,
  B_DS_LAPIS,
  B_DS_DIAMOND,
  B_GRANITE,
  B_DIORITE,
  B_ANDESITE,
  B_TUFF,
  B_CALCITE,
  B_SMOOTH_BASALT,
  B_AMETHYST,
  B_DRIPSTONE,
  B_MOSS,
  B_MUD,
  B_PACKED_ICE,
  B_ICE,
  B_SNOW,
  B_POWDER_SNOW,
  B_TERRACOTTA,
  B_OAK_LOG,
  B_OAK_LEAVES,
  B_BIRCH_LOG,
  B_BIRCH_LEAVES,
  B_SPRUCE_LOG,
  B_SPRUCE_LEAVES,
  B_JUNGLE_LOG,
  B_JUNGLE_LEAVES,
  B_ACACIA_LOG,
  B_ACACIA_LEAVES,
  B_DARK_LOG,
  B_DARK_LEAVES,
  B_CHERRY_LOG,
  B_CHERRY_LEAVES,
  B_MANGROVE_LOG,
  B_MANGROVE_LEAVES,
  B_CACTUS,
  B_SUGAR_CANE,
  B_PUMPKIN,
  B_KELP,
  B_SEAGRASS,
  B_GLOW_LICHEN,
  B_PODZOL,
  B_MYCELIUM,
  B_ROOTED_DIRT,
  B_SNOW_BLOCK,
  B_MAGMA,
  B_OBSIDIAN,
  B_INFESTED,
  B_SHORT_GRASS,
  B_FERN,
  B_DEAD_BUSH,
  B_LILY,
  B_RAW_COPPER,
  B_RAW_IRON,
  B_ALLIUM,
  B_AMETHYST_CLUSTER,
  B_AZURE_BLUET,
  B_BLUE_ICE,
  B_BLUE_ORCHID,
  B_BROWN_MUSHROOM,
  B_BUDDING_AMETHYST,
  B_BUSH,
  B_CACTUS_FLOWER,
  B_CAVE_VINES,
  B_CAVE_VINES_PLANT,
  B_CLOSED_EYEBLOSSOM,
  B_CORNFLOWER,
  B_DANDELION,
  B_DEEPSLATE_EMERALD_ORE,
  B_FIREFLY_BUSH,
  B_HANGING_ROOTS,
  B_INFESTED_DEEPSLATE,
  B_LARGE_AMETHYST_BUD,
  B_LARGE_FERN,
  B_LEAF_LITTER,
  B_LILAC,
  B_LILY_OF_THE_VALLEY,
  B_MEDIUM_AMETHYST_BUD,
  B_MELON,
  B_MOSSY_COBBLESTONE,
  B_ORANGE_TULIP,
  B_OXEYE_DAISY,
  B_PALE_MOSS_BLOCK,
  B_PEONY,
  B_PINK_PETALS,
  B_PINK_TULIP,
  B_POINTED_DRIPSTONE,
  B_POPPY,
  B_POTENT_SULFUR,
  B_RED_MUSHROOM,
  B_RED_TULIP,
  B_ROSE_BUSH,
  B_SHORT_DRY_GRASS,
  B_SMALL_AMETHYST_BUD,
  B_SPORE_BLOSSOM,
  B_SULFUR,
  B_SULFUR_SPIKE,
  B_SUNFLOWER,
  B_SWEET_BERRY_BUSH,
  B_TALL_DRY_GRASS,
  B_TALL_GRASS,
  B_COBWEB,
  B_SPAWNER,
  B_COBBLESTONE,
  B_MOSS_CARPET,
  B_HANGING_MOSS,
  B_GLOW_BERRIES,
  B_CAVE_AIR,
  B_WHITE_TERRACOTTA,
  B_ORANGE_TERRACOTTA,
  B_YELLOW_TERRACOTTA,
  B_BROWN_TERRACOTTA,
  B_RED_TERRACOTTA,
  B_LIGHT_GRAY_TERRACOTTA,
  B_COARSE_DIRT,
  B_RED_SANDSTONE,
  B_CINNABAR,
  B_SOUL_SOIL,
  B_MUDDY_MANGROVE_ROOTS,
  B_SCULK,
  B_SCULK_VEIN,
  B_SCULK_CATALYST,
  B_SCULK_SHRIEKER,
  B_SUSPICIOUS_SAND,
  B_SANDSTONE_SLAB,
  B_BONE_BLOCK,
  B_SCULK_SENSOR,
  B_CHEST,
  B_FARMLAND,
  B_TALL_SEAGRASS,
  B_KELP_PLANT,
  B_SEA_PICKLE,
  B_BAMBOO,
  B_VINE,
  B_COCOA,
  B_BEE_NEST,
  B_MANGROVE_ROOTS,
  B_MANGROVE_PROPAGULE,
  B_AZALEA,
  B_AZALEA_LEAVES,
  B_FLOWERING_AZALEA,
  B_FLOWERING_AZALEA_LEAVES,
  B_BIG_DRIPLEAF,
  B_BIG_DRIPLEAF_STEM,
  B_SMALL_DRIPLEAF,
  B_PALE_OAK_LOG,
  B_PALE_OAK_LEAVES,
  B_PALE_MOSS_CARPET,
  B_PALE_HANGING_MOSS,
  B_CREAKING_HEART,
  B_CRIMSON_ROOTS,
  B_FIRE,
  B_SOUL_FIRE,
  B_WHITE_TULIP,
  B_WILDFLOWERS,
  B_VOID_AIR,
  B_OAK_SAPLING,
  B_SPRUCE_SAPLING,
  B_BIRCH_SAPLING,
  B_JUNGLE_SAPLING,
  B_ACACIA_SAPLING,
  B_DARK_OAK_SAPLING,
  B_CHERRY_SAPLING,
  B_PALE_OAK_SAPLING,
  B_COUNT
};

/* Vanilla resource names (used by --dump and by the surface/carver ports). */
const char *block_name(int block);
const char *biome_name(int biome);

/* ------------------------------------------------------------------ biomes */

enum {
  BIO_OCEAN = 0,
  BIO_DEEP_OCEAN,
  BIO_FROZEN_OCEAN,
  BIO_DEEP_FROZEN_OCEAN,
  BIO_COLD_OCEAN,
  BIO_DEEP_COLD_OCEAN,
  BIO_LUKEWARM_OCEAN,
  BIO_DEEP_LUKEWARM_OCEAN,
  BIO_WARM_OCEAN,
  BIO_RIVER,
  BIO_FROZEN_RIVER,
  BIO_BEACH,
  BIO_SNOWY_BEACH,
  BIO_STONY_SHORE,
  BIO_MUSHROOM,
  BIO_PLAINS,
  BIO_SUNFLOWER,
  BIO_FOREST,
  BIO_FLOWER_FOREST,
  BIO_BIRCH,
  BIO_OLD_BIRCH,
  BIO_DARK_FOREST,
  BIO_TAIGA,
  BIO_SNOWY_TAIGA,
  BIO_OLD_PINE,
  BIO_OLD_SPRUCE,
  BIO_SAVANNA,
  BIO_SAVANNA_PLATEAU,
  BIO_WINDSWEPT_SAVANNA,
  BIO_JUNGLE,
  BIO_SPARSE_JUNGLE,
  BIO_BAMBOO_JUNGLE,
  BIO_DESERT,
  BIO_SWAMP,
  BIO_MANGROVE,
  BIO_BADLANDS,
  BIO_WOODED_BADLANDS,
  BIO_ERODED_BADLANDS,
  BIO_MEADOW,
  BIO_CHERRY,
  BIO_GROVE,
  BIO_SNOWY_SLOPES,
  BIO_JAGGED_PEAKS,
  BIO_FROZEN_PEAKS,
  BIO_STONY_PEAKS,
  BIO_SNOWY_PLAINS,
  BIO_ICE_SPIKES,
  BIO_WINDSWEPT_HILLS,
  BIO_WINDSWEPT_GRAVEL,
  BIO_WINDSWEPT_FOREST,
  BIO_DRIPSTONE,
  BIO_LUSH,
  BIO_SULFUR,
  BIO_DEEP_DARK,
  BIO_PALE_GARDEN,
  BIO_COUNT
};

/* Vanilla resource names, indexed by the enum above (used by --dump). */

/* --------------------------------------------------------------- constants */

#define MIN_Y (-64)
#define HEIGHT 384
#define SEA 63
#define CX 16
#define CY 384
#define CZ 16
#define CELL_XZ 4
#define CELL_Y 8

/* Biome cells per chunk: 24 sections x 4x4x4, section-major. */
#define BIOME_CELLS (24 * 64)

static inline int idx(int x, int y, int z) {
  return ((y - MIN_Y) * CZ + z) * CX + x;
}

static inline int in_chunk(int lx, int y, int lz) {
  return lx >= 0 && lx < 16 && lz >= 0 && lz < 16 && y >= MIN_Y && y < MIN_Y + HEIGHT;
}

/* Biome cell index inside a chunk (section-major 4x4x4, as PalettedContainer). */
static inline int biome_idx(int x, int y, int z) {
  int sec = (y >> 4) - (MIN_Y >> 4);
  return sec * 64 + (y & 3) * 16 + (z & 3) * 4 + (x & 3);
}

/* ------------------------------------------------------- Java math helpers */

static inline double jlerp(double a, double p0, double p1) { return p0 + a * (p1 - p0); }

static inline double jlerp2(double a1, double a2, double x00, double x10, double x01, double x11) {
  return jlerp(a2, jlerp(a1, x00, x10), jlerp(a1, x01, x11));
}

static inline double jlerp3(double a1, double a2, double a3, double x000, double x100, double x010,
                            double x110, double x001, double x101, double x011, double x111) {
  return jlerp(a3, jlerp2(a1, a2, x000, x100, x010, x110), jlerp2(a1, a2, x001, x101, x011, x111));
}

static inline double jsmoothstep(double x) { return x * x * x * (x * (x * 6.0 - 15.0) + 10.0); }

/* Math.min / Math.max: NaN in, NaN out. */
static inline double jmin(double a, double b) {
  if (isnan(a) || isnan(b)) return NAN;
  return a < b ? a : b;
}

static inline double jmax(double a, double b) {
  if (isnan(a) || isnan(b)) return NAN;
  return a > b ? a : b;
}

static inline double jclamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

static inline double jclamped_lerp(double factor, double lo, double hi) {
  if (factor < 0.0) return lo;
  return factor > 1.0 ? hi : jlerp(factor, lo, hi);
}

static inline double jinverse_lerp(double v, double lo, double hi) { return (v - lo) / (hi - lo); }

static inline double jclamped_map(double v, double from_min, double from_max, double to_min, double to_max) {
  return jclamped_lerp(jinverse_lerp(v, from_min, from_max), to_min, to_max);
}

static inline long jlfloor(double v) { return (long)floor(v); }
static inline int jfloor_div(int a, int b) { return (int)floor((double)a / (double)b); }
static inline int jfloor(double v) { return (int)floor(v); }

/* -------------------------------------------------------------------- json */

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JKind;

typedef struct Jv {
  JKind kind;
  double num;
  const char *str;
  int slen;
  const char *key;
  int klen;
  struct Jv *child, *next;
} Jv;

typedef struct {
  char *arena;
  size_t used, cap;
} Arena;

void *agrow(Arena *a, size_t n);
Jv *jnew(Arena *a);
Jv *jparse(Arena *a, const char *s);
Jv *jget(Jv *o, const char *k);
const char *jstr(Jv *v);
double jnum(Jv *v, double d);
int jbool(Jv *v, int d);
int jarr_len(Jv *v);
Jv *jget_idx(Jv *arr, int i);

/* --------------------------------------------------- java random (xoroshiro) */

typedef struct {
  uint64_t lo, hi;
} Xoro;

void xoro_set_seed(Xoro *r, uint64_t lo, uint64_t hi);
uint64_t xoro_next_long(Xoro *r);
int xoro_next_int(Xoro *r);
int xoro_next_bound(Xoro *r, int bound);
double xoro_next_double(Xoro *r);
float xoro_next_float(Xoro *r);
void xoro_from_seed(Xoro *r, uint64_t seed);

typedef struct {
  uint64_t lo, hi;
} XoroFactory;

XoroFactory xoro_fork_positional(Xoro *r);
void xoro_at(XoroFactory f, int x, int y, int z, Xoro *out);
void xoro_from_hash(XoroFactory f, const char *name, Xoro *out);
void xoro_seed_from_hash(const char *name, uint64_t *lo, uint64_t *hi);
long mth_get_seed(int x, int y, int z);

/* ------------------------------------- java random (legacy / java.util.Random) */

typedef struct {
  uint64_t seed;
  /* MarsagliaPolarGaussian state (java.util.Random keeps the second value) */
  double next_next_gaussian;
  int have_next_next_gaussian;
  /* WorldgenRandom wraps another RandomSource and delegates next(bits) and
   * setSeed to it. WorldgenRandom extends LegacyRandomSource, so a worldgen RNG
   * built over the legacy source is the plain LCG above; the one built over an
   * Xoroshiro source (which is what applyBiomeDecoration uses, and therefore
   * what every feature RNG is) goes through the 128-bit generator instead.
   * Nothing else changes: nextInt(bound), nextFloat, nextDouble and nextLong are
   * all BitRandomSource defaults expressed in terms of next(bits). */
  int kind; /* LEGACY_RNG_LCG | LEGACY_RNG_XORO */
  Xoro xoro;
} LegacyRng;

#define LEGACY_RNG_LCG 0
#define LEGACY_RNG_XORO 1

void legacy_set_seed(LegacyRng *r, uint64_t seed);
int legacy_next(LegacyRng *r, int bits);
int legacy_next_int(LegacyRng *r);
int legacy_next_bound(LegacyRng *r, int bound);
float legacy_next_float(LegacyRng *r);
double legacy_next_double(LegacyRng *r);
int64_t legacy_next_long(LegacyRng *r);
void legacy_consume(LegacyRng *r, int n);
void legacy_set_large_feature_seed(LegacyRng *r, uint64_t world_seed, int chunk_x, int chunk_z);
void legacy_set_decoration_seed(LegacyRng *r, uint64_t world_seed, int min_block_x, int min_block_z);
int64_t legacy_next_long_bound(LegacyRng *r, int64_t bound);
/* java.util.Random.nextGaussian (Marsaglia polar form, cached second value) */
double legacy_next_gaussian(LegacyRng *r);

/* ------------------------------------------------------------------- noise */

typedef struct NormalNoise NormalNoise;

NormalNoise *normal_noise_create(Xoro *random, int first_octave, const double *amps, int namp);
double normal_noise_get(const NormalNoise *nn, double x, double y, double z);
double normal_noise_max(const NormalNoise *nn);
/* NormalNoise.create(new WorldgenRandom(new LegacyRandomSource(seed)), firstOctave, amps...)
 * with the legacy positional factory (PerlinNoise useNewInitialization = true over
 * a legacy random source): one nextLong() per perlin, then
 * octaveSeed = (octave_ + octave).hashCode() ^ factorySeed. */
NormalNoise *normal_noise_create_legacy(LegacyRng *r, int first_octave, const double *amps, int namp);

typedef struct BlendedNoise BlendedNoise;

BlendedNoise *blended_noise_create(Xoro *random, double xz_scale, double y_scale, double xz_factor,
                                   double y_factor, double smear_scale_multiplier);
double blended_noise_compute(const BlendedNoise *bn, int block_x, int block_y, int block_z);
double blended_noise_max(const BlendedNoise *bn);

/* -------------------------------------------------------- density functions */

typedef enum {
  DF_CONST,
  DF_ADD,
  DF_MUL,
  DF_MIN,
  DF_MAX,
  DF_ABS,
  DF_SQUARE,
  DF_CUBE,
  DF_HALF_NEG,
  DF_QUART_NEG,
  DF_SQUEEZE,
  DF_INVERT,
  DF_CLAMP,
  DF_NOISE,
  DF_SHIFTED,
  DF_SHIFT_A,
  DF_SHIFT_B,
  DF_RANGE,
  DF_YCLAMP,
  DF_CACHE2D,
  DF_CACHE_ONCE,
  DF_FLAT,
  DF_INTERP,
  DF_BLEND_DENSITY,
  DF_FIND_TOP,
  DF_OLD_BLEND,
  DF_BLEND_A,
  DF_BLEND_O,
  DF_SPLINE,
  DF_INTERVAL_SELECT
} DfKind;

/* CubicSpline (net.minecraft.util.CubicSpline): constants and multipoints. */
typedef struct Spline {
  int is_const;
  float value;
  struct DfNode *coordinate; /* only for multipoints */
  int n;                     /* points */
  float *locations;
  float *derivatives;
  struct Spline **values;
} Spline;

typedef struct DfNode {
  DfKind kind;
  double c;                  /* DF_CONST */
  struct DfNode *a, *b, *c3;
  NormalNoise *noise;        /* DF_NOISE / DF_SHIFTED / DF_SHIFT_A / DF_SHIFT_B */
  BlendedNoise *blend;       /* DF_OLD_BLEND */
  Spline *spline;            /* DF_SPLINE */
  int nselect;               /* DF_INTERVAL_SELECT */
  double *thresholds;
  struct DfNode **fns;
  double xz_scale, y_scale;
  double from_v, to_v;       /* clamp / range / y clamp */
  int from_y, to_y;          /* y clamped gradient */
  int lower_bound, cell_height; /* find_top_surface */
  int cid;                   /* per-chunk cache slot */
  const char *name;
} DfNode;

#define MAX_CID 640
#define MAX_INTERP 8
#define MAX_FLAT 24

typedef struct {
  DfNode *node;
  double *grid; /* (CELL_XZ+1) * (HEIGHT/CELL_Y+1) * (CELL_XZ+1) corner values of node->a */
  int filled;
} InterpGrid;

typedef struct {
  DfNode *node;
  double v[25];
  unsigned char filled[25];
} FlatEntry;

typedef struct Gen Gen;
typedef struct Aquifer Aquifer;

typedef struct {
  uint64_t *keys;
  int *vals;
  unsigned char *used;
  int cap, count;
} PrelimCache;

/* Per-chunk density-function evaluation state (mirrors one vanilla NoiseChunk). */
typedef struct ChunkEval {
  Gen *g;
  int cx, cz;
  int x, y, z; /* current evaluation position, block coordinates */
  InterpGrid interp[MAX_INTERP];
  int ninterp;
  FlatEntry flat[MAX_FLAT];
  int nflat;
  double c2d_v[MAX_CID];
  int c2d_x[MAX_CID], c2d_z[MAX_CID];
  unsigned char c2d_ok[MAX_CID];
  double c1_v[MAX_CID];
  int c1_x[MAX_CID], c1_y[MAX_CID], c1_z[MAX_CID];
  unsigned char c1_ok[MAX_CID];
  Aquifer *aquifer;
  PrelimCache prelim;
  int aquifer_ready;
} ChunkEval;

DfNode *df_compile(Gen *g, Jv *v);
DfNode *df_compile_named(Gen *g, const char *name);
double df_eval_at(Gen *g, ChunkEval *ce, DfNode *n, int x, int y, int z);
void chunk_eval_begin(Gen *g, ChunkEval *ce, int cx, int cz);
void chunk_eval_free(ChunkEval *ce);
void df_register_roots(Gen *g, DfNode **roots, int nroots);

/* ------------------------------------------------------------------ chunks */

typedef struct Chunk {
  uint16_t *b;                /* 16*384*16, index by idx(x,y,z) */
  uint8_t biome[BIOME_CELLS]; /* per 4x4x4 cell */
  /* OCEAN_FLOOR_WG / WORLD_SURFACE_WG: first-available Y (top + 1) */
  int32_t hm_ocean_floor[256];
  int32_t hm_world_surface[256];
  int cx, cz;
} Chunk;

static inline void setb(Chunk *c, int x, int y, int z, uint16_t v) {
  if (in_chunk(x, y, z)) c->b[idx(x, y, z)] = v;
}

static inline uint16_t getb(const Chunk *c, int x, int y, int z) {
  if (!in_chunk(x, y, z)) return B_AIR;
  return c->b[idx(x, y, z)];
}

static inline int is_fluid(uint16_t b) { return b == B_WATER || b == B_LAVA; }

/* MATERIAL_MOTION_BLOCKING (Heightmap.Types.OCEAN_FLOOR_WG predicate). */
static inline int blocks_motion(uint16_t b) {
  switch (b) {
    case B_AIR:
    case B_WATER:
    case B_LAVA:
    case B_SHORT_GRASS:
    case B_FERN:
    case B_KELP:
    case B_SEAGRASS:
    case B_LILY:
    case B_GLOW_LICHEN:
    case B_SUGAR_CANE:
    case B_DEAD_BUSH:
      return 0;
    default:
      return 1;
  }
}

/* Heightmap.update(x, y, z, state) with `opaque` = predicate(state). */
#define HM_NOT_AIR 0
#define HM_MOTION_BLOCKING 1
void hm_update(int32_t *hm, Chunk *c, int lx, int ly, int lz, int kind);

static inline int is_leaves(uint16_t b) {
  return b == B_OAK_LEAVES || b == B_BIRCH_LEAVES || b == B_SPRUCE_LEAVES ||
         b == B_JUNGLE_LEAVES || b == B_ACACIA_LEAVES || b == B_DARK_LEAVES ||
         b == B_CHERRY_LEAVES || b == B_MANGROVE_LEAVES;
}
static inline int hm_top(const int32_t *hm, int lx, int lz) { return hm[lz * 16 + lx] - 1; }

/* --------------------------------------------------------------- generator */

typedef struct {
  char id[64];
  NormalNoise *nn;
} NoiseEnt;

struct Gen {
  uint64_t world_seed;
  /* world positional factory; aquifer/ore are derived from it */
  XoroFactory world_random;
  XoroFactory aquifer_random;
  XoroFactory ore_random;
  NoiseEnt noises[96];
  int nnoise;
  Arena arena;
  DfNode *final_d, *temp, *veg, *cont, *eros, *ridges, *prelim;
  DfNode *vein_toggle, *vein_ridged, *vein_gap;
  DfNode *barrier, *flood, *spread, *lava, *depth_d, *erosion_d;
  DfNode *surface_d;
  const struct BiomeParamPoint *params;
  int nparams;
  struct Brtree *btree;
  Jv *surface_rule;
  Jv *overworld;
  int sea_level;
  int ore_veins;
  int aquifers;
  int64_t biome_zoom_seed; /* BiomeManager.obfuscateSeed(world seed) */
  /* wrapping nodes that need per-chunk state (filled by gen_init) */
  DfNode *interp_nodes[MAX_INTERP];
  int n_interp;
  DfNode *flat_nodes[MAX_FLAT];
  int n_flat;
};

NormalNoise *gen_noise(Gen *g, const char *id);

/* RandomState.getOrCreateRandomFactory(Identifier): a positional factory seeded
 * from the world seed and the resource name (used by surface rule gradients). */
XoroFactory gen_random_factory(Gen *g, const char *name);

/* ---------------------------------------------------------------- aquifer */

Aquifer *aquifer_create(Gen *g, ChunkEval *ce);
void aquifer_free(Aquifer *aq);
/* 1 = produced air/water/lava (writes *out), 0 = solid (leave to default block) */
int aquifer_substance(Aquifer *aq, ChunkEval *ce, int x, int y, int z, double density, uint16_t *out);
int aquifer_schedule_update(Aquifer *aq);
int prelim_surface_level(Gen *g, ChunkEval *ce, int block_x, int block_z);

/* ----------------------------------------------------------------- biomes */

typedef struct BiomeParamPoint {
  int16_t biome;
  int64_t temperature[2];
  int64_t humidity[2];
  int64_t continentalness[2];
  int64_t erosion[2];
  int64_t depth[2];
  int64_t weirdness[2];
  int64_t offset;
} BiomeParamPoint;

void biome_init(Gen *g);
void biome_fill_chunk(Gen *g, Chunk *c, ChunkEval *ce);
double biome_temperature(Gen *g, int biome);
const char *biome_path(int biome);

/* Vanilla Climate.ParameterList.findValue: search tree over the parameter list. */
int biome_pick(Gen *g, const int64_t target[7], int *last_leaf);
int biome_at_quart(Gen *g, ChunkEval *ce, int quart_x, int quart_y, int quart_z);
int biome_at_block(Gen *g, ChunkEval *ce, int x, int y, int z);
/* BiomeManager.getBiome: the fuzzy 8-corner lookup used by the surface rules
 * and by carvers' topMaterial (vanilla: BiomeManager with the zoomed seed). */
int biome_at_block_fuzzy(Gen *g, ChunkEval *ce, int x, int y, int z);

/* ------------------------------------------------------------------ stages */

void gen_fill_noise(Gen *g, Chunk *c, ChunkEval *ce);
void gen_surface(Gen *g, Chunk *c, ChunkEval *ce);
void gen_carvers(Gen *g, Chunk *c, ChunkEval *ce);
void gen_light(Gen *g, Chunk *c);

/* Surface system hook used by carvers (CarvingContext.topMaterial).
 * TOP_MATERIAL_NONE means vanilla's Optional.empty(): no rule produced a block. */
#define TOP_MATERIAL_NONE 0xFFFFu
uint16_t surface_top_material(Gen *g, Chunk *c, ChunkEval *ce, int lx, int ly, int lz, int under_fluid);

#endif /* CINDER_VANILLA_COMMON_H */
