// Vegetation configured-feature types for the vanilla feature stage.
//
// Ported from net.minecraft.world.level.levelgen.feature.* (26.2):
//   TreeFeature (+ trunkplacers/*, foliageplacers/*, treedecorators/*,
//   rootplacers/*, featuresize/*), SimpleBlockFeature, RandomSelectorFeature,
//   SimpleRandomSelectorFeature, BlockColumnFeature, VegetationPatchFeature,
//   SeagrassFeature, KelpFeature, SeaPickleFeature, BambooFeature, VinesFeature,
//   MultifaceGrowthFeature, RootSystemFeature, SnowAndFreezeFeature, plus the
//   stateproviders/* the tree configs use.
//
// Everything is mirrored line by line: same draw order, same float/double
// types, same iteration order.  Cinder's palette is *name-level* (no block
// properties), so a property write (log axis, leaf distance/persistent,
// waterlogged, half, facing, age, ...) collapses to "the same block".  Those
// collapses are listed in the file's closing comment block; they are the only
// intentional differences from the Java.
#ifndef CINDER_INC_VANILLA_FEATURE_COMMON_H
#define CINDER_INC_VANILLA_FEATURE_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_common.h"
#endif
#include CINDER_INC_VANILLA_FEATURE_COMMON_H

#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------- directions */

/* Direction.values() order: DOWN, UP, NORTH, SOUTH, WEST, EAST. */
#define VEG_DOWN 0
#define VEG_UP 1
#define VEG_NORTH 2
#define VEG_SOUTH 3
#define VEG_WEST 4
#define VEG_EAST 5
static const int veg_dx[6] = {0, 0, 0, 0, -1, 1};
static const int veg_dy[6] = {-1, 1, 0, 0, 0, 0};
static const int veg_dz[6] = {0, 0, -1, 1, 0, 0};
static const int veg_opp[6] = {VEG_UP, VEG_DOWN, VEG_SOUTH, VEG_NORTH, VEG_EAST, VEG_WEST};
/* Direction.Plane.HORIZONTAL: NORTH, EAST, SOUTH, WEST. */
static const int veg_horiz[4] = {VEG_NORTH, VEG_EAST, VEG_SOUTH, VEG_WEST};
/* Direction.getClockWise for the horizontal plane. */
static int veg_cw(int d) {
  switch (d) {
    case VEG_NORTH: return VEG_EAST;
    case VEG_EAST: return VEG_SOUTH;
    case VEG_SOUTH: return VEG_WEST;
    default: return VEG_NORTH;
  }
}
/* Direction.getAxis(): 0 = X, 1 = Y, 2 = Z. */
static int veg_axis(int d) { return d == VEG_DOWN || d == VEG_UP ? 1 : (d == VEG_NORTH || d == VEG_SOUTH ? 2 : 0); }
/* Direction.getAxisDirection() == POSITIVE (EAST/SOUTH/UP). */
static int veg_pos_axis_dir(int d) { return d == VEG_UP || d == VEG_SOUTH || d == VEG_EAST; }

/* ------------------------------------------------------------- small bits */

static int veg_air(uint16_t b) { return b == B_AIR || b == B_CAVE_AIR || b == B_VOID_AIR; }
static int veg_fluid(uint16_t b) { return b == B_WATER || b == B_LAVA; }
static int veg_water(uint16_t b) { return b == B_WATER; }

/* #minecraft:logs (logs_that_burn + the nether stems; the palette only holds
 * the overworld logs). */
static int veg_log(uint16_t b) {
  switch (b) {
    case B_OAK_LOG:
    case B_BIRCH_LOG:
    case B_SPRUCE_LOG:
    case B_JUNGLE_LOG:
    case B_ACACIA_LOG:
    case B_DARK_LOG:
    case B_CHERRY_LOG:
    case B_MANGROVE_LOG:
    case B_PALE_OAK_LOG:
      return 1;
    default:
      return 0;
  }
}

/* #minecraft:leaves */
static int veg_leaves(uint16_t b) {
  switch (b) {
    case B_OAK_LEAVES:
    case B_BIRCH_LEAVES:
    case B_SPRUCE_LEAVES:
    case B_JUNGLE_LEAVES:
    case B_ACACIA_LEAVES:
    case B_DARK_LEAVES:
    case B_CHERRY_LEAVES:
    case B_MANGROVE_LEAVES:
    case B_PALE_OAK_LEAVES:
    case B_AZALEA_LEAVES:
    case B_FLOWERING_AZALEA_LEAVES:
      return 1;
    default:
      return 0;
  }
}

/* #minecraft:substrate_overworld */
static int veg_substrate(uint16_t b) {
  switch (b) {
    case B_DIRT:
    case B_COARSE_DIRT:
    case B_ROOTED_DIRT:
    case B_MUD:
    case B_MUDDY_MANGROVE_ROOTS:
    case B_MOSS:
    case B_PALE_MOSS_BLOCK:
    case B_GRASS:
    case B_PODZOL:
    case B_MYCELIUM:
      return 1;
    default:
      return 0;
  }
}
/* #minecraft:supports_vegetation */
static int veg_supports_vegetation(uint16_t b) { return veg_substrate(b) || b == B_FARMLAND; }
/* #minecraft:sand */
static int veg_sand(uint16_t b) { return b == B_SAND || b == B_RED_SAND || b == B_SUSPICIOUS_SAND; }
/* #minecraft:terracotta (the palette only keeps the base + 6 colours) */
static int veg_terracotta(uint16_t b) {
  switch (b) {
    case B_TERRACOTTA:
    case B_WHITE_TERRACOTTA:
    case B_ORANGE_TERRACOTTA:
    case B_YELLOW_TERRACOTTA:
    case B_BROWN_TERRACOTTA:
    case B_RED_TERRACOTTA:
    case B_LIGHT_GRAY_TERRACOTTA:
      return 1;
    default:
      return 0;
  }
}
/* #minecraft:supports_dry_vegetation */
static int veg_supports_dry(uint16_t b) { return veg_sand(b) || veg_terracotta(b) || veg_supports_vegetation(b); }
/* #minecraft:supports_bamboo */
static int veg_supports_bamboo(uint16_t b) {
  return veg_sand(b) || veg_substrate(b) || b == B_BAMBOO || b == B_GRAVEL;
}
/* #minecraft:supports_sugar_cane */
static int veg_supports_cane(uint16_t b) { return veg_substrate(b) || veg_sand(b); }
/* #minecraft:supports_small_dripleaf */
static int veg_supports_small_dripleaf(uint16_t b) { return b == B_CLAY || b == B_MOSS; }
/* #minecraft:supports_big_dripleaf */
static int veg_supports_big_dripleaf(uint16_t b) {
  return veg_supports_small_dripleaf(b) || veg_supports_vegetation(b) || b == B_MUD || b == B_MUDDY_MANGROVE_ROOTS;
}

/* TreeFeature.validTreePos: air or #replaceable_by_trees. */
static int veg_valid_tree_pos(FeatureCtx *fc, int x, int y, int z) {
  uint16_t b = feat_get(fc, x, y, z);
  return veg_air(b) || block_tag_contains("minecraft:replaceable_by_trees", b);
}
/* TrunkPlacer.isFree: validTreePos or #logs. */
static int veg_is_free(FeatureCtx *fc, int x, int y, int z) {
  uint16_t b = feat_get(fc, x, y, z);
  return veg_air(b) || block_tag_contains("minecraft:replaceable_by_trees", b) || veg_log(b);
}
/* TreeFeature.isAirOrLeaves */
static int veg_air_or_leaves(FeatureCtx *fc, int x, int y, int z) {
  uint16_t b = feat_get(fc, x, y, z);
  return veg_air(b) || veg_leaves(b);
}
static int veg_is_vine(FeatureCtx *fc, int x, int y, int z) { return feat_get(fc, x, y, z) == B_VINE; }

/* MultifaceBlock.canAttachTo: the neighbour exposes a full face towards us. */
static int veg_can_attach_to(FeatureCtx *fc, int x, int y, int z, int dir_towards_neighbour) {
  int nx = x + veg_dx[dir_towards_neighbour], ny = y + veg_dy[dir_towards_neighbour],
      nz = z + veg_dz[dir_towards_neighbour];
  uint16_t nb = feat_get(fc, nx, ny, nz);
  return feat_is_face_sturdy(nb, veg_opp[dir_towards_neighbour]) ||
         feat_occludes_face(nb, veg_opp[dir_towards_neighbour]);
}

/* BlockStateBase.canSurvive for every block this file can place.  Mirrors the
 * block classes one by one (see the file header).  Falls back to the shared
 * helper for blocks owned by the other families. */
static int veg_survive(FeatureCtx *fc, uint16_t b, int x, int y, int z) {
  uint16_t below = feat_get(fc, x, y - 1, z);
  switch (b) {
    /* VegetationBlock / FlowerBlock / BushBlock / SweetBerryBushBlock /
     * FireflyBushBlock / FlowerBedBlock / SaplingBlock */
    case B_SHORT_GRASS:
    case B_FERN:
    case B_BUSH:
    case B_FIREFLY_BUSH:
    case B_SWEET_BERRY_BUSH:
    case B_PINK_PETALS:
    case B_WILDFLOWERS:
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
    case B_WHITE_TULIP:
    case B_CLOSED_EYEBLOSSOM:
    case B_MANGROVE_PROPAGULE:
      return veg_supports_vegetation(below);
    /* DryVegetationBlock (dead_bush / the two dry grasses) */
    case B_DEAD_BUSH:
    case B_SHORT_DRY_GRASS:
    case B_TALL_DRY_GRASS:
      return veg_supports_dry(below);
    /* DoublePlantBlock (lower half): the tall flowers and the tall grasses */
    case B_TALL_GRASS:
    case B_LARGE_FERN:
    case B_SUNFLOWER:
    case B_LILAC:
    case B_ROSE_BUSH:
    case B_PEONY:
      return veg_supports_vegetation(below);
    /* SmallDripleafBlock (lower half) */
    case B_SMALL_DRIPLEAF:
      return veg_supports_small_dripleaf(below) ||
             (veg_water(feat_get(fc, x, y, z)) && veg_supports_vegetation(below));
    /* TallSeagrassBlock (lower half): sturdy support, no magma, full water */
    case B_TALL_SEAGRASS:
      return feat_is_face_sturdy(below, VEG_UP) && below != B_MAGMA && veg_water(feat_get(fc, x, y, z));
    /* SeagrassBlock */
    case B_SEAGRASS:
      return feat_is_face_sturdy(below, VEG_UP) && below != B_MAGMA;
    /* BigDripleafBlock / BigDripleafStemBlock */
    case B_BIG_DRIPLEAF:
    case B_BIG_DRIPLEAF_STEM:
      return below == B_BIG_DRIPLEAF || below == B_BIG_DRIPLEAF_STEM || veg_supports_big_dripleaf(below);
    /* SeaPickleBlock */
    case B_SEA_PICKLE:
      return feat_is_collision_full(below) || feat_is_face_sturdy(below, VEG_UP);
    /* BambooStalkBlock */
    case B_BAMBOO:
      return veg_supports_bamboo(below);
    /* CactusBlock */
    case B_CACTUS: {
      for (int i = 0; i < 4; i++) {
        int d = veg_horiz[i];
        uint16_t nb = feat_get(fc, x + veg_dx[d], y, z + veg_dz[d]);
        if (feat_is_collision_full(nb) || nb == B_LAVA) return 0;
      }
      return (below == B_CACTUS || veg_sand(below)) && !veg_fluid(feat_get(fc, x, y + 1, z));
    }
    /* SugarCaneBlock */
    case B_SUGAR_CANE: {
      if (below == B_SUGAR_CANE) return 1;
      if (veg_supports_cane(below)) {
        for (int i = 0; i < 4; i++) {
          int d = veg_horiz[i];
          if (veg_water(feat_get(fc, x + veg_dx[d], y - 1, z + veg_dz[d]))) return 1;
        }
      }
      return 0;
    }
    /* CarpetBlock (moss_carpet) */
    case B_MOSS_CARPET:
      return !veg_air(below);
    /* MossyCarpetBlock (pale_moss_carpet): BASE=true only */
    case B_PALE_MOSS_CARPET:
      return !veg_air(below);
    /* LeafLitterBlock */
    case B_LEAF_LITTER:
      return feat_is_face_sturdy(below, VEG_UP);
    /* MushroomBlock: light is 0 during worldgen, so only the support matters */
    case B_BROWN_MUSHROOM:
    case B_RED_MUSHROOM:
      return feat_is_collision_full(below) && !veg_leaves(below) && !veg_air(below);
    /* SporeBlossomBlock: centre support above, not water */
    case B_SPORE_BLOSSOM:
      return feat_is_face_sturdy(feat_get(fc, x, y + 1, z), VEG_DOWN) && !veg_water(feat_get(fc, x, y, z));
    /* HangingRootsBlock */
    case B_HANGING_ROOTS:
      return feat_is_face_sturdy(feat_get(fc, x, y + 1, z), VEG_DOWN);
    /* LilyPadBlock: water/ice below, no fluid at the pad itself */
    case B_LILY:
      return (veg_water(below) || below == B_ICE) && !veg_fluid(feat_get(fc, x, y, z));
    /* CocoaBlock */
    case B_COCOA:
      return feat_get(fc, x + 1, y, z) == B_JUNGLE_LOG;
    /* Kelp: GrowingPlantBodyBlock/HeadBlock canSurvive */
    case B_KELP:
    case B_KELP_PLANT:
      return veg_water(below) || below == B_KELP || below == B_KELP_PLANT || feat_is_face_sturdy(below, VEG_UP);
    /* FireBlock: the flammable-neighbour branch is not modelled (nether only) */
    case B_FIRE:
    case B_SOUL_FIRE:
      return feat_is_face_sturdy(below, VEG_UP);
    /* CrimsonRootsBlock (nether only) */
    case B_CRIMSON_ROOTS:
      return below == B_SOUL_SOIL || veg_substrate(below);
    /* PumpkinBlock / MelonBlock / the simple plants with no support rule */
    default:
      return feat_would_survive(fc, b, x, y, z);
  }
}

/* ------------------------------------------------------- provider wrapper */

/* BlockStateProvider.getOptionalState: the shared helper covers every provider
 * type except the three noise-based ones, which need a LegacyRandomSource
 * seeded NormalNoise (NoiseBasedStateProvider).  Those are built here. */

typedef struct {
  double xo, yo, zo;
  uint8_t p[256];
} VegImproved;

typedef struct {
  int nlevels;
  int first_octave;
  VegImproved **levels;
  double *amps;
  double lfif, lfvf;
} VegPerlin;

typedef struct {
  VegPerlin first, second;
  double value_factor;
} VegNormal;

static int32_t veg_java_hash(const char *s) {
  int32_t h = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) h = h * 31 + (int32_t)*p;
  return h;
}

static void veg_improved_init(VegImproved *n, LegacyRng *r) {
  n->xo = legacy_next_double(r) * 256.0;
  n->yo = legacy_next_double(r) * 256.0;
  n->zo = legacy_next_double(r) * 256.0;
  for (int i = 0; i < 256; i++) n->p[i] = (uint8_t)i;
  for (int i = 0; i < 256; i++) {
    int offset = legacy_next_bound(r, 256 - i);
    uint8_t tmp = n->p[i];
    n->p[i] = n->p[i + offset];
    n->p[i + offset] = tmp;
  }
}

static int veg_p(const VegImproved *n, int x) { return n->p[x & 0xFF] & 0xFF; }

/* SimplexNoise.GRADIENT (used by ImprovedNoise.gradDot). */
static const int veg_grad[16][3] = {{1, 1, 0},  {-1, 1, 0},  {1, -1, 0},  {-1, -1, 0},
                                    {1, 0, 1},  {-1, 0, 1},  {1, 0, -1},  {-1, 0, -1},
                                    {0, 1, 1},  {0, -1, 1},  {0, 1, -1},  {0, -1, -1},
                                    {1, 1, 0},  {0, -1, 1},  {-1, 1, 0},  {0, -1, -1}};

static double veg_grad_dot(int hash, double x, double y, double z) {
  const int *g = veg_grad[hash & 15];
  return g[0] * x + g[1] * y + g[2] * z;
}

static double veg_improved_noise(const VegImproved *n, double _x, double _y, double _z, double yScale,
                                 double yFudge) {
  double x = _x + n->xo, y = _y + n->yo, z = _z + n->zo;
  int xf = jfloor(x), yf = jfloor(y), zf = jfloor(z);
  double xr = x - xf, yr = y - yf, zr = z - zf;
  double yr_fudge;
  if (yScale != 0.0) {
    double limit = (yFudge >= 0.0 && yFudge < yr) ? yFudge : yr;
    yr_fudge = (double)jfloor(limit / yScale + 1.0e-7f) * yScale;
  } else {
    yr_fudge = 0.0;
  }
  int x0 = veg_p(n, xf), x1 = veg_p(n, xf + 1);
  int xy00 = veg_p(n, x0 + yf), xy01 = veg_p(n, x0 + yf + 1);
  int xy10 = veg_p(n, x1 + yf), xy11 = veg_p(n, x1 + yf + 1);
  double d000 = veg_grad_dot(veg_p(n, xy00 + zf), xr, yr - yr_fudge, zr);
  double d100 = veg_grad_dot(veg_p(n, xy10 + zf), xr - 1.0, yr - yr_fudge, zr);
  double d010 = veg_grad_dot(veg_p(n, xy01 + zf), xr, yr - yr_fudge - 1.0, zr);
  double d110 = veg_grad_dot(veg_p(n, xy11 + zf), xr - 1.0, yr - yr_fudge - 1.0, zr);
  double d001 = veg_grad_dot(veg_p(n, xy00 + zf + 1), xr, yr - yr_fudge, zr - 1.0);
  double d101 = veg_grad_dot(veg_p(n, xy10 + zf + 1), xr - 1.0, yr - yr_fudge, zr - 1.0);
  double d011 = veg_grad_dot(veg_p(n, xy01 + zf + 1), xr, yr - yr_fudge - 1.0, zr - 1.0);
  double d111 = veg_grad_dot(veg_p(n, xy11 + zf + 1), xr - 1.0, yr - yr_fudge - 1.0, zr - 1.0);
  return jlerp3(jsmoothstep(xr), jsmoothstep(yr), jsmoothstep(zr), d000, d100, d010, d110, d001, d101, d011,
                d111);
}

/* PerlinNoise.wrap */
static double veg_wrap(double x) { return x - (double)jlfloor(x / 3.3554432e7 + 0.5) * 3.3554432e7; }

/* PerlinNoise.create(random, firstOctave, amplitudes) with
 * useNewInitialization = true, over a LegacyRandomSource-backed RandomSource:
 * forkPositional() consumes one nextLong(), then each non-zero octave gets
 * fromHashOf("octave_<n>") = new LegacyRandomSource(name.hashCode() ^ seed). */
static void veg_perlin_init(VegPerlin *p, LegacyRng *r, int first_octave, const double *amps, int nlevels) {
  p->nlevels = nlevels;
  p->first_octave = first_octave;
  p->levels = (VegImproved **)calloc((size_t)nlevels, sizeof(VegImproved *));
  p->amps = (double *)calloc((size_t)nlevels, sizeof(double));
  for (int i = 0; i < nlevels; i++) p->amps[i] = amps[i];
  int64_t factory_seed = legacy_next_long(r);
  for (int i = 0; i < nlevels; i++) {
    if (amps[i] == 0.0) continue;
    char key[32];
    snprintf(key, sizeof key, "octave_%d", first_octave + i);
    LegacyRng sub;
    memset(&sub, 0, sizeof sub); /* LEGACY_RNG_LCG */
    legacy_set_seed(&sub, (uint64_t)(int64_t)((int64_t)veg_java_hash(key) ^ factory_seed));
    p->levels[i] = (VegImproved *)calloc(1, sizeof(VegImproved));
    veg_improved_init(p->levels[i], &sub);
  }
  int zero_octave_index = -first_octave;
  p->lfif = pow(2.0, (double)(-zero_octave_index));
  p->lfvf = pow(2.0, (double)(nlevels - 1)) / (pow(2.0, (double)nlevels) - 1.0);
}

static double veg_perlin_get(const VegPerlin *p, double x, double y, double z) {
  double value = 0.0, factor = p->lfif, value_factor = p->lfvf;
  for (int i = 0; i < p->nlevels; i++) {
    if (p->levels[i]) {
      double nv = veg_improved_noise(p->levels[i], veg_wrap(x * factor), veg_wrap(y * factor),
                                     veg_wrap(z * factor), 0.0, 0.0);
      value += p->amps[i] * nv * value_factor;
    }
    factor *= 2.0;
    value_factor /= 2.0;
  }
  return value;
}

static void veg_perlin_free(VegPerlin *p) {
  for (int i = 0; i < p->nlevels; i++) free(p->levels[i]);
  free(p->levels);
  free(p->amps);
}

/* NormalNoise.create(new WorldgenRandom(new LegacyRandomSource(seed)), params) */
static void veg_normal_create(VegNormal *nn, int64_t seed, int first_octave, const double *amps, int namp) {
  LegacyRng r;
  memset(&r, 0, sizeof r); /* LEGACY_RNG_LCG */
  legacy_set_seed(&r, (uint64_t)seed);
  veg_perlin_init(&nn->first, &r, first_octave, amps, namp);
  veg_perlin_init(&nn->second, &r, first_octave, amps, namp);
  int min_octave = INT32_MAX, max_octave = INT32_MIN;
  for (int i = 0; i < namp; i++) {
    if (amps[i] != 0.0) {
      if (i < min_octave) min_octave = i;
      if (i > max_octave) max_octave = i;
    }
  }
  nn->value_factor = 0.16666666666666666 / (0.1 * (1.0 + 1.0 / (double)(max_octave - min_octave + 1)));
}

static double veg_normal_get(const VegNormal *nn, double x, double y, double z) {
  double x2 = x * 1.0181268882175227, y2 = y * 1.0181268882175227, z2 = z * 1.0181268882175227;
  return (veg_perlin_get(&nn->first, x, y, z) + veg_perlin_get(&nn->second, x2, y2, z2)) * nn->value_factor;
}

static void veg_normal_free(VegNormal *nn) {
  veg_perlin_free(&nn->first);
  veg_perlin_free(&nn->second);
}

/* Jv "noise" parameters -> (first_octave, amplitudes). */
static void veg_noise_params(Jv *np, int *first_octave, double *amps, int *namp) {
  *first_octave = (int)jnum(jget(np, "firstOctave"), 0);
  Jv *a = jget(np, "amplitudes");
  int n = 0;
  for (Jv *c = a ? a->child : 0; c && n < 32; c = c->next) amps[n++] = jnum(c, 0.0);
  *namp = n;
}

/* NoiseProvider.getRandomState(List, double) */
static int veg_noise_state(Jv *states, double noise_value, uint16_t *out) {
  int n = jarr_len(states);
  if (n <= 0) return 0;
  double placement = jclamp((1.0 + noise_value) / 2.0, 0.0, 0.9999);
  Jv *pick = jget_idx(states, (int)(placement * (double)n));
  const char *name = pick ? (pick->kind == J_STR ? pick->str : jstr(jget(pick, "Name"))) : 0;
  int id = block_id_for_name(name);
  if (id < 0) return 0;
  *out = (uint16_t)id;
  return 1;
}

/* NoiseProvider / DualNoiseProvider / NoiseThresholdProvider. */
static int veg_noise_provider(FeatureCtx *fc, Jv *sp, int x, int y, int z, uint16_t *out) {
  const char *type = jstr(jget(sp, "type"));
  if (type && !strncmp(type, "minecraft:", 10)) type += 10;
  if (!type) return 0;
  int dual = !strcmp(type, "dual_noise_provider");
  int threshold_provider = !strcmp(type, "noise_threshold_provider");
  if (!dual && !threshold_provider && strcmp(type, "noise_provider")) return 0;

  int64_t seed = (int64_t)jnum(jget(sp, "seed"), 0);
  double scale = jnum(jget(sp, "scale"), 1.0);
  int first_octave = 0, namp = 0;
  double amps[32];
  veg_noise_params(jget(sp, "noise"), &first_octave, amps, &namp);
  VegNormal noise;
  veg_normal_create(&noise, seed, first_octave, amps, namp);

  if (threshold_provider) {
    double value = veg_normal_get(&noise, (double)x * scale, (double)y * scale, (double)z * scale);
    double threshold = jnum(jget(sp, "threshold"), 0.0);
    double high_chance = jnum(jget(sp, "high_chance"), 0.0);
    int ok = 0;
    if (value < threshold) {
      ok = veg_noise_state(jget(sp, "low_states"), 0, out);
      if (ok) {
        Jv *states = jget(sp, "low_states");
        int n = jarr_len(states);
        if (n > 0) {
          Jv *pick = jget_idx(states, legacy_next_bound(feat_rng(fc), n));
          const char *name = pick->kind == J_STR ? pick->str : jstr(jget(pick, "Name"));
          int id = block_id_for_name(name);
          ok = id >= 0;
          if (ok) *out = (uint16_t)id;
        } else {
          ok = 0;
        }
      }
    } else {
      if (legacy_next_float(feat_rng(fc)) < (float)high_chance) {
        Jv *states = jget(sp, "high_states");
        int n = jarr_len(states);
        if (n > 0) {
          Jv *pick = jget_idx(states, legacy_next_bound(feat_rng(fc), n));
          const char *name = pick->kind == J_STR ? pick->str : jstr(jget(pick, "Name"));
          int id = block_id_for_name(name);
          ok = id >= 0;
          if (ok) *out = (uint16_t)id;
        }
      } else {
        Jv *d = jget(sp, "default_state");
        const char *name = d ? (d->kind == J_STR ? d->str : jstr(jget(d, "Name"))) : 0;
        int id = block_id_for_name(name);
        ok = id >= 0;
        if (ok) *out = (uint16_t)id;
      }
    }
    veg_normal_free(&noise);
    return ok;
  }

  if (dual) {
    int slow_first = 0, slow_namp = 0;
    double slow_amps[32];
    veg_noise_params(jget(sp, "slow_noise"), &slow_first, slow_amps, &slow_namp);
    double slow_scale = jnum(jget(sp, "slow_scale"), 1.0);
    VegNormal slow;
    veg_normal_create(&slow, seed, slow_first, slow_amps, slow_namp);
    Jv *variety = jget(sp, "variety");
    int vmin = (int)jnum(jget_idx(variety, 0), 1);
    int vmax = (int)jnum(jget_idx(variety, 1), 1);
    Jv *states = jget(sp, "states");
    int nstates = jarr_len(states);

    double variety_noise = veg_normal_get(&slow, (double)x * slow_scale, (double)y * slow_scale, (double)z * slow_scale);
    int local_variety = (int)jclamped_map(variety_noise, -1.0, 1.0, (double)vmin, (double)(vmax + 1));
    uint16_t *possible = (uint16_t *)calloc((size_t)(local_variety > 0 ? local_variety : 1), sizeof(uint16_t));
    for (int i = 0; i < local_variety; i++) {
      int px = x + i * 54545, pz = z + i * 34234;
      double nv = veg_normal_get(&slow, (double)px * slow_scale, (double)y * slow_scale, (double)pz * slow_scale);
      uint16_t st = 0;
      if (veg_noise_state(states, nv, &st)) possible[i] = st;
    }
    int ok = 0;
    if (local_variety > 0 && nstates > 0) {
      double nv = veg_normal_get(&noise, (double)x * scale, (double)y * scale, (double)z * scale);
      double placement = jclamp((1.0 + nv) / 2.0, 0.0, 0.9999);
      uint16_t pick = possible[(int)(placement * (double)local_variety)];
      if (pick) {
        *out = pick;
        ok = 1;
      }
    }
    free(possible);
    veg_normal_free(&slow);
    veg_normal_free(&noise);
    return ok;
  }

  {
    double nv = veg_normal_get(&noise, (double)x * scale, (double)y * scale, (double)z * scale);
    int ok = veg_noise_state(jget(sp, "states"), nv, out);
    veg_normal_free(&noise);
    return ok;
  }
}

static int veg_state_provider(FeatureCtx *fc, Jv *sp, int x, int y, int z, uint16_t *out) {
  if (!sp) return 0;
  const char *type = jstr(jget(sp, "type"));
  if (type) {
    const char *t = type;
    if (!strncmp(t, "minecraft:", 10)) t += 10;
    if (!strcmp(t, "noise_provider") || !strcmp(t, "dual_noise_provider") ||
        !strcmp(t, "noise_threshold_provider")) {
      return veg_noise_provider(fc, sp, x, y, z, out);
    }
  }
  return feat_state_provider(fc, sp, x, y, z, out);
}

/* RuleBasedStateProvider.getState (not getOptionalState): falls back to the
 * block already at the position. */
static int veg_state_provider_or_current(FeatureCtx *fc, Jv *sp, int x, int y, int z, uint16_t *out) {
  if (veg_state_provider(fc, sp, x, y, z, out)) return 1;
  *out = feat_get(fc, x, y, z);
  return 1;
}

/* ------------------------------------------------------- placed-feature ref */

/* A PlacedFeature reference in a config is either an inline object or a
 * registry name; the name is resolved against the PLACED feature registry
 * (embed key "pf:<name>").  `feat_apply_placed` runs the placement list. */
static Jv *veg_placed_ref(FeatureCtx *fc, Jv *ref) {
  if (!ref) return 0;
  if (ref->kind != J_STR) return ref;
  char key[160];
  const char *name = ref->str;
  if (!strncmp(name, "minecraft:", 10)) name += 10;
  snprintf(key, sizeof key, "pf:%s", name);
  return embed_json(fc->g, key);
}

/* ConfiguredFeature.place (no placement modifiers): used by the pale-moss
 * decorator, which places the registered PALE_MOSS_PATCH configured feature. */
static int veg_place_configured(FeatureCtx *fc, const char *cf_name, int x, int y, int z) {
  char key[160];
  snprintf(key, sizeof key, "cf:%s", cf_name);
  const char *json = embed_str(key);
  if (!json) return 0;
  if (!feat_in_window(fc, x, y, z)) return 0;
  Jv *cf = jparse(&fc->g->arena, json);
  const char *type = jstr(jget(cf, "type"));
  if (!type) return 0;
  FeatureFn fn = feature_lookup(type);
  if (!fn) return 0;
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  fc->origin_x = x;
  fc->origin_y = y;
  fc->origin_z = z;
  int r = fn(fc, jget(cf, "config"));
  fc->origin_x = ox;
  fc->origin_y = oy;
  fc->origin_z = oz;
  return r;
}

/* --------------------------------------------------------------- the RNG */

static LegacyRng *veg_rng(FeatureCtx *fc) { return feat_rng(fc); }
static int veg_next_bound(FeatureCtx *fc, int bound) { return legacy_next_bound(feat_rng(fc), bound); }
static int veg_next_int(FeatureCtx *fc) { return legacy_next_int(feat_rng(fc)); }
static float veg_next_float(FeatureCtx *fc) { return legacy_next_float(feat_rng(fc)); }
static double veg_next_double(FeatureCtx *fc) { return legacy_next_double(feat_rng(fc)); }
/* BitRandomSource.nextBoolean: next(1) != 0. */
static int veg_next_bool(FeatureCtx *fc) { return legacy_next(feat_rng(fc), 1) != 0; }
/* Mth.randomBetweenInclusive */
static int veg_between(FeatureCtx *fc, int lo, int hi) {
  if (hi < lo) return lo;
  return lo + legacy_next_bound(feat_rng(fc), hi - lo + 1);
}
/* Util.getRandom(List, random) */
static int veg_get_random(FeatureCtx *fc, int n) { return n <= 0 ? 0 : legacy_next_bound(feat_rng(fc), n); }
/* Util.shuffle(List, random) */
static void veg_shuffle(FeatureCtx *fc, int *idx, int n) {
  LegacyRng *r = feat_rng(fc);
  for (int i = n; i > 1; i--) {
    int swap_to = legacy_next_bound(r, i);
    int t = idx[i - 1];
    idx[i - 1] = idx[swap_to];
    idx[swap_to] = t;
  }
}

/* ============================================================== Java sets */

/* TreeFeature keeps its trunk/foliage/root/decoration positions in
 * java.util.HashSet<BlockPos>, and the decorators iterate the set contents
 * (sorted by Y with a stable sort).  To keep the draw order identical the set
 * is emulated here: bucket chains in insertion order, capacity 16 growing by
 * doubling at 0.75 load, index = (h ^ (h >>> 16)) & (cap - 1) where
 * h = Vec3i.hashCode() = (y + z * 31) * 31 + x.  Iteration walks the buckets in
 * index order and each chain in insertion order, which is what HashSet does. */
typedef struct {
  int x, y, z;
} VegPos;

typedef struct {
  VegPos *keys;
  int *hash;
  int *next;
  int *bucket;
  int cap, size, n, grow;
} VegSet;

static int veg_pos_hash(int x, int y, int z) {
  int h = (y + z * 31) * 31 + x;
  return h ^ (int)((uint32_t)h >> 16);
}

static void veg_set_init(VegSet *s) {
  s->keys = 0;
  s->hash = 0;
  s->next = 0;
  s->bucket = 0;
  s->cap = 0;
  s->size = 0;
  s->n = 0;
  s->grow = 16;
}

static void veg_set_free(VegSet *s) {
  free(s->keys);
  free(s->hash);
  free(s->next);
  free(s->bucket);
}

static void veg_set_grow_to(VegSet *s, int cap) {
  int *bucket = (int *)malloc((size_t)cap * sizeof(int));
  for (int i = 0; i < cap; i++) bucket[i] = -1;
  for (int i = 0; i < s->n; i++) {
    int b = s->hash[i] & (cap - 1);
    s->next[i] = bucket[b];
    bucket[b] = i;
  }
  free(s->bucket);
  s->bucket = bucket;
  s->cap = cap;
}

/* Java's resize splits each bucket into a lo/hi list preserving order, so the
 * chains stay in insertion order; rebuilding from the entry list (which is in
 * insertion order) with tail insertion reproduces exactly that. */
static void veg_set_resize(VegSet *s, int cap) {
  int *bucket = (int *)malloc((size_t)cap * sizeof(int));
  int *tail = (int *)malloc((size_t)cap * sizeof(int));
  for (int i = 0; i < cap; i++) bucket[i] = -1;
  for (int i = 0; i < s->n; i++) {
    int b = s->hash[i] & (cap - 1);
    s->next[i] = -1;
    if (bucket[b] < 0) {
      bucket[b] = i;
    } else {
      s->next[tail[b]] = i;
    }
    tail[b] = i;
  }
  free(tail);
  free(s->bucket);
  s->bucket = bucket;
  s->cap = cap;
}

static int veg_set_contains(const VegSet *s, int x, int y, int z) {
  if (!s->cap) return 0;
  int h = veg_pos_hash(x, y, z);
  for (int i = s->bucket[h & (s->cap - 1)]; i >= 0; i = s->next[i]) {
    if (s->keys[i].x == x && s->keys[i].y == y && s->keys[i].z == z) return 1;
  }
  return 0;
}

static int veg_set_add(VegSet *s, int x, int y, int z) {
  if (veg_set_contains(s, x, y, z)) return 0;
  if (!s->cap) veg_set_resize(s, 16);
  if (!s->keys || s->n == s->grow) {
    int grow = s->keys ? s->grow * 2 : s->grow;
    s->keys = (VegPos *)realloc(s->keys, (size_t)grow * sizeof(VegPos));
    s->hash = (int *)realloc(s->hash, (size_t)grow * sizeof(int));
    s->next = (int *)realloc(s->next, (size_t)grow * sizeof(int));
    s->grow = grow;
  }
  int h = veg_pos_hash(x, y, z);
  int i = s->n++;
  s->keys[i].x = x;
  s->keys[i].y = y;
  s->keys[i].z = z;
  s->hash[i] = h;
  int b = h & (s->cap - 1);
  s->next[i] = -1;
  if (s->bucket[b] < 0) {
    s->bucket[b] = i;
  } else {
    int t = s->bucket[b];
    while (s->next[t] >= 0) t = s->next[t];
    s->next[t] = i;
  }
  if (++s->size > (s->cap * 3) / 4) veg_set_resize(s, s->cap * 2);
  return 1;
}

/* Materialise the set in HashSet iteration order. */
static int veg_set_to_array(const VegSet *s, VegPos *out) {
  int n = 0;
  for (int b = 0; b < s->cap; b++) {
    for (int i = s->bucket[b]; i >= 0; i = s->next[i]) out[n++] = s->keys[i];
  }
  return n;
}

/* List.sort(Comparator.comparingInt(Vec3i::getY)) is a stable sort. */
static void veg_stable_sort_y(VegPos *a, int n) {
  for (int i = 1; i < n; i++) {
    VegPos key = a[i];
    int j = i - 1;
    while (j >= 0 && a[j].y > key.y) {
      a[j + 1] = a[j];
      j--;
    }
    a[j + 1] = key;
  }
}
/* ====================================================== tree scaffolding */

typedef struct {
  int x, y, z;
  int radius_offset;
  int double_trunk;
} VegAttach;

typedef struct {
  VegAttach *a;
  int n, cap;
} VegAttachList;

static void veg_attach_add(VegAttachList *l, int x, int y, int z, int radius_offset, int double_trunk) {
  if (l->n == l->cap) {
    l->cap = l->cap ? l->cap * 2 : 8;
    l->a = (VegAttach *)realloc(l->a, (size_t)l->cap * sizeof(VegAttach));
  }
  l->a[l->n].x = x;
  l->a[l->n].y = y;
  l->a[l->n].z = z;
  l->a[l->n].radius_offset = radius_offset;
  l->a[l->n].double_trunk = double_trunk;
  l->n++;
}

typedef struct {
  FeatureCtx *fc;
  VegSet roots, logs, foliage, decorations;
  Jv *trunk_provider, *foliage_provider, *below_trunk_provider;
  Jv *trunk_placer, *foliage_placer, *minimum_size, *root_placer;
  Jv *decorators;
  Jv *can_grow_through; /* UpwardsBranchingTrunkPlacer only */
  int ignore_vines;
} VegTree;

static void veg_tree_init(VegTree *t, FeatureCtx *fc, Jv *config) {
  memset(t, 0, sizeof *t);
  t->fc = fc;
  veg_set_init(&t->roots);
  veg_set_init(&t->logs);
  veg_set_init(&t->foliage);
  veg_set_init(&t->decorations);
  t->trunk_provider = jget(config, "trunk_provider");
  t->foliage_provider = jget(config, "foliage_provider");
  t->below_trunk_provider = jget(config, "below_trunk_provider");
  t->trunk_placer = jget(config, "trunk_placer");
  t->foliage_placer = jget(config, "foliage_placer");
  t->minimum_size = jget(config, "minimum_size");
  t->root_placer = jget(config, "root_placer");
  t->decorators = jget(config, "decorators");
  t->ignore_vines = jbool(jget(config, "ignore_vines"), 0);
}

static void veg_tree_free(VegTree *t) {
  veg_set_free(&t->roots);
  veg_set_free(&t->logs);
  veg_set_free(&t->foliage);
  veg_set_free(&t->decorations);
}

/* BiConsumer<BlockPos, BlockState> setters: record the position in the set and
 * write the block (WorldGenRegion.setBlock drops writes outside the window). */
static void veg_root_set(VegTree *t, int x, int y, int z, uint16_t b) {
  veg_set_add(&t->roots, x, y, z);
  feat_set(t->fc, x, y, z, b);
}
static void veg_trunk_set(VegTree *t, int x, int y, int z, uint16_t b) {
  veg_set_add(&t->logs, x, y, z);
  feat_set(t->fc, x, y, z, b);
}
static void veg_foliage_set(VegTree *t, int x, int y, int z, uint16_t b) {
  veg_set_add(&t->foliage, x, y, z);
  feat_set(t->fc, x, y, z, b);
}

static int veg_holderset_contains(Jv *hs, uint16_t b) {
  if (!hs) return 0;
  if (hs->kind == J_STR) {
    const char *s = hs->str;
    if (s[0] == '#') return block_tag_contains(s + 1, b);
    return (uint16_t)block_id_for_name(s) == b;
  }
  for (Jv *c = hs->child; c; c = c->next) {
    const char *name = c->kind == J_STR ? c->str : jstr(jget(c, "Name"));
    if (name && name[0] == '#') {
      if (block_tag_contains(name + 1, b)) return 1;
    } else if (name && (uint16_t)block_id_for_name(name) == b) {
      return 1;
    }
  }
  return 0;
}

/* TrunkPlacer.validTreePos, with the UpwardsBranchingTrunkPlacer override. */
static int veg_trunk_valid_pos(VegTree *t, int x, int y, int z) {
  if (veg_valid_tree_pos(t->fc, x, y, z)) return 1;
  if (t->can_grow_through) return veg_holderset_contains(t->can_grow_through, feat_get(t->fc, x, y, z));
  return 0;
}
static int veg_trunk_is_free(VegTree *t, int x, int y, int z) {
  if (veg_trunk_valid_pos(t, x, y, z)) return 1;
  return veg_log(feat_get(t->fc, x, y, z));
}

/* TrunkPlacer.placeLog */
static int veg_place_log(VegTree *t, int x, int y, int z) {
  if (!veg_trunk_valid_pos(t, x, y, z)) return 0;
  uint16_t b = 0;
  if (veg_state_provider(t->fc, t->trunk_provider, x, y, z, &b)) veg_trunk_set(t, x, y, z, b);
  return 1;
}
/* TrunkPlacer.placeLogIfFree */
static void veg_place_log_if_free(VegTree *t, int x, int y, int z) {
  if (veg_trunk_is_free(t, x, y, z)) veg_place_log(t, x, y, z);
}
/* TrunkPlacer.placeBelowTrunkBlock (belowTrunkProvider.getOptionalState) */
static void veg_place_below_trunk(VegTree *t, int x, int y, int z) {
  uint16_t b = 0;
  if (!veg_state_provider(t->fc, t->below_trunk_provider, x, y, z, &b)) return;
  veg_trunk_set(t, x, y, z, b);
}

/* TrunkPlacer.getTreeHeight */
static int veg_tree_height(FeatureCtx *fc, Jv *tp) {
  int base = (int)jnum(jget(tp, "base_height"), 0);
  int a = (int)jnum(jget(tp, "height_rand_a"), 0);
  int b = (int)jnum(jget(tp, "height_rand_b"), 0);
  return base + veg_next_bound(fc, a + 1) + veg_next_bound(fc, b + 1);
}

/* FeatureSize.getSizeAtHeight / minClippedHeight */
static int veg_size_at_height(Jv *ms, int tree_height, int y) {
  const char *type = jstr(jget(ms, "type"));
  if (type && !strncmp(type, "minecraft:", 10)) type += 10;
  if (type && !strcmp(type, "three_layers_feature_size")) {
    int limit = (int)jnum(jget(ms, "limit"), 1);
    int upper_limit = (int)jnum(jget(ms, "upper_limit"), 1);
    int lower = (int)jnum(jget(ms, "lower_size"), 0);
    int middle = (int)jnum(jget(ms, "middle_size"), 1);
    int upper = (int)jnum(jget(ms, "upper_size"), 1);
    if (y < limit) return lower;
    return y >= tree_height - upper_limit ? upper : middle;
  }
  int limit = (int)jnum(jget(ms, "limit"), 1);
  int lower = (int)jnum(jget(ms, "lower_size"), 0);
  int upper = (int)jnum(jget(ms, "upper_size"), 1);
  return y < limit ? lower : upper;
}
static int veg_min_clipped_height(Jv *ms, int *out) {
  Jv *v = jget(ms, "min_clipped_height");
  if (!v) return 0;
  *out = (int)jnum(v, 0);
  return 1;
}

/* Mth.cos / Mth.sin (the 65536-entry table; computed identically here). */
static float veg_mth_cos(double i) {
  long idx = (long)(i * 10430.378350470453 + 16384.0) & 65535L;
  return (float)sin((double)idx / 10430.378350470453);
}
static float veg_mth_sin(double i) {
  long idx = (long)(i * 10430.378350470453) & 65535L;
  return (float)sin((double)idx / 10430.378350470453);
}

/* IntProvider.sample.  `feat_int_provider` covers the typed providers; a bare
 * UniformInt (IntProviders.validateCodec over UniformInt.MAP_CODEC, e.g.
 * cherry's branch_start_offset_from_top) has no "type" field, so it is handled
 * here. */
static int veg_int_provider(Jv *p, LegacyRng *r) {
  if (!p) return 0;
  if (p->kind == J_NUM) return (int)p->num;
  const char *type = jstr(jget(p, "type"));
  if (!type) {
    Jv *lo = jget(p, "min_inclusive");
    Jv *hi = jget(p, "max_inclusive");
    if (lo && hi) {
      int a = (int)jnum(lo, 0), b = (int)jnum(hi, 0);
      if (b < a) return a;
      return a + legacy_next_bound(r, b - a + 1);
    }
    return (int)jnum(p, 0);
  }
  return feat_int_provider(p, r);
}
static int veg_floor_f(float v) { return (int)floor((double)v); }
static int veg_abs(int v) { return v < 0 ? -v : v; }
static int veg_max(int a, int b) { return a > b ? a : b; }
static int veg_min(int a, int b) { return a < b ? a : b; }

/* ------------------------------------------------------------ trunk placers */

static void veg_place_trunk(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out);

/* StraightTrunkPlacer */
static void veg_trunk_straight(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out) {
  veg_place_below_trunk(t, ox, oy - 1, oz);
  for (int y = 0; y < tree_height; y++) veg_place_log(t, ox, oy + y, oz);
  veg_attach_add(out, ox, oy + tree_height, oz, 0, 0);
}

/* ForkingTrunkPlacer */
static void veg_trunk_forking(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out) {
  FeatureCtx *fc = t->fc;
  veg_place_below_trunk(t, ox, oy - 1, oz);
  int lean_direction = veg_horiz[veg_next_bound(fc, 4)];
  int lean_height = tree_height - veg_next_bound(fc, 4) - 1;
  int lean_steps = 3 - veg_next_bound(fc, 3);
  int tx = ox, tz = oz;
  int ey = -1;
  for (int yo = 0; yo < tree_height; yo++) {
    int yy = oy + yo;
    if (yo >= lean_height && lean_steps > 0) {
      tx += veg_dx[lean_direction];
      tz += veg_dz[lean_direction];
      lean_steps--;
    }
    if (veg_place_log(t, tx, yy, tz)) ey = yy + 1;
  }
  if (ey >= 0) veg_attach_add(out, tx, ey, tz, 1, 0);

  tx = ox;
  tz = oz;
  int branch_direction = veg_horiz[veg_next_bound(fc, 4)];
  if (branch_direction != lean_direction) {
    int branch_pos = lean_height - veg_next_bound(fc, 2) - 1;
    int branch_steps = 1 + veg_next_bound(fc, 3);
    ey = -1;
    for (int yo = branch_pos; yo < tree_height && branch_steps > 0; branch_steps--) {
      if (yo >= 1) {
        int yy = oy + yo;
        tx += veg_dx[branch_direction];
        tz += veg_dz[branch_direction];
        if (veg_place_log(t, tx, yy, tz)) ey = yy + 1;
      }
      yo++;
    }
    if (ey >= 0) veg_attach_add(out, tx, ey, tz, 0, 0);
  }
}

/* BendingTrunkPlacer */
static void veg_trunk_bending(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out) {
  FeatureCtx *fc = t->fc;
  Jv *tp = t->trunk_placer;
  int min_height_for_leaves = (int)jnum(jget(tp, "min_height_for_leaves"), 1);
  int direction = veg_horiz[veg_next_bound(fc, 4)];
  int log_height = tree_height - 1;
  int px = ox, py = oy, pz = oz;
  veg_place_below_trunk(t, ox, oy - 1, oz);
  for (int i = 0; i <= log_height; i++) {
    if (i + 1 >= log_height + veg_next_bound(fc, 2)) {
      px += veg_dx[direction];
      pz += veg_dz[direction];
    }
    if (veg_valid_tree_pos(fc, px, py, pz)) veg_place_log(t, px, py, pz);
    if (i >= min_height_for_leaves) veg_attach_add(out, px, py, pz, 0, 0);
    py += 1;
  }
  int dir_length = veg_int_provider(jget(tp, "bend_length"), veg_rng(fc));
  for (int i = 0; i <= dir_length; i++) {
    if (veg_valid_tree_pos(fc, px, py, pz)) veg_place_log(t, px, py, pz);
    veg_attach_add(out, px, py, pz, 0, 0);
    px += veg_dx[direction];
    pz += veg_dz[direction];
  }
}

/* GiantTrunkPlacer */
static void veg_trunk_giant(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out) {
  veg_place_below_trunk(t, ox, oy - 1, oz);
  veg_place_below_trunk(t, ox + 1, oy - 1, oz);
  veg_place_below_trunk(t, ox, oy - 1, oz + 1);
  veg_place_below_trunk(t, ox + 1, oy - 1, oz + 1);
  for (int hh = 0; hh < tree_height; hh++) {
    veg_place_log_if_free(t, ox, oy + hh, oz);
    if (hh < tree_height - 1) {
      veg_place_log_if_free(t, ox + 1, oy + hh, oz);
      veg_place_log_if_free(t, ox + 1, oy + hh, oz + 1);
      veg_place_log_if_free(t, ox, oy + hh, oz + 1);
    }
  }
  veg_attach_add(out, ox, oy + tree_height, oz, 0, 1);
}

/* MegaJungleTrunkPlacer extends GiantTrunkPlacer */
static void veg_trunk_mega_jungle(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out) {
  FeatureCtx *fc = t->fc;
  veg_trunk_giant(t, tree_height, ox, oy, oz, out);
  for (int branch_height = tree_height - 2 - veg_next_bound(fc, 4); branch_height > tree_height / 2;
       branch_height -= 2 + veg_next_bound(fc, 4)) {
    float angle = veg_next_float(fc) * (float)(M_PI * 2);
    int bx = 0, bz = 0;
    for (int b = 0; b < 5; b++) {
      bx = (int)(1.5f + veg_mth_cos((double)angle) * (float)b);
      bz = (int)(1.5f + veg_mth_sin((double)angle) * (float)b);
      veg_place_log(t, ox + bx, oy + branch_height - 3 + b / 2, oz + bz);
    }
    veg_attach_add(out, ox + bx, oy + branch_height, oz + bz, -2, 0);
  }
}

/* DarkOakTrunkPlacer */
static void veg_trunk_dark_oak(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out) {
  FeatureCtx *fc = t->fc;
  veg_place_below_trunk(t, ox, oy - 1, oz);
  veg_place_below_trunk(t, ox + 1, oy - 1, oz);
  veg_place_below_trunk(t, ox, oy - 1, oz + 1);
  veg_place_below_trunk(t, ox + 1, oy - 1, oz + 1);
  int lean_direction = veg_horiz[veg_next_bound(fc, 4)];
  int lean_height = tree_height - veg_next_bound(fc, 4);
  int lean_steps = 2 - veg_next_bound(fc, 3);
  int tx = ox, tz = oz;
  int ey = oy + tree_height - 1;
  for (int dy = 0; dy < tree_height; dy++) {
    if (dy >= lean_height && lean_steps > 0) {
      tx += veg_dx[lean_direction];
      tz += veg_dz[lean_direction];
      lean_steps--;
    }
    int yy = oy + dy;
    if (veg_air_or_leaves(fc, tx, yy, tz)) {
      veg_place_log(t, tx, yy, tz);
      veg_place_log(t, tx + 1, yy, tz);
      veg_place_log(t, tx, yy, tz + 1);
      veg_place_log(t, tx + 1, yy, tz + 1);
    }
  }
  veg_attach_add(out, tx, ey, tz, 0, 1);

  for (int oxx = -1; oxx <= 2; oxx++) {
    for (int ozz = -1; ozz <= 2; ozz++) {
      if ((oxx < 0 || oxx > 1 || ozz < 0 || ozz > 1) && veg_next_bound(fc, 3) <= 0) {
        int length = veg_next_bound(fc, 3) + 2;
        for (int branch_y = 0; branch_y < length; branch_y++) {
          veg_place_log(t, ox + oxx, ey - branch_y - 1, oz + ozz);
        }
        veg_attach_add(out, ox + oxx, ey, oz + ozz, 0, 0);
      }
    }
  }
}

/* FancyTrunkPlacer */
static float veg_fancy_tree_shape(int height, int y) {
  if ((float)y < (float)height * 0.3f) return -1.0f;
  float radius = (float)height / 2.0f;
  float adjacent = radius - (float)y;
  float distance = (float)sqrt((double)(radius * radius - adjacent * adjacent));
  if (adjacent == 0.0f) {
    distance = radius;
  } else if ((float)fabs((double)adjacent) >= radius) {
    return 0.0f;
  }
  return distance * 0.5f;
}

static int veg_fancy_get_steps(int dx, int dy, int dz) {
  int ax = veg_abs(dx), ay = veg_abs(dy), az = veg_abs(dz);
  return veg_max(ax, veg_max(ay, az));
}

static int veg_fancy_make_limb(VegTree *t, int sx, int sy, int sz, int ex, int ey, int ez, int do_place) {
  if (!do_place && sx == ex && sy == ey && sz == ez) return 1;
  int ddx = ex - sx, ddy = ey - sy, ddz = ez - sz;
  int steps = veg_fancy_get_steps(ddx, ddy, ddz);
  float dx = steps ? (float)ddx / (float)steps : 0.0f;
  float dy = steps ? (float)ddy / (float)steps : 0.0f;
  float dz = steps ? (float)ddz / (float)steps : 0.0f;
  for (int i = 0; i <= steps; i++) {
    int bx = sx + veg_floor_f(0.5f + (float)i * dx);
    int by = sy + veg_floor_f(0.5f + (float)i * dy);
    int bz = sz + veg_floor_f(0.5f + (float)i * dz);
    if (do_place) {
      veg_place_log(t, bx, by, bz);
    } else if (!veg_trunk_is_free(t, bx, by, bz)) {
      return 0;
    }
  }
  return 1;
}

static int veg_fancy_trim_branches(int height, int local_y) { return (double)local_y >= (double)height * 0.2; }

static void veg_trunk_fancy(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out) {
  FeatureCtx *fc = t->fc;
  int height = tree_height + 2;
  int trunk_height = (int)floor((double)height * 0.618);
  veg_place_below_trunk(t, ox, oy - 1, oz);
  int clusters_per_y = veg_min(1, (int)floor(1.382 + pow(1.0 * (double)height / 13.0, 2.0)));
  int trunk_top = oy + trunk_height;
  int relative_y = height - 5;
  /* FoliageCoords: attachment + branchBase */
  int *fx = 0, *fy = 0, *fz = 0, *fbase = 0;
  int nfc = 0, cfc = 0;
  int first_x = ox, first_y = oy + relative_y, first_z = oz;
  (void)first_x;
  (void)first_y;
  (void)first_z;

#define VEG_FC_PUSH(px, py, pz, base)                     \
  do {                                                    \
    if (nfc == cfc) {                                     \
      cfc = cfc ? cfc * 2 : 8;                            \
      fx = (int *)realloc(fx, (size_t)cfc * sizeof(int)); \
      fy = (int *)realloc(fy, (size_t)cfc * sizeof(int)); \
      fz = (int *)realloc(fz, (size_t)cfc * sizeof(int)); \
      fbase = (int *)realloc(fbase, (size_t)cfc * sizeof(int)); \
    }                                                     \
    fx[nfc] = (px);                                       \
    fy[nfc] = (py);                                       \
    fz[nfc] = (pz);                                       \
    fbase[nfc] = (base);                                  \
    nfc++;                                                \
  } while (0)

  VEG_FC_PUSH(ox, oy + relative_y, oz, trunk_top);
  for (; relative_y >= 0; relative_y--) {
    float shape = veg_fancy_tree_shape(height, relative_y);
    if (!(shape < 0.0f)) {
      for (int i = 0; i < clusters_per_y; i++) {
        double radius = 1.0 * (double)shape * ((double)veg_next_float(fc) + 0.328);
        double angle = (double)(veg_next_float(fc) * 2.0f) * M_PI;
        double xx = radius * sin(angle) + 0.5;
        double zz = radius * cos(angle) + 0.5;
        int csx = ox + (int)floor(xx), csy = oy + relative_y - 1, csz = oz + (int)floor(zz);
        if (veg_fancy_make_limb(t, csx, csy, csz, csx, csy + 5, csz, 0)) {
          int ddx = ox - csx, ddz = oz - csz;
          double branch_height = (double)csy - sqrt((double)(ddx * ddx + ddz * ddz)) * 0.381;
          int branch_top = branch_height > (double)trunk_top ? trunk_top : (int)branch_height;
          if (veg_fancy_make_limb(t, ox, branch_top, oz, csx, csy, csz, 0)) {
            VEG_FC_PUSH(csx, csy, csz, branch_top);
          }
        }
      }
    }
  }
  veg_fancy_make_limb(t, ox, oy, oz, ox, oy + trunk_height, oz, 1);
  /* makeBranches */
  for (int i = 0; i < nfc; i++) {
    int base = fbase[i];
    int bx = ox, by = base, bz = oz;
    if (!(bx == fx[i] && by == fy[i] && bz == fz[i]) && veg_fancy_trim_branches(height, base - oy)) {
      veg_fancy_make_limb(t, bx, by, bz, fx[i], fy[i], fz[i], 1);
    }
  }
  for (int i = 0; i < nfc; i++) {
    if (veg_fancy_trim_branches(height, fbase[i] - oy)) {
      veg_attach_add(out, fx[i], fy[i], fz[i], 0, 0);
    }
  }
  free(fx);
  free(fy);
  free(fz);
  free(fbase);
#undef VEG_FC_PUSH
}

/* CherryTrunkPlacer */
static void veg_trunk_cherry_generate_branch(VegTree *t, int tree_height, int ox, int oy, int oz,
                                             int branch_direction, int offset_from_origin,
                                             int middle_continues_upwards, VegAttachList *out) {
  FeatureCtx *fc = t->fc;
  Jv *tp = t->trunk_placer;
  int lx = ox, ly = oy + offset_from_origin, lz = oz;
  int end_offset = tree_height - 1 + veg_int_provider(jget(tp, "branch_end_offset_from_top"), veg_rng(fc));
  int extend_away = middle_continues_upwards || end_offset < offset_from_origin;
  int distance_to_trunk = veg_int_provider(jget(tp, "branch_horizontal_length"), veg_rng(fc)) + (extend_away ? 1 : 0);
  int bex = ox + veg_dx[branch_direction] * distance_to_trunk;
  int bey = oy + end_offset;
  int bez = oz + veg_dz[branch_direction] * distance_to_trunk;
  int steps_horizontally = extend_away ? 2 : 1;
  for (int i = 0; i < steps_horizontally; i++) {
    lx += veg_dx[branch_direction];
    lz += veg_dz[branch_direction];
    veg_place_log(t, lx, ly, lz);
  }
  int vertical_direction = bey > ly ? VEG_UP : VEG_DOWN;
  for (;;) {
    int distance = veg_abs(lx - bex) + veg_abs(ly - bey) + veg_abs(lz - bez);
    if (distance == 0) {
      veg_attach_add(out, bex, bey + 1, bez, 0, 0);
      return;
    }
    float chance = (float)veg_abs(bey - ly) / (float)distance;
    int grow_vertically = veg_next_float(fc) < chance;
    if (grow_vertically) {
      lx += veg_dx[vertical_direction];
      ly += veg_dy[vertical_direction];
      lz += veg_dz[vertical_direction];
    } else {
      lx += veg_dx[branch_direction];
      lz += veg_dz[branch_direction];
    }
    veg_place_log(t, lx, ly, lz);
  }
}

static void veg_trunk_cherry(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out) {
  FeatureCtx *fc = t->fc;
  Jv *tp = t->trunk_placer;
  veg_place_below_trunk(t, ox, oy - 1, oz);
  Jv *start = jget(tp, "branch_start_offset_from_top");
  int smin = (int)jnum(jget(start, "min_inclusive"), 0);
  int smax = (int)jnum(jget(start, "max_inclusive"), 0);
  int first_offset = veg_max(0, tree_height - 1 + veg_int_provider(start, veg_rng(fc)));
  int sbound = smax - 1 - smin + 1; /* UniformInt.of(smin, smax - 1).sample */
  int second_sample = sbound <= 0 ? smin : smin + legacy_next_bound(feat_rng(fc), sbound);
  int second_offset = veg_max(0, tree_height - 1 + second_sample);
  if (second_offset >= first_offset) second_offset++;
  int branch_count = veg_int_provider(jget(tp, "branch_count"), veg_rng(fc));
  int has_middle = branch_count == 3;
  int has_both = branch_count >= 2;
  int trunk_height;
  if (has_middle) {
    trunk_height = tree_height;
  } else if (has_both) {
    trunk_height = veg_max(first_offset, second_offset) + 1;
  } else {
    trunk_height = first_offset + 1;
  }
  for (int y = 0; y < trunk_height; y++) veg_place_log(t, ox, oy + y, oz);
  if (has_middle) veg_attach_add(out, ox, oy + trunk_height, oz, 0, 0);
  int tree_direction = veg_horiz[veg_next_bound(fc, 4)];
  veg_trunk_cherry_generate_branch(t, tree_height, ox, oy, oz, tree_direction, first_offset,
                                   first_offset < trunk_height - 1, out);
  if (has_both) {
    veg_trunk_cherry_generate_branch(t, tree_height, ox, oy, oz, veg_opp[tree_direction], second_offset,
                                     second_offset < trunk_height - 1, out);
  }
}

/* UpwardsBranchingTrunkPlacer */
static void veg_trunk_upwards_branch(VegTree *t, int tree_height, int current_height, int log_x, int log_z,
                                     int branch_dir, int branch_pos, int branch_steps, VegAttachList *out) {
  int height_along_branch = current_height + branch_pos;
  int lx = log_x, lz = log_z;
  int branch_placement_index = branch_pos;
  while (branch_placement_index < tree_height && branch_steps > 0) {
    if (branch_placement_index >= 1) {
      int placement_height = current_height + branch_placement_index;
      lx += veg_dx[branch_dir];
      lz += veg_dz[branch_dir];
      height_along_branch = placement_height;
      if (veg_place_log(t, lx, placement_height, lz)) height_along_branch++;
      veg_attach_add(out, lx, placement_height, lz, 0, 0);
    }
    branch_placement_index++;
    branch_steps--;
  }
  if (height_along_branch - current_height > 1) {
    veg_attach_add(out, lx, height_along_branch, lz, 0, 0);
    veg_attach_add(out, lx, height_along_branch - 2, lz, 0, 0);
  }
}

static void veg_trunk_upwards_branching(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out) {
  FeatureCtx *fc = t->fc;
  Jv *tp = t->trunk_placer;
  float branch_probability = (float)jnum(jget(tp, "place_branch_per_log_probability"), 0.0);
  for (int height_pos = 0; height_pos < tree_height; height_pos++) {
    int current_height = oy + height_pos;
    if (veg_place_log(t, ox, current_height, oz) && height_pos < tree_height - 1 &&
        veg_next_float(fc) < branch_probability) {
      int branch_dir = veg_horiz[veg_next_bound(fc, 4)];
      int branch_len = veg_int_provider(jget(tp, "extra_branch_length"), veg_rng(fc));
      int branch_pos = veg_max(0, branch_len - veg_int_provider(jget(tp, "extra_branch_length"), veg_rng(fc)) - 1);
      int branch_steps = veg_int_provider(jget(tp, "extra_branch_steps"), veg_rng(fc));
      veg_trunk_upwards_branch(t, tree_height, current_height, ox, oz, branch_dir, branch_pos, branch_steps, out);
    }
    if (height_pos == tree_height - 1) veg_attach_add(out, ox, current_height + 1, oz, 0, 0);
  }
}

static void veg_place_trunk(VegTree *t, int tree_height, int ox, int oy, int oz, VegAttachList *out) {
  const char *type = jstr(jget(t->trunk_placer, "type"));
  if (type && !strncmp(type, "minecraft:", 10)) type += 10;
  if (!type) return;
  if (!strcmp(type, "straight_trunk_placer")) {
    veg_trunk_straight(t, tree_height, ox, oy, oz, out);
  } else if (!strcmp(type, "forking_trunk_placer")) {
    veg_trunk_forking(t, tree_height, ox, oy, oz, out);
  } else if (!strcmp(type, "bending_trunk_placer")) {
    veg_trunk_bending(t, tree_height, ox, oy, oz, out);
  } else if (!strcmp(type, "giant_trunk_placer")) {
    veg_trunk_giant(t, tree_height, ox, oy, oz, out);
  } else if (!strcmp(type, "mega_jungle_trunk_placer")) {
    veg_trunk_mega_jungle(t, tree_height, ox, oy, oz, out);
  } else if (!strcmp(type, "dark_oak_trunk_placer")) {
    veg_trunk_dark_oak(t, tree_height, ox, oy, oz, out);
  } else if (!strcmp(type, "fancy_trunk_placer")) {
    veg_trunk_fancy(t, tree_height, ox, oy, oz, out);
  } else if (!strcmp(type, "cherry_trunk_placer")) {
    veg_trunk_cherry(t, tree_height, ox, oy, oz, out);
  } else if (!strcmp(type, "upwards_branching_trunk_placer")) {
    t->can_grow_through = jget(t->trunk_placer, "can_grow_through");
    veg_trunk_upwards_branching(t, tree_height, ox, oy, oz, out);
  }
}
/* ---------------------------------------------------------- foliage placers */

enum {
  VFP_BLOB,
  VFP_BUSH,
  VFP_FANCY,
  VFP_ACACIA,
  VFP_DARK_OAK,
  VFP_CHERRY,
  VFP_MEGA_JUNGLE,
  VFP_MEGA_PINE,
  VFP_PINE,
  VFP_SPRUCE,
  VFP_RANDOM_SPREAD
};

static int veg_fp_kind(Jv *fp) {
  const char *type = jstr(jget(fp, "type"));
  if (type && !strncmp(type, "minecraft:", 10)) type += 10;
  if (!type) return -1;
  if (!strcmp(type, "blob_foliage_placer")) return VFP_BLOB;
  if (!strcmp(type, "bush_foliage_placer")) return VFP_BUSH;
  if (!strcmp(type, "fancy_foliage_placer")) return VFP_FANCY;
  if (!strcmp(type, "acacia_foliage_placer")) return VFP_ACACIA;
  if (!strcmp(type, "dark_oak_foliage_placer")) return VFP_DARK_OAK;
  if (!strcmp(type, "cherry_foliage_placer")) return VFP_CHERRY;
  if (!strcmp(type, "jungle_foliage_placer")) return VFP_MEGA_JUNGLE;
  if (!strcmp(type, "mega_pine_foliage_placer")) return VFP_MEGA_PINE;
  if (!strcmp(type, "pine_foliage_placer")) return VFP_PINE;
  if (!strcmp(type, "spruce_foliage_placer")) return VFP_SPRUCE;
  if (!strcmp(type, "random_spread_foliage_placer")) return VFP_RANDOM_SPREAD;
  return -1;
}

static int veg_fp_int(Jv *fp, const char *key, int def) { return (int)jnum(jget(fp, key), def); }
static double veg_fp_float(Jv *fp, const char *key) { return jnum(jget(fp, key), 0.0); }

static int veg_fp_radius(FeatureCtx *fc, Jv *fp) {
  int r = veg_int_provider(jget(fp, "radius"), veg_rng(fc));
  return r;
}

static int veg_fp_foliage_height(FeatureCtx *fc, Jv *fp, int tree_height) {
  int kind = veg_fp_kind(fp);
  switch (kind) {
    case VFP_ACACIA:
      return 0;
    case VFP_DARK_OAK:
      return 4;
    case VFP_CHERRY:
      return veg_int_provider(jget(fp, "height"), veg_rng(fc));
    case VFP_MEGA_PINE:
      return veg_int_provider(jget(fp, "crown_height"), veg_rng(fc));
    case VFP_PINE:
      return veg_int_provider(jget(fp, "height"), veg_rng(fc));
    case VFP_SPRUCE:
      return veg_max(4, tree_height - veg_int_provider(jget(fp, "trunk_height"), veg_rng(fc)));
    case VFP_RANDOM_SPREAD:
      return veg_int_provider(jget(fp, "foliage_height"), veg_rng(fc));
    default:
      return veg_fp_int(fp, "height", 0);
  }
}

static int veg_fp_foliage_radius(FeatureCtx *fc, Jv *fp, int trunk_height) {
  int r = veg_fp_radius(fc, fp);
  if (veg_fp_kind(fp) == VFP_PINE) r += veg_next_bound(fc, veg_max(trunk_height + 1, 1));
  return r;
}

static int veg_fp_should_skip(FeatureCtx *fc, Jv *fp, int dx, int y, int dz, int current_radius,
                              int double_trunk) {
  int kind = veg_fp_kind(fp);
  switch (kind) {
    case VFP_BLOB:
      return dx == current_radius && dz == current_radius &&
             (veg_next_bound(fc, 2) == 0 || y == 0);
    case VFP_BUSH:
      return dx == current_radius && dz == current_radius && veg_next_bound(fc, 2) == 0;
    case VFP_FANCY: {
      float a = (float)dx + 0.5f, b = (float)dz + 0.5f;
      return a * a + b * b > (float)(current_radius * current_radius);
    }
    case VFP_ACACIA:
      if (y == 0) return (dx > 1 || dz > 1) && dx != 0 && dz != 0;
      return dx == current_radius && dz == current_radius && current_radius > 0;
    case VFP_DARK_OAK:
      if (y == -1 && !double_trunk) return dx == current_radius && dz == current_radius;
      return y == 1 ? dx + dz > current_radius * 2 - 2 : 0;
    case VFP_CHERRY: {
      double wide = veg_fp_float(fp, "wide_bottom_layer_hole_chance");
      double corner_hole = veg_fp_float(fp, "corner_hole_chance");
      if (y == -1 && (dx == current_radius || dz == current_radius) && veg_next_float(fc) < (float)wide)
        return 1;
      int corner = dx == current_radius && dz == current_radius;
      int wide_layer = current_radius > 2;
      if (wide_layer) {
        if (corner) return 1;
        return dx + dz > current_radius * 2 - 2 && veg_next_float(fc) < (float)corner_hole;
      }
      return corner && veg_next_float(fc) < (float)corner_hole;
    }
    case VFP_MEGA_JUNGLE:
    case VFP_MEGA_PINE:
      return dx + dz >= 7 || dx * dx + dz * dz > current_radius * current_radius;
    case VFP_PINE:
    case VFP_SPRUCE:
      return dx == current_radius && dz == current_radius && current_radius > 0;
    default:
      return 0;
  }
}

/* FoliagePlacer.shouldSkipLocationSigned */
static int veg_fp_should_skip_signed(FeatureCtx *fc, Jv *fp, int dx, int y, int dz, int current_radius,
                                     int double_trunk) {
  int min_dx, min_dz;
  if (double_trunk) {
    min_dx = veg_min(veg_abs(dx), veg_abs(dx - 1));
    min_dz = veg_min(veg_abs(dz), veg_abs(dz - 1));
  } else {
    min_dx = veg_abs(dx);
    min_dz = veg_abs(dz);
  }
  if (veg_fp_kind(fp) == VFP_DARK_OAK) {
    /* DarkOakFoliagePlacer.shouldSkipLocationSigned */
    if (!(y != 0 || !double_trunk || (dx != -current_radius && dx < current_radius) ||
          (dz != -current_radius && dz < current_radius)))
      return 1;
  }
  return veg_fp_should_skip(fc, fp, min_dx, y, min_dz, current_radius, double_trunk);
}

/* FoliagePlacer.tryPlaceLeaf */
static int veg_try_place_leaf(VegTree *t, int x, int y, int z) {
  FeatureCtx *fc = t->fc;
  if (!veg_valid_tree_pos(fc, x, y, z)) return 0;
  uint16_t b = 0;
  if (!veg_state_provider(fc, t->foliage_provider, x, y, z, &b)) return 0;
  veg_foliage_set(t, x, y, z, b);
  return 1;
}

/* FoliagePlacer.placeLeavesRow */
static void veg_place_leaves_row(VegTree *t, Jv *fp, int ox, int oy, int oz, int current_radius, int y,
                                 int double_trunk) {
  int offset = double_trunk ? 1 : 0;
  for (int dx = -current_radius; dx <= current_radius + offset; dx++) {
    for (int dz = -current_radius; dz <= current_radius + offset; dz++) {
      if (!veg_fp_should_skip_signed(t->fc, fp, dx, y, dz, current_radius, double_trunk)) {
        veg_try_place_leaf(t, ox + dx, oy + y, oz + dz);
      }
    }
  }
}

/* FoliagePlacer.tryPlaceExtension */
static int veg_try_place_extension(VegTree *t, Jv *fp, float chance, int log_x, int log_y, int log_z, int x,
                                   int y, int z) {
  if (veg_abs(x - log_x) + veg_abs(y - log_y) + veg_abs(z - log_z) >= 7) return 0;
  if (veg_next_float(t->fc) > chance) return 0;
  return veg_try_place_leaf(t, x, y, z);
}

/* FoliagePlacer.placeLeavesRowWithHangingLeavesBelow */
static void veg_place_leaves_row_hanging(VegTree *t, Jv *fp, int ox, int oy, int oz, int current_radius,
                                         int y, int double_trunk) {
  veg_place_leaves_row(t, fp, ox, oy, oz, current_radius, y, double_trunk);
  int offset = double_trunk ? 1 : 0;
  int log_x = ox, log_y = oy - 1, log_z = oz;
  float hanging = (float)veg_fp_float(fp, "hanging_leaves_chance");
  float extension = (float)veg_fp_float(fp, "hanging_leaves_extension_chance");
  for (int hi = 0; hi < 4; hi++) {
    int along_edge = veg_horiz[hi];
    int to_edge = veg_cw(along_edge);
    int offset_to_edge = veg_pos_axis_dir(to_edge) ? current_radius + offset : current_radius;
    int px = ox + veg_dx[to_edge] * offset_to_edge + veg_dx[along_edge] * (-current_radius);
    int py = oy + y - 1;
    int pz = oz + veg_dz[to_edge] * offset_to_edge + veg_dz[along_edge] * (-current_radius);
    int offset_along_edge = -current_radius;
    while (offset_along_edge < current_radius + offset) {
      int ux = px, uy = py + 1, uz = pz;
      int leaves_above = veg_set_contains(&t->foliage, ux, uy, uz);
      if (leaves_above && veg_try_place_extension(t, fp, hanging, log_x, log_y, log_z, px, py, pz)) {
        veg_try_place_extension(t, fp, extension, log_x, log_y, log_z, px, py - 1, pz);
      }
      offset_along_edge++;
      px += veg_dx[along_edge];
      pz += veg_dz[along_edge];
    }
  }
}

static void veg_fp_create_foliage(VegTree *t, Jv *fp, VegAttach *a, int foliage_height, int leaf_radius,
                                  int offset, int tree_height) {
  FeatureCtx *fc = t->fc;
  int kind = veg_fp_kind(fp);
  int radius_offset = a->radius_offset;
  int double_trunk = a->double_trunk;
  switch (kind) {
    case VFP_BLOB:
      for (int yo = offset; yo >= offset - foliage_height; yo--) {
        int r = veg_max(leaf_radius + radius_offset - 1 - yo / 2, 0);
        veg_place_leaves_row(t, fp, a->x, a->y, a->z, r, yo, double_trunk);
      }
      break;
    case VFP_BUSH:
      for (int yo = offset; yo >= offset - foliage_height; yo--) {
        int r = leaf_radius + radius_offset - 1 - yo;
        veg_place_leaves_row(t, fp, a->x, a->y, a->z, r, yo, double_trunk);
      }
      break;
    case VFP_FANCY:
      for (int yo = offset; yo >= offset - foliage_height; yo--) {
        int r = leaf_radius + ((yo != offset && yo != offset - foliage_height) ? 1 : 0);
        veg_place_leaves_row(t, fp, a->x, a->y, a->z, r, yo, double_trunk);
      }
      break;
    case VFP_ACACIA:
      veg_place_leaves_row(t, fp, a->x, a->y + offset, a->z, leaf_radius + radius_offset, -1 - foliage_height,
                           double_trunk);
      veg_place_leaves_row(t, fp, a->x, a->y + offset, a->z, leaf_radius - 1, -foliage_height, double_trunk);
      veg_place_leaves_row(t, fp, a->x, a->y + offset, a->z, leaf_radius + radius_offset - 1, 0, double_trunk);
      break;
    case VFP_DARK_OAK:
      if (double_trunk) {
        veg_place_leaves_row(t, fp, a->x, a->y + offset, a->z, leaf_radius + 2, -1, double_trunk);
        veg_place_leaves_row(t, fp, a->x, a->y + offset, a->z, leaf_radius + 3, 0, double_trunk);
        veg_place_leaves_row(t, fp, a->x, a->y + offset, a->z, leaf_radius + 2, 1, double_trunk);
        if (veg_next_bool(fc)) veg_place_leaves_row(t, fp, a->x, a->y + offset, a->z, leaf_radius, 2, double_trunk);
      } else {
        veg_place_leaves_row(t, fp, a->x, a->y + offset, a->z, leaf_radius + 2, -1, double_trunk);
        veg_place_leaves_row(t, fp, a->x, a->y + offset, a->z, leaf_radius + 1, 0, double_trunk);
      }
      break;
    case VFP_CHERRY: {
      int current_radius = leaf_radius + radius_offset - 1;
      int fy = a->y + offset;
      veg_place_leaves_row(t, fp, a->x, fy, a->z, current_radius - 2, foliage_height - 3, double_trunk);
      veg_place_leaves_row(t, fp, a->x, fy, a->z, current_radius - 1, foliage_height - 4, double_trunk);
      for (int y = foliage_height - 5; y >= 0; y--) {
        veg_place_leaves_row(t, fp, a->x, fy, a->z, current_radius, y, double_trunk);
      }
      veg_place_leaves_row_hanging(t, fp, a->x, fy, a->z, current_radius, -1, double_trunk);
      veg_place_leaves_row_hanging(t, fp, a->x, fy, a->z, current_radius - 1, -2, double_trunk);
      break;
    }
    case VFP_MEGA_JUNGLE: {
      int leaf_height = double_trunk ? foliage_height : 1 + veg_next_bound(fc, 2);
      for (int yo = offset; yo >= offset - leaf_height; yo--) {
        int r = leaf_radius + radius_offset + 1 - yo;
        veg_place_leaves_row(t, fp, a->x, a->y, a->z, r, yo, double_trunk);
      }
      break;
    }
    case VFP_MEGA_PINE: {
      int prev_radius = 0;
      int base_y = a->y - foliage_height + offset;
      for (int yy = base_y; yy <= a->y + offset; yy++) {
        int yo = a->y - yy;
        int smooth = leaf_radius + radius_offset + veg_floor_f((float)yo / (float)foliage_height * 3.5f);
        int jagged;
        if (yo > 0 && smooth == prev_radius && (yy & 1) == 0) {
          jagged = smooth + 1;
        } else {
          jagged = smooth;
        }
        veg_place_leaves_row(t, fp, a->x, yy, a->z, jagged, 0, double_trunk);
        prev_radius = smooth;
      }
      break;
    }
    case VFP_PINE: {
      int current_radius = 0;
      for (int yo = offset; yo >= offset - foliage_height; yo--) {
        veg_place_leaves_row(t, fp, a->x, a->y, a->z, current_radius, yo, double_trunk);
        if (current_radius >= 1 && yo == offset - foliage_height + 1) {
          current_radius--;
        } else if (current_radius < leaf_radius + radius_offset) {
          current_radius++;
        }
      }
      break;
    }
    case VFP_SPRUCE: {
      int current_radius = veg_next_bound(fc, 2);
      int max_radius = 1, min_radius = 0;
      for (int yo = offset; yo >= -foliage_height; yo--) {
        veg_place_leaves_row(t, fp, a->x, a->y, a->z, current_radius, yo, double_trunk);
        if (current_radius >= max_radius) {
          current_radius = min_radius;
          min_radius = 1;
          max_radius = veg_min(max_radius + 1, leaf_radius + radius_offset);
        } else {
          current_radius++;
        }
      }
      break;
    }
    case VFP_RANDOM_SPREAD: {
      int attempts = veg_fp_int(fp, "leaf_placement_attempts", 0);
      for (int i = 0; i < attempts; i++) {
        int dx = veg_next_bound(fc, leaf_radius) - veg_next_bound(fc, leaf_radius);
        int dy = veg_next_bound(fc, foliage_height) - veg_next_bound(fc, foliage_height);
        int dz = veg_next_bound(fc, leaf_radius) - veg_next_bound(fc, leaf_radius);
        veg_try_place_leaf(t, a->x + dx, a->y + dy, a->z + dz);
      }
      break;
    }
    default:
      break;
  }
  (void)tree_height;
}

/* FoliagePlacer.createFoliage (public): samples the offset first. */
static void veg_create_foliage(VegTree *t, Jv *fp, VegAttach *a, int foliage_height, int leaf_radius,
                               int tree_height) {
  int offset = veg_int_provider(jget(fp, "offset"), veg_rng(t->fc));
  veg_fp_create_foliage(t, fp, a, foliage_height, leaf_radius, offset, tree_height);
}

/* ----------------------------------------------------------- root placers */

/* MangroveRootPlacer.placeRoots (+ RootPlacer.placeRoot / canPlaceRoot). */
static int veg_root_place_one(VegTree *t, Jv *rp, int x, int y, int z) {
  FeatureCtx *fc = t->fc;
  uint16_t here = feat_get(fc, x, y, z);
  Jv *mr = jget(rp, "mangrove_root_placement");
  if (mr && veg_holderset_contains(jget(mr, "muddy_roots_in"), here)) {
    uint16_t b = 0;
    if (veg_state_provider(fc, jget(mr, "muddy_roots_provider"), x, y, z, &b)) veg_root_set(t, x, y, z, b);
    return 1;
  }
  /* RootPlacer.canPlaceRoot: TreeFeature.validTreePos */
  if (!veg_valid_tree_pos(fc, x, y, z)) return 0;
  uint16_t b = 0;
  if (veg_state_provider(fc, jget(rp, "root_provider"), x, y, z, &b)) veg_root_set(t, x, y, z, b);
  Jv *above_place = jget(rp, "above_root_placement");
  if (above_place) {
    float chance = (float)jnum(jget(above_place, "above_root_placement_chance"), 0.0);
    if (veg_next_float(fc) < chance && veg_air(feat_get(fc, x, y + 1, z))) {
      uint16_t ab = 0;
      if (veg_state_provider(fc, jget(above_place, "above_root_provider"), x, y + 1, z, &ab))
        veg_root_set(t, x, y + 1, z, ab);
    }
  }
  return 1;
}

static int veg_root_can_place(VegTree *t, Jv *rp, int x, int y, int z) {
  if (veg_valid_tree_pos(t->fc, x, y, z)) return 1;
  Jv *mr = jget(rp, "mangrove_root_placement");
  if (mr && veg_holderset_contains(jget(mr, "can_grow_through"), feat_get(t->fc, x, y, z))) return 1;
  return 0;
}

/* potentialRootPositions: writes up to two candidate positions. */
static int veg_root_potential(VegTree *t, Jv *rp, int px, int py, int pz, int dir, int rx, int ry, int rz,
                              int out_x[2], int out_y[2], int out_z[2]) {
  Jv *mr = jget(rp, "mangrove_root_placement");
  int max_width = (int)jnum(jget(mr, "max_root_width"), 8);
  float skew = (float)jnum(jget(mr, "random_skew_chance"), 0.2);
  int below_x = px, below_y = py - 1, below_z = pz;
  int next_x = px + veg_dx[dir], next_y = py, next_z = pz + veg_dz[dir];
  int width = veg_abs(px - rx) + veg_abs(py - ry) + veg_abs(pz - rz);
  if (width > max_width - 3 && width <= max_width) {
    if (veg_next_float(t->fc) < skew) {
      out_x[0] = below_x; out_y[0] = below_y; out_z[0] = below_z;
      out_x[1] = next_x; out_y[1] = next_y - 1; out_z[1] = next_z;
      return 2;
    }
    out_x[0] = below_x; out_y[0] = below_y; out_z[0] = below_z;
    return 1;
  }
  if (width > max_width) {
    out_x[0] = below_x; out_y[0] = below_y; out_z[0] = below_z;
    return 1;
  }
  if (veg_next_float(t->fc) < skew) {
    out_x[0] = below_x; out_y[0] = below_y; out_z[0] = below_z;
    return 1;
  }
  if (veg_next_bool(t->fc)) {
    out_x[0] = next_x; out_y[0] = next_y; out_z[0] = next_z;
  } else {
    out_x[0] = below_x; out_y[0] = below_y; out_z[0] = below_z;
  }
  return 1;
}

typedef struct {
  VegPos *a;
  int n, cap;
} VegPosList;

static void veg_plist_push(VegPosList *l, int x, int y, int z) {
  if (l->n == l->cap) {
    l->cap = l->cap ? l->cap * 2 : 16;
    l->a = (VegPos *)realloc(l->a, (size_t)l->cap * sizeof(VegPos));
  }
  l->a[l->n].x = x;
  l->a[l->n].y = y;
  l->a[l->n].z = z;
  l->n++;
}

static int veg_root_simulate(VegTree *t, Jv *rp, int px, int py, int pz, int dir, int rx, int ry, int rz,
                             VegPosList *list, int layer) {
  Jv *mr = jget(rp, "mangrove_root_placement");
  int max_length = (int)jnum(jget(mr, "max_root_length"), 15);
  if (layer != max_length && list->n <= max_length) {
    int ox[2], oy[2], oz[2];
    int cnt = veg_root_potential(t, rp, px, py, pz, dir, rx, ry, rz, ox, oy, oz);
    for (int i = 0; i < cnt; i++) {
      if (veg_root_can_place(t, rp, ox[i], oy[i], oz[i])) {
        veg_plist_push(list, ox[i], oy[i], oz[i]);
        if (!veg_root_simulate(t, rp, ox[i], oy[i], oz[i], dir, rx, ry, rz, list, layer + 1)) {
          return 0;
        }
      }
    }
    return 1;
  }
  return 0;
}

static int veg_place_roots(VegTree *t, int ox, int oy, int oz, int trunk_x, int trunk_y, int trunk_z) {
  Jv *rp = t->root_placer;
  VegPosList list;
  list.a = 0;
  list.n = 0;
  list.cap = 0;
  int cy = oy;
  while (cy < trunk_y) {
    if (!veg_root_can_place(t, rp, ox, cy, oz)) {
      free(list.a);
      return 0;
    }
    cy++;
  }
  veg_plist_push(&list, trunk_x, trunk_y - 1, trunk_z);
  for (int hi = 0; hi < 4; hi++) {
    int dir = veg_horiz[hi];
    int px = trunk_x + veg_dx[dir], py = trunk_y, pz = trunk_z + veg_dz[dir];
    if (!veg_root_simulate(t, rp, px, py, pz, dir, trunk_x, trunk_y, trunk_z, &list, 0)) {
      free(list.a);
      return 0;
    }
    veg_plist_push(&list, px, py, pz);
  }
  for (int i = 0; i < list.n; i++) veg_root_place_one(t, rp, list.a[i].x, list.a[i].y, list.a[i].z);
  free(list.a);
  return 1;
}

/* RootPlacer.getTrunkOrigin */
static int veg_root_trunk_origin(FeatureCtx *fc, Jv *rp, int oy) {
  return oy + veg_int_provider(jget(rp, "trunk_offset_y"), veg_rng(fc));
}
/* ------------------------------------------------------------- decorators */

typedef struct {
  VegTree *t;
  VegPos *logs;
  int nlogs;
  VegPos *leaves;
  int nleaves;
  VegPos *roots;
  int nroots;
} VegDeco;

static void veg_deco_build(VegTree *t, VegDeco *d) {
  memset(d, 0, sizeof *d);
  d->t = t;
  d->logs = (VegPos *)calloc((size_t)(t->logs.n ? t->logs.n : 1), sizeof(VegPos));
  d->leaves = (VegPos *)calloc((size_t)(t->foliage.n ? t->foliage.n : 1), sizeof(VegPos));
  d->roots = (VegPos *)calloc((size_t)(t->roots.n ? t->roots.n : 1), sizeof(VegPos));
  d->nlogs = veg_set_to_array(&t->logs, d->logs);
  d->nleaves = veg_set_to_array(&t->foliage, d->leaves);
  d->nroots = veg_set_to_array(&t->roots, d->roots);
  veg_stable_sort_y(d->logs, d->nlogs);
  veg_stable_sort_y(d->leaves, d->nleaves);
  veg_stable_sort_y(d->roots, d->nroots);
}
static void veg_deco_free(VegDeco *d) {
  free(d->logs);
  free(d->leaves);
  free(d->roots);
}
static void veg_deco_set_b(VegDeco *d, int x, int y, int z, uint16_t b) {
  veg_set_add(&d->t->decorations, x, y, z);
  feat_set(d->t->fc, x, y, z, b);
}
static int veg_deco_is_air(VegDeco *d, int x, int y, int z) { return veg_air(feat_get(d->t->fc, x, y, z)); }

/* Util.shuffle over two parallel arrays (beehive's hivePlacements). */
static void veg_shuffle_slots(FeatureCtx *fc, int *px, int *pz, int n) {
  LegacyRng *r = veg_rng(fc);
  for (int i = n; i > 1; i--) {
    int s = legacy_next_bound(r, i);
    int t = px[i - 1];
    px[i - 1] = px[s];
    px[s] = t;
    t = pz[i - 1];
    pz[i - 1] = pz[s];
    pz[s] = t;
  }
}

/* TreeFeature.getLowestTrunkOrRootOfTree */
static VegPos *veg_lowest_trunk_or_root(VegDeco *d, int *out_n) {
  VegPos *out = (VegPos *)calloc((size_t)(d->nlogs + d->nroots + 1), sizeof(VegPos));
  int n = 0;
  if (d->nroots == 0) {
    for (int i = 0; i < d->nlogs; i++) out[n++] = d->logs[i];
  } else if (d->nlogs > 0 && d->roots[0].y == d->logs[0].y) {
    for (int i = 0; i < d->nlogs; i++) out[n++] = d->logs[i];
    for (int i = 0; i < d->nroots; i++) out[n++] = d->roots[i];
  } else {
    for (int i = 0; i < d->nroots; i++) out[n++] = d->roots[i];
  }
  *out_n = n;
  return out;
}

/* BeehiveDecorator */
static void veg_deco_beehive(VegDeco *d, Jv *cfg) {
  FeatureCtx *fc = d->t->fc;
  float probability = (float)jnum(jget(cfg, "probability"), 0.0);
  if (d->nlogs == 0) return;
  if (!(veg_next_float(fc) < probability)) return;
  int hive_y;
  if (d->nleaves > 0) {
    hive_y = veg_max(d->leaves[0].y - 1, d->logs[0].y + 1);
  } else {
    hive_y = veg_min(d->logs[0].y + 1 + veg_next_bound(fc, 3), d->logs[d->nlogs - 1].y);
  }
  /* SPAWN_DIRECTIONS = Plane.HORIZONTAL minus NORTH (= SOUTH.getOpposite()) */
  static const int spawn[3] = {VEG_EAST, VEG_SOUTH, VEG_WEST};
  int *px = 0, *pz = 0, n = 0, cap = 0;
  for (int i = 0; i < d->nlogs; i++) {
    if (d->logs[i].y != hive_y) continue;
    for (int s = 0; s < 3; s++) {
      if (n == cap) {
        cap = cap ? cap * 2 : 16;
        px = (int *)realloc(px, (size_t)cap * sizeof(int));
        pz = (int *)realloc(pz, (size_t)cap * sizeof(int));
      }
      px[n] = d->logs[i].x + veg_dx[spawn[s]];
      pz[n] = d->logs[i].z + veg_dz[spawn[s]];
      n++;
    }
  }
  if (n > 0) {
    veg_shuffle_slots(fc, px, pz, n);
    for (int i = 0; i < n; i++) {
      int x = px[i], z = pz[i];
      if (veg_deco_is_air(d, x, hive_y, z) && veg_deco_is_air(d, x, hive_y, z + 1)) {
        veg_deco_set_b(d, x, hive_y, z, B_BEE_NEST);
        /* WorldGenRegion.setBlock creates the DUMMY block entity, so the
         * beehive's occupant list is filled and those draws do happen. */
        int num_bees = 2 + veg_next_bound(fc, 2);
        for (int b = 0; b < num_bees; b++) (void)veg_next_bound(fc, 599);
        break;
      }
    }
  }
  free(px);
  free(pz);
}

/* CocoaDecorator */
static void veg_deco_cocoa(VegDeco *d, Jv *cfg) {
  FeatureCtx *fc = d->t->fc;
  float probability = (float)jnum(jget(cfg, "probability"), 0.0);
  if (!(veg_next_float(fc) < probability)) return;
  if (d->nlogs == 0) return;
  int tree_y = d->logs[0].y;
  for (int i = 0; i < d->nlogs; i++) {
    if (d->logs[i].y - tree_y > 2) continue;
    for (int hi = 0; hi < 4; hi++) {
      int dir = veg_horiz[hi];
      if (veg_next_float(fc) <= 0.25f) {
        int opp = veg_opp[dir];
        int x = d->logs[i].x + veg_dx[opp], z = d->logs[i].z + veg_dz[opp];
        if (veg_deco_is_air(d, x, d->logs[i].y, z)) {
          (void)veg_next_bound(fc, 3); /* CocoaBlock.AGE */
          veg_deco_set_b(d, x, d->logs[i].y, z, B_COCOA);
        }
      }
    }
  }
}

/* TrunkVineDecorator */
static void veg_deco_trunk_vine(VegDeco *d) {
  FeatureCtx *fc = d->t->fc;
  for (int i = 0; i < d->nlogs; i++) {
    int x = d->logs[i].x, y = d->logs[i].y, z = d->logs[i].z;
    if (veg_next_bound(fc, 3) > 0 && veg_deco_is_air(d, x - 1, y, z)) veg_deco_set_b(d, x - 1, y, z, B_VINE);
    if (veg_next_bound(fc, 3) > 0 && veg_deco_is_air(d, x + 1, y, z)) veg_deco_set_b(d, x + 1, y, z, B_VINE);
    if (veg_next_bound(fc, 3) > 0 && veg_deco_is_air(d, x, y, z - 1)) veg_deco_set_b(d, x, y, z - 1, B_VINE);
    if (veg_next_bound(fc, 3) > 0 && veg_deco_is_air(d, x, y, z + 1)) veg_deco_set_b(d, x, y, z + 1, B_VINE);
  }
}

/* LeaveVineDecorator */
static void veg_deco_leave_vine(VegDeco *d, Jv *cfg) {
  FeatureCtx *fc = d->t->fc;
  float probability = (float)jnum(jget(cfg, "probability"), 0.0);
  for (int i = 0; i < d->nleaves; i++) {
    int x = d->leaves[i].x, y = d->leaves[i].y, z = d->leaves[i].z;
    for (int hi = 0; hi < 4; hi++) {
      if (!(veg_next_float(fc) < probability)) continue;
      int dir = veg_horiz[hi];
      int vx = x + veg_dx[dir], vz = z + veg_dz[dir];
      if (!veg_deco_is_air(d, vx, y, vz)) continue;
      veg_deco_set_b(d, vx, y, vz, B_VINE);
      int max_dir = 4;
      int cy = y - 1;
      while (veg_deco_is_air(d, vx, cy, vz) && max_dir > 0) {
        veg_deco_set_b(d, vx, cy, vz, B_VINE);
        cy--;
        max_dir--;
      }
    }
  }
}

/* PlaceOnGroundDecorator */
static void veg_deco_place_on_ground(VegDeco *d, Jv *cfg) {
  FeatureCtx *fc = d->t->fc;
  int tries = (int)jnum(jget(cfg, "tries"), 128);
  int radius = (int)jnum(jget(cfg, "radius"), 2);
  int height = (int)jnum(jget(cfg, "height"), 1);
  Jv *provider = jget(cfg, "block_state_provider");
  int np = 0;
  VegPos *bp = veg_lowest_trunk_or_root(d, &np);
  if (np == 0) {
    free(bp);
    return;
  }
  int min_y = bp[0].y;
  int min_x = bp[0].x, max_x = bp[0].x, min_z = bp[0].z, max_z = bp[0].z;
  for (int i = 0; i < np; i++) {
    if (bp[i].y != min_y) continue;
    min_x = veg_min(min_x, bp[i].x);
    max_x = veg_max(max_x, bp[i].x);
    min_z = veg_min(min_z, bp[i].z);
    max_z = veg_max(max_z, bp[i].z);
  }
  int bx0 = min_x - radius, bx1 = max_x + radius;
  int by0 = min_y - height, by1 = min_y + height;
  int bz0 = min_z - radius, bz1 = max_z + radius;
  for (int i = 0; i < tries; i++) {
    int x = veg_between(fc, bx0, bx1);
    int y = veg_between(fc, by0, by1);
    int z = veg_between(fc, bz0, bz1);
    int above_y = y + 1;
    uint16_t above = feat_get(fc, x, above_y, z);
    if (!(veg_air(above) || above == B_VINE)) continue;
    uint16_t below = feat_get(fc, x, y, z);
    if (!feat_is_collision_full(below)) continue;
    if (feat_height(fc, x, z, FEAT_HM_MOTION_BLOCKING_NO_LEAVES) > above_y) continue;
    uint16_t b = 0;
    if (veg_state_provider(fc, provider, x, above_y, z, &b)) veg_deco_set_b(d, x, above_y, z, b);
  }
  free(bp);
}

/* AlterGroundDecorator */
static void veg_alter_ground_at(VegDeco *d, Jv *provider, int x, int y, int z) {
  FeatureCtx *fc = d->t->fc;
  for (int dy = 2; dy >= -3; dy--) {
    int cy = y + dy;
    uint16_t b = 0;
    if (veg_state_provider(fc, provider, x, cy, z, &b)) {
      veg_deco_set_b(d, x, cy, z, b);
      break;
    }
    if (!veg_deco_is_air(d, x, cy, z) && dy < 0) break;
  }
}
static void veg_alter_ground_circle(VegDeco *d, Jv *provider, int x, int y, int z) {
  for (int xx = -2; xx <= 2; xx++) {
    for (int zz = -2; zz <= 2; zz++) {
      if (veg_abs(xx) != 2 || veg_abs(zz) != 2) veg_alter_ground_at(d, provider, x + xx, y, z + zz);
    }
  }
}
static void veg_deco_alter_ground(VegDeco *d, Jv *cfg) {
  FeatureCtx *fc = d->t->fc;
  Jv *provider = jget(cfg, "provider");
  int np = 0;
  VegPos *bp = veg_lowest_trunk_or_root(d, &np);
  if (np == 0) {
    free(bp);
    return;
  }
  int min_y = bp[0].y;
  for (int i = 0; i < np; i++) {
    if (bp[i].y != min_y) continue;
    int x = bp[i].x, z = bp[i].z;
    veg_alter_ground_circle(d, provider, x - 1, min_y, z - 1);
    veg_alter_ground_circle(d, provider, x + 2, min_y, z - 1);
    veg_alter_ground_circle(d, provider, x - 1, min_y, z + 2);
    veg_alter_ground_circle(d, provider, x + 2, min_y, z + 2);
    for (int k = 0; k < 5; k++) {
      int placement = veg_next_bound(fc, 64);
      int xx = placement % 8, zz = placement / 8;
      if (xx == 0 || xx == 7 || zz == 0 || zz == 7) {
        veg_alter_ground_circle(d, provider, x - 3 + xx, min_y, z - 3 + zz);
      }
    }
  }
  free(bp);
}

/* PaleMossDecorator */
static void veg_moss_hanger(VegDeco *d, int x, int y, int z) {
  FeatureCtx *fc = d->t->fc;
  while (veg_deco_is_air(d, x, y - 1, z) && !(veg_next_float(fc) < 0.5f)) {
    veg_deco_set_b(d, x, y, z, B_PALE_HANGING_MOSS);
    y--;
  }
  veg_deco_set_b(d, x, y, z, B_PALE_HANGING_MOSS);
}
static void veg_deco_pale_moss(VegDeco *d, Jv *cfg) {
  FeatureCtx *fc = d->t->fc;
  float leaves_p = (float)jnum(jget(cfg, "leaves_probability"), 0.0);
  float trunk_p = (float)jnum(jget(cfg, "trunk_probability"), 0.0);
  float ground_p = (float)jnum(jget(cfg, "ground_probability"), 0.0);
  if (d->nlogs == 0) return;
  int *idx = (int *)calloc((size_t)d->nlogs, sizeof(int));
  for (int i = 0; i < d->nlogs; i++) idx[i] = i;
  veg_shuffle(fc, idx, d->nlogs);
  int best = idx[0];
  for (int i = 0; i < d->nlogs; i++) {
    if (d->logs[idx[i]].y < d->logs[best].y) best = idx[i];
  }
  if (veg_next_float(fc) < ground_p) {
    veg_place_configured(fc, "pale_moss_patch", d->logs[best].x, d->logs[best].y + 1, d->logs[best].z);
  }
  for (int i = 0; i < d->nlogs; i++) {
    if (veg_next_float(fc) < trunk_p && veg_deco_is_air(d, d->logs[i].x, d->logs[i].y - 1, d->logs[i].z))
      veg_moss_hanger(d, d->logs[i].x, d->logs[i].y - 1, d->logs[i].z);
  }
  for (int i = 0; i < d->nleaves; i++) {
    if (veg_next_float(fc) < leaves_p && veg_deco_is_air(d, d->leaves[i].x, d->leaves[i].y - 1, d->leaves[i].z))
      veg_moss_hanger(d, d->leaves[i].x, d->leaves[i].y - 1, d->leaves[i].z);
  }
  free(idx);
}

/* CreakingHeartDecorator */
static void veg_deco_creaking_heart(VegDeco *d, Jv *cfg) {
  FeatureCtx *fc = d->t->fc;
  float probability = (float)jnum(jget(cfg, "probability"), 0.0);
  if (d->nlogs == 0) return;
  if (veg_next_float(fc) >= probability) return;
  int *idx = (int *)calloc((size_t)d->nlogs, sizeof(int));
  for (int i = 0; i < d->nlogs; i++) idx[i] = i;
  veg_shuffle(fc, idx, d->nlogs);
  for (int i = 0; i < d->nlogs; i++) {
    int x = d->logs[idx[i]].x, y = d->logs[idx[i]].y, z = d->logs[idx[i]].z;
    int all = 1;
    for (int dir = 0; dir < 6; dir++) {
      if (!veg_log(feat_get(fc, x + veg_dx[dir], y + veg_dy[dir], z + veg_dz[dir]))) {
        all = 0;
        break;
      }
    }
    if (all) {
      veg_deco_set_b(d, x, y, z, B_CREAKING_HEART);
      break;
    }
  }
  free(idx);
}

/* AttachedToLeavesDecorator */
static void veg_deco_attached_to_leaves(VegDeco *d, Jv *cfg) {
  FeatureCtx *fc = d->t->fc;
  float probability = (float)jnum(jget(cfg, "probability"), 0.0);
  int ex_xz = (int)jnum(jget(cfg, "exclusion_radius_xz"), 0);
  int ex_y = (int)jnum(jget(cfg, "exclusion_radius_y"), 0);
  int required_empty = (int)jnum(jget(cfg, "required_empty_blocks"), 1);
  Jv *provider = jget(cfg, "block_provider");
  Jv *dirs = jget(cfg, "directions");
  int ndirs = jarr_len(dirs);
  if (ndirs <= 0) return;
  VegSet blacklist;
  veg_set_init(&blacklist);
  int *idx = (int *)calloc((size_t)(d->nleaves ? d->nleaves : 1), sizeof(int));
  for (int i = 0; i < d->nleaves; i++) idx[i] = i;
  veg_shuffle(fc, idx, d->nleaves);
  for (int i = 0; i < d->nleaves; i++) {
    VegPos leaf = d->leaves[idx[i]];
    int dsel = veg_get_random(fc, ndirs);
    const char *dname = jstr(jget_idx(dirs, dsel));
    int dir = VEG_DOWN;
    if (dname) {
      if (!strcmp(dname, "up")) dir = VEG_UP;
      else if (!strcmp(dname, "north")) dir = VEG_NORTH;
      else if (!strcmp(dname, "south")) dir = VEG_SOUTH;
      else if (!strcmp(dname, "west")) dir = VEG_WEST;
      else if (!strcmp(dname, "east")) dir = VEG_EAST;
    }
    int px = leaf.x + veg_dx[dir], py = leaf.y + veg_dy[dir], pz = leaf.z + veg_dz[dir];
    if (veg_set_contains(&blacklist, px, py, pz)) continue;
    if (!(veg_next_float(fc) < probability)) continue;
    int empty_ok = 1;
    for (int k = 1; k <= required_empty; k++) {
      if (!veg_deco_is_air(d, leaf.x + veg_dx[dir] * k, leaf.y + veg_dy[dir] * k, leaf.z + veg_dz[dir] * k)) {
        empty_ok = 0;
        break;
      }
    }
    if (!empty_ok) continue;
    for (int dx = -ex_xz; dx <= ex_xz; dx++) {
      for (int dy = -ex_y; dy <= ex_y; dy++) {
        for (int dz = -ex_xz; dz <= ex_xz; dz++) {
          veg_set_add(&blacklist, px + dx, py + dy, pz + dz);
        }
      }
    }
    uint16_t b = 0;
    if (veg_state_provider(fc, provider, px, py, pz, &b)) veg_deco_set_b(d, px, py, pz, b);
  }
  veg_set_free(&blacklist);
  free(idx);
}

static void veg_run_decorators(VegTree *t) {
  if (!t->decorators) return;
  VegDeco d;
  veg_deco_build(t, &d);
  for (Jv *deco = t->decorators->child; deco; deco = deco->next) {
    const char *type = jstr(jget(deco, "type"));
    if (!type) continue;
    if (!strncmp(type, "minecraft:", 10)) type += 10;
    if (!strcmp(type, "beehive")) {
      veg_deco_beehive(&d, deco);
    } else if (!strcmp(type, "cocoa")) {
      veg_deco_cocoa(&d, deco);
    } else if (!strcmp(type, "trunk_vine")) {
      veg_deco_trunk_vine(&d);
    } else if (!strcmp(type, "leave_vine")) {
      veg_deco_leave_vine(&d, deco);
    } else if (!strcmp(type, "place_on_ground")) {
      veg_deco_place_on_ground(&d, deco);
    } else if (!strcmp(type, "alter_ground")) {
      veg_deco_alter_ground(&d, deco);
    } else if (!strcmp(type, "pale_moss")) {
      veg_deco_pale_moss(&d, deco);
    } else if (!strcmp(type, "creaking_heart")) {
      veg_deco_creaking_heart(&d, deco);
    } else if (!strcmp(type, "attached_to_leaves")) {
      veg_deco_attached_to_leaves(&d, deco);
    }
  }
  veg_deco_free(&d);
}

/* ============================================================= TreeFeature */

/* getMaxFreeTreeHeight */
static int veg_max_free_height(VegTree *t, int max_tree_height, int tx, int ty, int tz) {
  for (int y = 0; y <= max_tree_height + 1; y++) {
    int r = veg_size_at_height(t->minimum_size, max_tree_height, y);
    for (int x = -r; x <= r; x++) {
      for (int z = -r; z <= r; z++) {
        if (!veg_trunk_is_free(t, tx + x, ty + y, tz + z) ||
            (!t->ignore_vines && veg_is_vine(t->fc, tx + x, ty + y, tz + z))) {
          return y - 2;
        }
      }
    }
  }
  return max_tree_height;
}

static int veg_do_place(VegTree *t, int ox, int oy, int oz) {
  FeatureCtx *fc = t->fc;
  int tree_height = veg_tree_height(fc, t->trunk_placer);
  int foliage_height = veg_fp_foliage_height(fc, t->foliage_placer, tree_height);
  int trunk_height = tree_height - foliage_height;
  int leaf_radius = veg_fp_foliage_radius(fc, t->foliage_placer, trunk_height);
  int trunk_x = ox, trunk_y = oy, trunk_z = oz;
  if (t->root_placer) trunk_y = veg_root_trunk_origin(fc, t->root_placer, oy);
  int min_y = veg_min(oy, trunk_y), max_y = veg_max(oy, trunk_y) + tree_height + 1;
  if (!(min_y >= MIN_Y + 1 && max_y <= MIN_Y + HEIGHT)) return 0;
  int min_clipped = 0;
  int has_min_clipped = veg_min_clipped_height(t->minimum_size, &min_clipped);
  int clipped = veg_max_free_height(t, tree_height, trunk_x, trunk_y, trunk_z);
  if (!(clipped >= tree_height || (has_min_clipped && clipped >= min_clipped))) return 0;
  if (t->root_placer && !veg_place_roots(t, ox, oy, oz, trunk_x, trunk_y, trunk_z)) return 0;
  VegAttachList att;
  memset(&att, 0, sizeof att);
  veg_place_trunk(t, clipped, trunk_x, trunk_y, trunk_z, &att);
  for (int i = 0; i < att.n; i++) {
    veg_create_foliage(t, t->foliage_placer, &att.a[i], foliage_height, leaf_radius, clipped);
  }
  free(att.a);
  return 1;
}

static int feature_tree(FeatureCtx *fc, Jv *config) {
  VegTree t;
  veg_tree_init(&t, fc, config);
  int ok = veg_do_place(&t, fc->origin_x, fc->origin_y, fc->origin_z);
  if (ok && (t.logs.size > 0 || t.foliage.size > 0)) {
    if (t.decorators && t.decorators->child) veg_run_decorators(&t);
  }
  veg_tree_free(&t);
  return ok;
}
/* ================================================= simple_block / selectors */

/* DoublePlantBlock subclasses in the palette (placeAt writes both halves). */
static int veg_is_double_plant(uint16_t b) {
  switch (b) {
    case B_TALL_GRASS:
    case B_LARGE_FERN:
    case B_SUNFLOWER:
    case B_LILAC:
    case B_ROSE_BUSH:
    case B_PEONY:
    case B_TALL_SEAGRASS:
    case B_SMALL_DRIPLEAF:
      return 1;
    default:
      return 0;
  }
}

/* SimpleBlockFeature */
static int feature_simple_block(FeatureCtx *fc, Jv *config) {
  int x = fc->origin_x, y = fc->origin_y, z = fc->origin_z;
  uint16_t b = 0;
  if (!veg_state_provider(fc, jget(config, "to_place"), x, y, z, &b)) return 0;
  if (!veg_survive(fc, b, x, y, z)) return 0;
  if (veg_is_double_plant(b)) {
    if (!veg_air(feat_get(fc, x, y + 1, z))) return 0;
    feat_set(fc, x, y, z, b);
    feat_set(fc, x, y + 1, z, b);
  } else if (b == B_PALE_MOSS_CARPET) {
    /* MossyCarpetBlock.placeAt: base carpet, then the topper.  The topper's
     * side pattern is decided with WorldGenRegion.getRandom() in vanilla, a
     * stream Cinder does not model, so only the base layer is placed here. */
    feat_set(fc, x, y, z, b);
  } else {
    feat_set(fc, x, y, z, b);
  }
  /* config.scheduleTick() has no worldgen-visible effect for the palette. */
  return 1;
}

/* RandomSelectorFeature */
static int feature_random_selector(FeatureCtx *fc, Jv *config) {
  Jv *features = jget(config, "features");
  for (Jv *e = features ? features->child : 0; e; e = e->next) {
    float chance = (float)jnum(jget(e, "chance"), 0.0);
    if (veg_next_float(fc) < chance) {
      Jv *placed = veg_placed_ref(fc, jget(e, "feature"));
      return feat_apply_placed(fc, placed, fc->origin_x, fc->origin_y, fc->origin_z);
    }
  }
  Jv *dflt = veg_placed_ref(fc, jget(config, "default"));
  return feat_apply_placed(fc, dflt, fc->origin_x, fc->origin_y, fc->origin_z);
}

/* SimpleRandomSelectorFeature */
static int feature_simple_random_selector(FeatureCtx *fc, Jv *config) {
  Jv *features = jget(config, "features");
  int n = jarr_len(features);
  if (n <= 0) return 0;
  int index = veg_next_bound(fc, n);
  Jv *placed = veg_placed_ref(fc, jget_idx(features, index));
  return feat_apply_placed(fc, placed, fc->origin_x, fc->origin_y, fc->origin_z);
}

/* ======================================================= BlockColumnFeature */

static void veg_column_truncate(int *heights, int n, int total, int new_height, int prioritize_tip) {
  int remove = total - new_height;
  int dir = prioritize_tip ? 1 : -1;
  int start = prioritize_tip ? 0 : n - 1;
  int end = prioritize_tip ? n : -1;
  for (int i = start; i != end && remove > 0; i += dir) {
    int take = veg_min(heights[i], remove);
    remove -= take;
    heights[i] -= take;
  }
}

static int feature_block_column(FeatureCtx *fc, Jv *config) {
  Jv *layers = jget(config, "layers");
  int nlayers = jarr_len(layers);
  int *heights = (int *)calloc((size_t)(nlayers ? nlayers : 1), sizeof(int));
  int total = 0;
  for (int i = 0; i < nlayers; i++) {
    heights[i] = veg_int_provider(jget(jget_idx(layers, i), "height"), veg_rng(fc));
    total += heights[i];
  }
  if (total == 0) {
    free(heights);
    return 0;
  }
  const char *dname = jstr(jget(config, "direction"));
  int dir = VEG_UP;
  if (dname) {
    if (!strcmp(dname, "minecraft:down")) dir = VEG_DOWN;
    else if (!strcmp(dname, "minecraft:north")) dir = VEG_NORTH;
    else if (!strcmp(dname, "minecraft:south")) dir = VEG_SOUTH;
    else if (!strcmp(dname, "minecraft:west")) dir = VEG_WEST;
    else if (!strcmp(dname, "minecraft:east")) dir = VEG_EAST;
  }
  Jv *allowed = jget(config, "allowed_placement");
  int px = fc->origin_x, py = fc->origin_y, pz = fc->origin_z;
  int nx = px + veg_dx[dir], ny = py + veg_dy[dir], nz = pz + veg_dz[dir];
  int prioritize_tip = jbool(jget(config, "prioritize_tip"), 0);
  for (int y = 0; y < total; y++) {
    if (!feat_block_predicate(fc, allowed, nx, ny, nz)) {
      veg_column_truncate(heights, nlayers, total, y, prioritize_tip);
      break;
    }
    nx += veg_dx[dir];
    ny += veg_dy[dir];
    nz += veg_dz[dir];
  }
  for (int i = 0; i < nlayers; i++) {
    int count = heights[i];
    if (count == 0) continue;
    Jv *provider = jget(jget_idx(layers, i), "provider");
    for (int y = 0; y < count; y++) {
      uint16_t b = 0;
      if (veg_state_provider(fc, provider, px, py, pz, &b)) feat_set(fc, px, py, pz, b);
      px += veg_dx[dir];
      py += veg_dy[dir];
      pz += veg_dz[dir];
    }
  }
  free(heights);
  return 1;
}

/* ================================================== VegetationPatchFeature */

static int veg_patch_ground(FeatureCtx *fc, Jv *config, Jv *replaceable, int x, int y, int z, int depth,
                            int surface_dir) {
  for (int i = 0; i < depth; i++) {
    uint16_t to_place = 0;
    if (!veg_state_provider_or_current(fc, jget(config, "ground_state"), x, y, z, &to_place)) return 0;
    uint16_t here = feat_get(fc, x, y, z);
    if (to_place != here) {
      if (!veg_holderset_contains(replaceable, here)) return i != 0;
      feat_set(fc, x, y, z, to_place);
      x += veg_dx[surface_dir];
      y += veg_dy[surface_dir];
      z += veg_dz[surface_dir];
    }
  }
  return 1;
}

static int feature_vegetation_patch(FeatureCtx *fc, Jv *config) {
  Jv *xz_radius = jget(config, "xz_radius");
  int x_radius = veg_int_provider(xz_radius, veg_rng(fc)) + 1;
  int z_radius = veg_int_provider(xz_radius, veg_rng(fc)) + 1;
  const char *sname = jstr(jget(config, "surface"));
  int inwards = (sname && !strcmp(sname, "ceiling")) ? VEG_UP : VEG_DOWN;
  int outwards = veg_opp[inwards];
  float extra_edge = (float)jnum(jget(config, "extra_edge_column_chance"), 0.0);
  float extra_bottom = (float)jnum(jget(config, "extra_bottom_block_chance"), 0.0);
  float vegetation_chance = (float)jnum(jget(config, "vegetation_chance"), 0.0);
  int vertical_range = (int)jnum(jget(config, "vertical_range"), 1);
  Jv *replaceable = jget(config, "replaceable");
  int origin_x = fc->origin_x, origin_y = fc->origin_y, origin_z = fc->origin_z;
  VegSet surface;
  veg_set_init(&surface);
  for (int dx = -x_radius; dx <= x_radius; dx++) {
    int is_x_edge = dx == -x_radius || dx == x_radius;
    for (int dz = -z_radius; dz <= z_radius; dz++) {
      int is_z_edge = dz == -z_radius || dz == z_radius;
      int is_edge = is_x_edge || is_z_edge;
      int is_corner = is_x_edge && is_z_edge;
      int is_edge_not_corner = is_edge && !is_corner;
      if (is_corner) continue;
      if (is_edge_not_corner && !(extra_edge != 0.0f && !(veg_next_float(fc) > extra_edge))) continue;
      int px = origin_x + dx, py = origin_y, pz = origin_z + dz;
      int offset = 0;
      while (veg_air(feat_get(fc, px, py, pz)) && offset < vertical_range) {
        px += veg_dx[inwards];
        py += veg_dy[inwards];
        pz += veg_dz[inwards];
        offset++;
      }
      int var = 0;
      while (!veg_air(feat_get(fc, px, py, pz)) && var < vertical_range) {
        px += veg_dx[outwards];
        py += veg_dy[outwards];
        pz += veg_dz[outwards];
        var++;
      }
      int bx = px + veg_dx[inwards], by = py + veg_dy[inwards], bz = pz + veg_dz[inwards];
      uint16_t below = feat_get(fc, bx, by, bz);
      if (veg_air(feat_get(fc, px, py, pz)) && feat_is_face_sturdy(below, outwards)) {
        int depth = veg_int_provider(jget(config, "depth"), veg_rng(fc));
        if (extra_bottom > 0.0f && veg_next_float(fc) < extra_bottom) depth++;
        if (veg_patch_ground(fc, config, replaceable, bx, by, bz, depth, inwards)) {
          veg_set_add(&surface, bx, by, bz);
        }
      }
    }
  }
  int placed_any = surface.size > 0;
  Jv *vegetation = jget(config, "vegetation_feature");
  VegPos *sp = (VegPos *)calloc((size_t)(surface.n ? surface.n : 1), sizeof(VegPos));
  int nsp = veg_set_to_array(&surface, sp);
  for (int i = 0; i < nsp; i++) {
    if (vegetation_chance > 0.0f && veg_next_float(fc) < vegetation_chance) {
      int vx = sp[i].x + veg_dx[outwards], vy = sp[i].y + veg_dy[outwards], vz = sp[i].z + veg_dz[outwards];
      feat_apply_placed(fc, vegetation, vx, vy, vz);
    }
  }
  free(sp);
  veg_set_free(&surface);
  return placed_any;
}

/* ============================================================ SeagrassFeature */

static int feature_seagrass(FeatureCtx *fc, Jv *config) {
  float probability = (float)jnum(jget(config, "probability"), 0.0);
  int ox = fc->origin_x, oz = fc->origin_z;
  int x = veg_next_bound(fc, 8) - veg_next_bound(fc, 8);
  int z = veg_next_bound(fc, 8) - veg_next_bound(fc, 8);
  int gx = ox + x, gz = oz + z;
  int y = feat_height(fc, gx, gz, FEAT_HM_OCEAN_FLOOR);
  int placed_any = 0;
  if (veg_water(feat_get(fc, gx, y, gz))) {
    int is_tall = veg_next_double(fc) < (double)probability;
    uint16_t b = is_tall ? B_TALL_SEAGRASS : B_SEAGRASS;
    if (veg_survive(fc, b, gx, y, gz)) {
      if (is_tall) {
        if (veg_water(feat_get(fc, gx, y + 1, gz))) {
          feat_set(fc, gx, y, gz, b);
          feat_set(fc, gx, y + 1, gz, b);
        }
      } else {
        feat_set(fc, gx, y, gz, b);
      }
      placed_any = 1;
    }
  }
  return placed_any;
}

/* ================================================================ KelpFeature */

static int feature_kelp(FeatureCtx *fc, Jv *config) {
  (void)config;
  int placed = 0;
  int x = fc->origin_x, z = fc->origin_z;
  int y = feat_height(fc, x, z, FEAT_HM_OCEAN_FLOOR);
  if (veg_water(feat_get(fc, x, y, z))) {
    int height = 1 + veg_next_bound(fc, 10);
    for (int h = 0; h <= height; h++) {
      if (veg_water(feat_get(fc, x, y, z)) && veg_water(feat_get(fc, x, y + 1, z)) &&
          veg_survive(fc, B_KELP_PLANT, x, y, z)) {
        if (h == height) {
          (void)veg_next_bound(fc, 4); /* KelpBlock.AGE */
          feat_set(fc, x, y, z, B_KELP);
          placed++;
        } else {
          feat_set(fc, x, y, z, B_KELP_PLANT);
        }
      } else if (h > 0) {
        int by = y - 1;
        if (veg_survive(fc, B_KELP, x, by, z) && feat_get(fc, x, by - 1, z) != B_KELP) {
          (void)veg_next_bound(fc, 4); /* KelpBlock.AGE */
          feat_set(fc, x, by, z, B_KELP);
          placed++;
        }
        break;
      }
      y++;
    }
  }
  return placed > 0;
}

/* =========================================================== SeaPickleFeature */

static int feature_sea_pickle(FeatureCtx *fc, Jv *config) {
  int placed = 0;
  int count = veg_int_provider(jget(config, "count"), veg_rng(fc));
  int ox = fc->origin_x, oz = fc->origin_z;
  for (int i = 0; i < count; i++) {
    int x = veg_next_bound(fc, 8) - veg_next_bound(fc, 8);
    int z = veg_next_bound(fc, 8) - veg_next_bound(fc, 8);
    int px = ox + x, pz = oz + z;
    int y = feat_height(fc, px, pz, FEAT_HM_OCEAN_FLOOR);
    (void)veg_next_bound(fc, 4); /* SeaPickleBlock.PICKLES */
    if (veg_water(feat_get(fc, px, y, pz)) && veg_survive(fc, B_SEA_PICKLE, px, y, pz)) {
      feat_set(fc, px, y, pz, B_SEA_PICKLE);
      placed++;
    }
  }
  return placed > 0;
}

/* ============================================================== BambooFeature */

static int feature_bamboo(FeatureCtx *fc, Jv *config) {
  int placed = 0;
  float probability = (float)jnum(jget(config, "probability"), 0.0);
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  if (veg_air(feat_get(fc, ox, oy, oz))) {
    if (veg_survive(fc, B_BAMBOO, ox, oy, oz)) {
      int height = veg_next_bound(fc, 12) + 5;
      if (veg_next_float(fc) < probability) {
        int r = veg_next_bound(fc, 4) + 1;
        for (int xx = ox - r; xx <= ox + r; xx++) {
          for (int zz = oz - r; zz <= oz + r; zz++) {
            int xd = xx - ox, zd = zz - oz;
            if (xd * xd + zd * zd <= r * r) {
              int py = feat_height(fc, xx, zz, FEAT_HM_WORLD_SURFACE) - 1;
              if (block_tag_contains("minecraft:substrate_overworld", feat_get(fc, xx, py, zz))) {
                feat_set(fc, xx, py, zz, B_PODZOL);
              }
            }
          }
        }
      }
      int cy = oy;
      int i = 0;
      while (i < height && veg_air(feat_get(fc, ox, cy, oz))) {
        feat_set(fc, ox, cy, oz, B_BAMBOO);
        cy++;
        i++;
      }
      if (cy - oy >= 3) {
        feat_set(fc, ox, cy, oz, B_BAMBOO);
        feat_set(fc, ox, cy - 1, oz, B_BAMBOO);
        feat_set(fc, ox, cy - 2, oz, B_BAMBOO);
      }
    }
    placed++;
  }
  return placed > 0;
}

/* ============================================================== VinesFeature */

static int feature_vines(FeatureCtx *fc, Jv *config) {
  (void)config;
  int x = fc->origin_x, y = fc->origin_y, z = fc->origin_z;
  if (!veg_air(feat_get(fc, x, y, z))) return 0;
  for (int dir = 0; dir < 6; dir++) {
    if (dir == VEG_DOWN) continue;
    if (veg_can_attach_to(fc, x, y, z, dir)) {
      feat_set(fc, x, y, z, B_VINE);
      return 1;
    }
  }
  return 0;
}

/* ==================================================== MultifaceGrowthFeature */

/* The face set of a placed multiface block is not representable in Cinder's
 * name-level palette, so the faces written during the current feature call are
 * tracked in a small side table (positions already in the world are treated as
 * the block's default state, i.e. no faces). */
typedef struct {
  VegPos *keys;
  uint8_t *masks;
  int n, cap;
} VegFaceMap;

static int veg_face_get(VegFaceMap *m, int x, int y, int z) {
  for (int i = 0; i < m->n; i++) {
    if (m->keys[i].x == x && m->keys[i].y == y && m->keys[i].z == z) return m->masks[i];
  }
  return 0;
}
static void veg_face_set(VegFaceMap *m, int x, int y, int z, int mask) {
  for (int i = 0; i < m->n; i++) {
    if (m->keys[i].x == x && m->keys[i].y == y && m->keys[i].z == z) {
      m->masks[i] = (uint8_t)mask;
      return;
    }
  }
  if (m->n == m->cap) {
    m->cap = m->cap ? m->cap * 2 : 32;
    m->keys = (VegPos *)realloc(m->keys, (size_t)m->cap * sizeof(VegPos));
    m->masks = (uint8_t *)realloc(m->masks, (size_t)m->cap);
  }
  m->keys[m->n].x = x;
  m->keys[m->n].y = y;
  m->keys[m->n].z = z;
  m->masks[m->n] = (uint8_t)mask;
  m->n++;
}

typedef struct {
  FeatureCtx *fc;
  uint16_t block;
  VegFaceMap faces;
} VegMultiface;

/* MultifaceBlock.isValidStateForPlacement / getStateForPlacement */
static int veg_mf_placement(VegMultiface *m, int x, int y, int z, int dir, uint16_t old_state, uint16_t *out) {
  if (old_state == m->block && (veg_face_get(&m->faces, x, y, z) & (1 << dir))) return 0;
  if (!veg_can_attach_to(m->fc, x, y, z, dir)) return 0;
  int mask = (old_state == m->block) ? veg_face_get(&m->faces, x, y, z) : 0;
  mask |= 1 << dir;
  *out = m->block;
  return mask;
}

/* MultifaceSpreader.DefaultSpreaderConfig.canSpreadInto */
static int veg_mf_can_spread(VegMultiface *m, int x, int y, int z, int dir) {
  uint16_t existing = feat_get(m->fc, x, y, z);
  int replaceable = veg_air(existing) || existing == m->block || existing == B_WATER;
  if (!replaceable) return 0;
  uint16_t out = 0;
  return veg_mf_placement(m, x, y, z, dir, existing, &out);
}

/* MultifaceSpreader.spreadFromFaceTowardDirection (postProcess is a no-op) */
static int veg_mf_spread_dir(VegMultiface *m, int x, int y, int z, int from_face, int spread_dir) {
  if (veg_axis(spread_dir) == veg_axis(from_face)) return 0;
  int mask = veg_face_get(&m->faces, x, y, z);
  if (!(mask & (1 << from_face)) || (mask & (1 << spread_dir))) return 0;
  /* SAME_POSITION, SAME_PLANE, WRAP_AROUND */
  int sx = x, sy = y, sz = z, face = spread_dir;
  if (veg_mf_can_spread(m, sx, sy, sz, face)) goto place;
  sx = x + veg_dx[spread_dir];
  sy = y + veg_dy[spread_dir];
  sz = z + veg_dz[spread_dir];
  face = from_face;
  if (veg_mf_can_spread(m, sx, sy, sz, face)) goto place;
  sx = sx + veg_dx[from_face];
  sy = sy + veg_dy[from_face];
  sz = sz + veg_dz[from_face];
  face = veg_opp[spread_dir];
  if (veg_mf_can_spread(m, sx, sy, sz, face)) goto place;
  return 0;
place: {
  uint16_t old = feat_get(m->fc, sx, sy, sz);
  uint16_t mask2 = 0;
  if (!veg_mf_placement(m, sx, sy, sz, face, old, &mask2)) return 0;
  feat_set(m->fc, sx, sy, sz, m->block);
  veg_face_set(&m->faces, sx, sy, sz, mask2);
  return 1;
}
}

/* MultifaceSpreader.spreadFromFaceTowardRandomDirection */
static void veg_mf_spread_random(VegMultiface *m, int x, int y, int z, int starting_face) {
  int order[6] = {VEG_DOWN, VEG_UP, VEG_NORTH, VEG_SOUTH, VEG_WEST, VEG_EAST};
  veg_shuffle(m->fc, order, 6);
  for (int i = 0; i < 6; i++) {
    if (veg_mf_spread_dir(m, x, y, z, starting_face, order[i])) return;
  }
}

/* MultifaceGrowthFeature.placeGrowthIfPossible */
static int veg_mf_place_if_possible(VegMultiface *m, Jv *config, int x, int y, int z, uint16_t old_state,
                                    const int *placement_dirs, int ndirs) {
  FeatureCtx *fc = m->fc;
  Jv *can_be_placed_on = jget(config, "can_be_placed_on");
  float chance = (float)jnum(jget(config, "chance_of_spreading"), 0.5);
  for (int i = 0; i < ndirs; i++) {
    int dir = placement_dirs[i];
    uint16_t neighbour = feat_get(fc, x + veg_dx[dir], y + veg_dy[dir], z + veg_dz[dir]);
    if (!veg_holderset_contains(can_be_placed_on, neighbour)) continue;
    uint16_t mask = 0;
    if (!veg_mf_placement(m, x, y, z, dir, old_state, &mask)) return 0;
    feat_set(fc, x, y, z, m->block);
    veg_face_set(&m->faces, x, y, z, mask);
    if (veg_next_float(fc) < chance) veg_mf_spread_random(m, x, y, z, dir);
    return 1;
  }
  return 0;
}

static int feature_multiface_growth(FeatureCtx *fc, Jv *config) {
  const char *bname = jstr(jget(config, "block"));
  int id = block_id_for_name(bname);
  if (id < 0) return 0;
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  uint16_t here = feat_get(fc, ox, oy, oz);
  if (!(veg_air(here) || here == B_WATER)) return 0;
  int search_range = (int)jnum(jget(config, "search_range"), 10);
  int can_floor = jbool(jget(config, "can_place_on_floor"), 0);
  int can_ceiling = jbool(jget(config, "can_place_on_ceiling"), 0);
  int can_wall = jbool(jget(config, "can_place_on_wall"), 0);
  int valid[6];
  int nvalid = 0;
  if (can_ceiling) valid[nvalid++] = VEG_UP;
  if (can_floor) valid[nvalid++] = VEG_DOWN;
  if (can_wall) {
    for (int i = 0; i < 4; i++) valid[nvalid++] = veg_horiz[i];
  }
  if (nvalid == 0) return 0;
  VegMultiface m;
  memset(&m, 0, sizeof m);
  m.fc = fc;
  m.block = (uint16_t)id;
  int order[6];
  for (int i = 0; i < nvalid; i++) order[i] = valid[i];
  veg_shuffle(fc, order, nvalid);
  if (veg_mf_place_if_possible(&m, config, ox, oy, oz, here, order, nvalid)) {
    free(m.faces.keys);
    free(m.faces.masks);
    return 1;
  }
  for (int i = 0; i < nvalid; i++) {
    int search_dir = order[i];
    int except[6], nex = 0;
    for (int k = 0; k < nvalid; k++) {
      if (valid[k] != veg_opp[search_dir]) except[nex++] = valid[k];
    }
    veg_shuffle(fc, except, nex);
    for (int step = 0; step < search_range; step++) {
      int px = ox + veg_dx[search_dir], py = oy + veg_dy[search_dir], pz = oz + veg_dz[search_dir];
      uint16_t state = feat_get(fc, px, py, pz);
      if (!(veg_air(state) || state == B_WATER) && state != m.block) break;
      if (veg_mf_place_if_possible(&m, config, px, py, pz, state, except, nex)) {
        free(m.faces.keys);
        free(m.faces.masks);
        return 1;
      }
    }
  }
  free(m.faces.keys);
  free(m.faces.masks);
  return 0;
}

/* =========================================================== RootSystemFeature */

static int veg_allowed_tree_space(uint16_t state, int blocks_above_origin, int allowed_water) {
  if (veg_air(state)) return 1;
  int above_ground = blocks_above_origin + 1;
  return above_ground <= allowed_water && veg_water(state);
}

static int veg_root_space_for_tree(FeatureCtx *fc, Jv *config, int x, int y, int z) {
  int required = (int)jnum(jget(config, "required_vertical_space_for_tree"), 1);
  int allowed_water = (int)jnum(jget(config, "allowed_vertical_water_for_tree"), 1);
  for (int i = 1; i <= required; i++) {
    if (!veg_allowed_tree_space(feat_get(fc, x, y + i, z), i, allowed_water)) return 0;
  }
  int level_test = (int)jnum(jget(config, "level_test_distance"), 0);
  if (level_test > 0) {
    int max_dev = (int)jnum(jget(config, "max_level_deviation"), 0);
    for (int i = 0; i < 4; i++) {
      int dir = veg_horiz[(i * 3) % 4]; /* Direction.from2DDataValue order: SOUTH, WEST, NORTH, EAST */
      dir = (i == 0) ? VEG_SOUTH : (i == 1) ? VEG_WEST : (i == 2) ? VEG_NORTH : VEG_EAST;
      int cx = x + veg_dx[dir] * level_test;
      int cz = z + veg_dz[dir] * level_test;
      if (veg_air(feat_get(fc, cx, y - max_dev, cz)) || !veg_air(feat_get(fc, cx, y + max_dev, cz))) return 0;
    }
  }
  return 1;
}

static void veg_root_place_dirt(FeatureCtx *fc, Jv *config, int ox, int oy, int oz, int target_height) {
  int root_radius = (int)jnum(jget(config, "root_radius"), 1);
  int attempts = (int)jnum(jget(config, "root_placement_attempts"), 1);
  Jv *replaceable = jget(config, "root_replaceable");
  Jv *provider = jget(config, "root_state_provider");
  for (int y = oy; y < target_height; y++) {
    for (int i = 0; i < attempts; i++) {
      int x = ox + veg_next_bound(fc, root_radius) - veg_next_bound(fc, root_radius);
      int z = oz + veg_next_bound(fc, root_radius) - veg_next_bound(fc, root_radius);
      uint16_t here = feat_get(fc, x, y, z);
      if (veg_holderset_contains(replaceable, here)) {
        uint16_t b = 0;
        if (veg_state_provider(fc, provider, x, y, z, &b)) feat_set(fc, x, y, z, b);
      }
    }
  }
}

static void veg_root_place_hanging(FeatureCtx *fc, Jv *config, int ox, int oy, int oz) {
  int radius = (int)jnum(jget(config, "hanging_root_radius"), 1);
  int span = (int)jnum(jget(config, "hanging_roots_vertical_span"), 1);
  int attempts = (int)jnum(jget(config, "hanging_root_placement_attempts"), 1);
  Jv *provider = jget(config, "hanging_root_state_provider");
  for (int i = 0; i < attempts; i++) {
    int x = ox + veg_next_bound(fc, radius) - veg_next_bound(fc, radius);
    int y = oy + veg_next_bound(fc, span) - veg_next_bound(fc, span);
    int z = oz + veg_next_bound(fc, radius) - veg_next_bound(fc, radius);
    if (!veg_air(feat_get(fc, x, y, z))) continue;
    uint16_t b = 0;
    if (!veg_state_provider(fc, provider, x, y, z, &b)) continue;
    if (veg_survive(fc, b, x, y, z) && feat_is_face_sturdy(feat_get(fc, x, y + 1, z), VEG_DOWN)) {
      feat_set(fc, x, y, z, b);
    }
  }
}

static int feature_root_system(FeatureCtx *fc, Jv *config) {
  int ox = fc->origin_x, oy = fc->origin_y, oz = fc->origin_z;
  if (!veg_air(feat_get(fc, ox, oy, oz))) return 0;
  int max_height = (int)jnum(jget(config, "root_column_max_height"), 1);
  Jv *allowed_tree_position = jget(config, "allowed_tree_position");
  Jv *tree_feature = jget(config, "feature");
  int placed = 0;
  for (int y = 0; y < max_height; y++) {
    int wy = oy + y + 1;
    if (feat_height(fc, ox, oz, FEAT_HM_WORLD_SURFACE) < wy) break;
    if (feat_block_predicate(fc, allowed_tree_position, ox, wy, oz) &&
        veg_root_space_for_tree(fc, config, ox, wy, oz)) {
      int by = wy - 1;
      if (feat_get(fc, ox, by, oz) == B_LAVA || !feat_is_solid_ground(fc, ox, by, oz)) break;
      if (feat_apply_placed(fc, tree_feature, ox, wy, oz)) {
        veg_root_place_dirt(fc, config, ox, oy, oz, oy + y);
        placed = 1;
        break;
      }
    }
  }
  if (placed) veg_root_place_hanging(fc, config, ox, oy, oz);
  return 1;
}

/* ======================================================== SnowAndFreezeFeature */

static int veg_biome_frozen(int biome) { return biome == BIO_FROZEN_OCEAN || biome == BIO_DEEP_FROZEN_OCEAN; }

/* Biome.getTemperature(pos, seaLevel), without the PerlinSimplexNoise height
 * adjustment (Cinder has no PerlinSimplexNoise); the base temperature plus the
 * FROZEN modifier decide `warmEnoughToRain` for every snow-relevant y here. */
static float veg_biome_temperature(FeatureCtx *fc, int biome) {
  float base = (float)biome_temperature(fc->g, biome);
  if (veg_biome_frozen(biome)) {
    /* TemperatureModifier.FROZEN returns 0.2 above the ice-patch noise; the
     * noise is not modelled, so keep the base temperature. */
    return base;
  }
  return base;
}

static int veg_warm_enough_to_rain(FeatureCtx *fc, int biome) {
  return veg_biome_temperature(fc, biome) >= 0.15f;
}

static int feature_freeze_top_layer(FeatureCtx *fc, Jv *config) {
  (void)config;
  int ox = fc->origin_x, oz = fc->origin_z;
  for (int dx = 0; dx < 16; dx++) {
    for (int dz = 0; dz < 16; dz++) {
      int x = ox + dx, z = oz + dz;
      int y = feat_height(fc, x, z, FEAT_HM_MOTION_BLOCKING);
      int biome = feat_biome(fc, x, y, z);
      int warm = veg_warm_enough_to_rain(fc, biome);
      /* Biome.shouldFreeze(level, belowPos, false): block light is 0 during
       * worldgen, so only the water check matters. */
      if (!warm && feat_get(fc, x, y - 1, z) == B_WATER) feat_set(fc, x, y - 1, z, B_ICE);
      /* Biome.shouldSnow(level, topPos) */
      if (!warm) {
        uint16_t top = feat_get(fc, x, y, z);
        if ((veg_air(top) || top == B_SNOW) && feat_would_survive(fc, B_SNOW, x, y, z)) {
          feat_set(fc, x, y, z, B_SNOW);
        }
      }
    }
  }
  return 1;
}

/* ======================================================= registration ====== */

typedef struct {
  const char *type;
  FeatureFn fn;
} VegFeatureEntry;

FeatureFn feature_vegetation_lookup(const char *type) {
  static const VegFeatureEntry entries[] = {
      {"tree", feature_tree},
      {"simple_block", feature_simple_block},
      {"random_selector", feature_random_selector},
      {"simple_random_selector", feature_simple_random_selector},
      {"block_column", feature_block_column},
      {"vegetation_patch", feature_vegetation_patch},
      {"seagrass", feature_seagrass},
      {"kelp", feature_kelp},
      {"sea_pickle", feature_sea_pickle},
      {"bamboo", feature_bamboo},
      {"vines", feature_vines},
      {"multiface_growth", feature_multiface_growth},
      {"root_system", feature_root_system},
      {"freeze_top_layer", feature_freeze_top_layer},
  };
  if (!type) return 0;
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  for (size_t i = 0; i < sizeof entries / sizeof entries[0]; i++) {
    if (!strcmp(type, entries[i].type)) return entries[i].fn;
  }
  return 0;
}
