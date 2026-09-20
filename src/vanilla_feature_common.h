#pragma once

// Feature stage: vanilla placement pipeline + feature implementations.
//
// Mirrors net.minecraft.world.level.chunk.ChunkGenerator.applyBiomeDecoration,
// FeatureSorter, PlacedFeature and the placement modifiers, with a 3x3 chunk
// window standing in for vanilla's WorldGenRegion (ChunkPyramid: the FEATURES
// step has blockStateWriteRadius(1) and requires CARVERS at radius 1, so
// features may write into the eight neighbouring chunks - and do, e.g. trees at
// a chunk border).
//
// Configured-feature implementations live in the sibling family files
// (vanilla_feature_terrain.c, vanilla_feature_vegetation.c) and register
// through feature_terrain_lookup / feature_vegetation_lookup.
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H

/* --------------------------------------------------------- world window -- */

typedef struct {
  int live;                    /* 1 while this slot holds generated terrain */
  int cx, cz;
  uint16_t *b;                 /* 16*384*16, idx(x,y,z) */
  int32_t *hm;                 /* [FEAT_HM_COUNT][256], first-available Y */
  unsigned char hm_primed[6];  /* per-heightmap primed flag */
  uint8_t biome[BIOME_CELLS];
} FeatureChunk;

#define FEAT_HM_OCEAN_FLOOR_WG 0
#define FEAT_HM_WORLD_SURFACE_WG 1
#define FEAT_HM_MOTION_BLOCKING 2
#define FEAT_HM_MOTION_BLOCKING_NO_LEAVES 3
#define FEAT_HM_WORLD_SURFACE 4
#define FEAT_HM_OCEAN_FLOOR 5
#define FEAT_HM_COUNT 6

typedef struct {
  Gen *g;
  ChunkEval *ce;
  FeatureChunk *win[3][3];     /* [dz+1][dx+1]; [1][1] is the centre chunk */
  LegacyRng rng;               /* the feature RNG (WorldgenRandom after setFeatureSeed) */
  int origin_x, origin_y, origin_z; /* the position ConfiguredFeature.place receives */
  int chunk_cx, chunk_cz;      /* the chunk being decorated */
  Jv *placed;                  /* the PlacedFeature JSON currently being applied */
  const char *placed_name;     /* its embed key ("pf:<name>"), for BiomeFilter */
  int global_index, step;      /* FeatureSorter identity + decoration step */
  int nested_placed_any;       /* set by the nested emit while feat_apply_placed runs */
} FeatureCtx;

/* FeatureChunk storage + the halo batch pipeline (vanilla_feature_driver.c). */
FeatureChunk *feat_chunk_new(int cx, int cz);
void feat_chunk_free(FeatureChunk *slot);
void feat_generate_terrain(Gen *g, int cx, int cz, FeatureChunk *slot);
/* terrain (parallel) -> features (halo-aware) -> lighting for a grid*grid batch */
void feat_run_batch(Gen *g, int grid, int ox, int oz, FeatureChunk **slots, int threads);
/* terrain only, with optional stage selection (the stage-by-stage checksum path) */
void feat_terrain_parallel_all(Gen *g, FeatureChunk **slots, int n, int threads,
                               int skip_surface, int skip_carvers);

/* vanilla WorldGenLevel accessors over the window */
uint16_t feat_get(FeatureCtx *fc, int x, int y, int z);
int feat_set(FeatureCtx *fc, int x, int y, int z, uint16_t block);
int feat_in_window(FeatureCtx *fc, int x, int y, int z);
int feat_height(FeatureCtx *fc, int x, int z, int hm);
int feat_biome(FeatureCtx *fc, int x, int y, int z);
LegacyRng *feat_rng(FeatureCtx *fc);
int feat_min_y(FeatureCtx *fc);

/* the placement chain: walks the modifier list and calls emit for every
 * position vanilla would hand to the configured feature (non-static so the
 * family files can drive nested placed features) */
typedef int (*PlaceEmit)(FeatureCtx *fc, int x, int y, int z);
void place_chain(FeatureCtx *fc, Jv *mod, int x, int y, int z, PlaceEmit emit);
int feat_gen_depth(FeatureCtx *fc);
/* vanilla BlockBehaviour.BlockStateBase semantics for our palette */
int feat_can_replace(uint16_t b);
int feat_is_fluid(uint16_t b);
int feat_is_solid_ground(FeatureCtx *fc, int x, int y, int z);
int feat_is_collision_full(uint16_t b);
int feat_is_face_sturdy(uint16_t b, int dir); /* 0=down,1=up,2=north,3=south,4=west,5=east */
int feat_occludes_face(uint16_t b, int dir);

/* vanilla block name -> Cinder id, or -1 (names not in the palette) */
int block_id_for_name(const char *name);
/* vanilla block tag membership (generated table in vanilla_block_tags.h) */
int block_tag_contains(const char *tag, uint16_t block);
/* BlockStateBase.canSurvive for the blocks features place (support rules) */
int feat_would_survive(FeatureCtx *fc, uint16_t block, int x, int y, int z);

/* shared helpers implemented in vanilla_placements.c */
int feat_block_predicate(FeatureCtx *fc, Jv *pred, int x, int y, int z);
int feat_state_provider(FeatureCtx *fc, Jv *sp, int x, int y, int z, uint16_t *out);
int feat_int_provider(Jv *p, LegacyRng *r);
double feat_float_provider(Jv *p, LegacyRng *r);
int feat_apply_placed(FeatureCtx *fc, Jv *placed_json, int x, int y, int z);
/* BiomeFilter: does this biome list the placed feature being applied? */
int feat_biome_has_feature(FeatureCtx *fc, int biome);
/* the emit callback used by feat_apply_placed (defined in vanilla_features.c) */
int feat_nested_emit(FeatureCtx *fc, int x, int y, int z);

/* -------------------------------------------------- feature registration -- */

typedef int (*FeatureFn)(FeatureCtx *fc, Jv *config);

FeatureFn feature_lookup(const char *type);
FeatureFn feature_terrain_lookup(const char *type);
FeatureFn feature_vegetation_lookup(const char *type);

/* ------------------------------------------------------------------ driver */

/* Runs vanilla's applyBiomeDecoration for one chunk. `win` is the 3x3 window
 * (centre = the chunk being decorated), `centre` its terrain buffer. */
void gen_features_window(Gen *g, ChunkEval *ce, FeatureChunk *win[3][3], FeatureChunk *centre);
/* vanilla's decoration seed for a chunk: WorldgenRandom.setDecorationSeed */
uint64_t feature_decoration_seed(Gen *g, int origin_x, int origin_z);
