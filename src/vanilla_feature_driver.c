// Chunk driver with the feature halo.
//
// Vanilla's FEATURES step writes into a 1-chunk write radius, so a chunk is only
// final after its eight neighbours have been decorated too (a tree at a border
// drops leaves into the chunk next door, and that write must happen exactly once
// and before that chunk is emitted). Cinder therefore generates in passes:
//
//   pass 0: terrain + surface + carvers for every chunk in the batch
//   pass 1: features for every chunk, in a deterministic order, each writing
//           through the 3x3 window (neighbours already have terrain)
//   pass 2: lighting (Cinder's own, not part of block parity)
//
// Neighbours outside the batch are generated on demand as "terrain-only" halo
// chunks; their own features are not run, which matches the reference world's
// boundary behaviour only if the reference is compared in the interior - the
// parity script compares interior chunks for exactly this reason.
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H

#ifndef CINDER_INC_VANILLA_FEATURE_COMMON_H
#define CINDER_INC_VANILLA_FEATURE_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_common.h"
#endif
#include CINDER_INC_VANILLA_FEATURE_COMMON_H

/* FeatureChunk storage: one terrain buffer + heightmaps per generated chunk. */
FeatureChunk *feat_chunk_new(int cx, int cz) {
  FeatureChunk *slot = (FeatureChunk *)calloc(1, sizeof(FeatureChunk));
  slot->b = (uint16_t *)calloc(16 * 384 * 16, sizeof(uint16_t));
  slot->hm = (int32_t *)calloc((size_t)FEAT_HM_COUNT * 256, sizeof(int32_t));
  slot->cx = cx;
  slot->cz = cz;
  slot->live = 1;
  for (int i = 0; i < 256; i++) {
    for (int hm = 0; hm < FEAT_HM_COUNT; hm++) slot->hm[(size_t)hm * 256 + i] = MIN_Y;
  }
  return slot;
}

void feat_chunk_free(FeatureChunk *slot) {
  if (!slot) return;
  free(slot->b);
  free(slot->hm);
  free(slot);
}

/* The four heightmaps vanilla's FEATURES step primes on the chunk it decorates
 * (Heightmap.primeHeightmaps(chunk, EnumSet.of(MOTION_BLOCKING,
 * MOTION_BLOCKING_NO_LEAVES, OCEAN_FLOOR, WORLD_SURFACE))).  The two _WG maps
 * are maintained by the terrain stages and are not touched by the feature step. */
static void feat_hm_prime_final(FeatureChunk *slot) {
  static const int types[4] = {FEAT_HM_MOTION_BLOCKING, FEAT_HM_MOTION_BLOCKING_NO_LEAVES,
                               FEAT_HM_OCEAN_FLOOR, FEAT_HM_WORLD_SURFACE};
  for (int i = 0; i < 4; i++) {
    int hm = types[i];
    if (!slot->hm_primed[hm]) feat_hm_prime(slot, hm);
  }
}

/* Terrain stages only (biomes -> noise -> surface -> carvers), no features. */
void feat_generate_terrain(Gen *g, int cx, int cz, FeatureChunk *slot) {
  Chunk c;
  ChunkEval ce;
  memset(&c, 0, sizeof c);
  memset(&ce, 0, sizeof ce);
  c.cx = cx;
  c.cz = cz;
  c.b = slot->b;
  chunk_eval_begin(g, &ce, cx, cz);
  gen_fill_noise(g, &c, &ce);
  gen_surface(g, &c, &ce);
  gen_carvers(g, &c, &ce);
  /* the biome array lives on the Chunk during the terrain stages */
  memcpy(slot->biome, c.biome, BIOME_CELLS);
  feat_hm_prime_final(slot);
  chunk_eval_free(&ce);
}

/* ------------------------------------------------------------ parallel -- */

typedef struct {
  Gen *g;
  FeatureChunk **slots;
  int n, start, end;
} FeatTerrainJob;

typedef struct {
  Gen *g;
  FeatureChunk **slots;
  int start, end;
  int skip_surface, skip_carvers;
} FeatStageJob;

static void feat_generate_terrain_stages(Gen *g, FeatureChunk *slot, int skip_surface,
                                         int skip_carvers);

static void *feat_stage_worker(void *p) {
  FeatStageJob *j = (FeatStageJob *)p;
  for (int i = j->start; i < j->end; i++) {
    feat_generate_terrain_stages(j->g, j->slots[i], j->skip_surface, j->skip_carvers);
  }
  return 0;
}

static void *feat_terrain_worker(void *p) {
  FeatTerrainJob *j = (FeatTerrainJob *)p;
  for (int i = j->start; i < j->end; i++) {
    FeatureChunk *slot = j->slots[i];
    feat_generate_terrain(j->g, slot->cx, slot->cz, slot);
  }
  return 0;
}

/* Terrain for one slot with optional stage selection (the stage-by-stage
 * checksum path: features off, surface/carvers optionally off). */
static void feat_generate_terrain_stages(Gen *g, FeatureChunk *slot, int skip_surface,
                                         int skip_carvers) {
  Chunk c;
  ChunkEval ce;
  memset(&c, 0, sizeof c);
  memset(&ce, 0, sizeof ce);
  c.cx = slot->cx;
  c.cz = slot->cz;
  c.b = slot->b;
  chunk_eval_begin(g, &ce, slot->cx, slot->cz);
  gen_fill_noise(g, &c, &ce);
  if (!skip_surface) gen_surface(g, &c, &ce);
  if (!skip_carvers) gen_carvers(g, &c, &ce);
  memcpy(slot->biome, c.biome, BIOME_CELLS);
  feat_hm_prime_final(slot);
  chunk_eval_free(&ce);
}

void feat_terrain_parallel_all(Gen *g, FeatureChunk **slots, int n, int threads,
                               int skip_surface, int skip_carvers) {
  if (n <= 0) return;
  int nt = threads;
  if (nt > n) nt = n;
  if (nt < 1) nt = 1;
  if (nt == 1) {
    for (int i = 0; i < n; i++) {
      feat_generate_terrain_stages(g, slots[i], skip_surface, skip_carvers);
    }
    return;
  }
  pthread_t th[32];
  FeatStageJob jobs[32];
  int per = (n + nt - 1) / nt;
  int started = 0;
  for (int t = 0; t < nt; t++) {
    jobs[t].g = g;
    jobs[t].slots = slots;
    jobs[t].start = t * per;
    jobs[t].end = jobs[t].start + per > n ? n : jobs[t].start + per;
    jobs[t].skip_surface = skip_surface;
    jobs[t].skip_carvers = skip_carvers;
    if (jobs[t].start >= jobs[t].end) break;
    pthread_create(&th[t], 0, feat_stage_worker, &jobs[t]);
    started++;
  }
  for (int t = 0; t < started; t++) pthread_join(th[t], 0);
}

/* Terrain for every slot, spread over `threads` workers. */
static void feat_terrain_parallel(Gen *g, FeatureChunk **slots, int n, int threads) {
  if (n <= 0) return;
  int nt = threads;
  if (nt > n) nt = n;
  if (nt < 1) nt = 1;
  pthread_t th[32];
  FeatTerrainJob jobs[32];
  int per = (n + nt - 1) / nt;
  for (int t = 0; t < nt; t++) {
    jobs[t].g = g;
    jobs[t].slots = slots;
    jobs[t].n = n;
    jobs[t].start = t * per;
    jobs[t].end = jobs[t].start + per > n ? n : jobs[t].start + per;
    pthread_create(&th[t], 0, feat_terrain_worker, &jobs[t]);
  }
  for (int t = 0; t < nt; t++) pthread_join(th[t], 0);
}

/* How many rings around the batch get their features run (see feature_batch_run).
 * 1 is what the write radius requires (a chunk's decoration writes into its eight
 * neighbours, so those neighbours' features must run too). Vanilla decorates two
 * rings, because its forceload rect is inflated by 2 chunks at FULL status;
 * measured over the 8x8 canonical area that second ring is worth +0.004 % block
 * agreement for about 25 % more feature-stage time, so it is off by default.
 * Define CINDER_DECORATE_RINGS=2 to match vanilla's band exactly. */
#ifndef CINDER_DECORATE_RINGS
#define CINDER_DECORATE_RINGS 1
#endif
#define FEAT_DECORATE_RINGS CINDER_DECORATE_RINGS

/* --------------------------------------------------------------- pipeline -- */

typedef struct {
  Gen *g;
  int grid, ox, oz;
  int threads;
  FeatureChunk **slots; /* grid*grid, index = cz*grid + cx (local) */
  uint32_t *sums;
} FeatureBatch;

/* Defined below feature_batch_run; declared here because that function uses it. */
static void feat_decorate_parallel(FeatureBatch *batch, FeatureChunk **halo, int halo_n,
                                   const int *list, int n);

static FeatureChunk *batch_slot(FeatureBatch *batch, int cx, int cz) {
  int lx = cx - batch->ox, lz = cz - batch->oz;
  if (lx < 0 || lz < 0 || lx >= batch->grid || lz >= batch->grid) return 0;
  return batch->slots[lz * batch->grid + lx];
}

/* Generates the batch and every chunk features can reach (the 1-chunk ring). */
void feature_batch_run(FeatureBatch *batch) {
  Gen *g = batch->g;
  /* Vanilla decorates every chunk the forceload rect covers plus its neighbours
   * at FULL status, so a batch chunk's blocks depend on the features of chunks
   * up to two away (a distance-2 chunk's feature writes into a distance-1 chunk,
   * which that chunk's own features then see, and they write into the batch).
   * Decorate that far out, and keep one more ring of terrain so the outermost
   * decorated ring has real terrain to read in its 3x3 windows. */
  const int RING = FEAT_DECORATE_RINGS + 1;
  int halo = batch->grid + 2 * RING;
  FeatureChunk **halo_slots = (FeatureChunk **)calloc((size_t)halo * halo, sizeof(FeatureChunk *));
  for (int lz = 0; lz < halo; lz++) {
    for (int lx = 0; lx < halo; lx++) {
      int cx = batch->ox - RING + lx, cz = batch->oz - RING + lz;
      int in_batch = lx >= RING && lz >= RING && lx < RING + batch->grid &&
                     lz < RING + batch->grid;
      FeatureChunk *slot = in_batch ? batch_slot(batch, cx, cz) : 0;
      if (!slot) slot = feat_chunk_new(cx, cz);
      halo_slots[lz * halo + lx] = slot;
    }
  }
  {
    int n = halo * halo;
    FeatureChunk **all = (FeatureChunk **)calloc((size_t)n, sizeof(FeatureChunk *));
    for (int i = 0; i < n; i++) all[i] = halo_slots[i];
    feat_terrain_parallel(g, all, n, batch->threads);
    free(all);
  }

  /* FEATURES step, in parallel.
   *
   * A chunk's decoration only reads and writes its own 3x3 window (vanilla's
   * blockStateWriteRadius(1)), so two chunks can be decorated concurrently
   * exactly when their windows are disjoint - i.e. when they are at least 3
   * apart in both axes. Splitting the batch by (lx % 3, lz % 3) gives nine
   * passes with that property, and every chunk runs in exactly one of them.
   * That is the same decoupling vanilla's chunk scheduler relies on; the pass
   * order is fixed so the output stays deterministic. */
  feat_sorter_init(g);
  {
    int pass;
    for (pass = 0; pass < 9; pass++) {
      int count = 0;
      int *list = (int *)malloc((size_t)halo * halo * sizeof(int));
      int n = 0;
      /* every decorated chunk (batch + inner ring), grouped by the world chunk
       * coordinates modulo 3 so a pass is pairwise >= 3 apart */
      for (int lz = 0; lz < halo; lz++) {
        for (int lx = 0; lx < halo; lx++) {
          int cx = batch->ox - RING + lx, cz = batch->oz - RING + lz;
          int decorated = lx >= RING - FEAT_DECORATE_RINGS && lz >= RING - FEAT_DECORATE_RINGS &&
                          lx <= RING + batch->grid - 1 + FEAT_DECORATE_RINGS &&
                          lz <= RING + batch->grid - 1 + FEAT_DECORATE_RINGS;
          if (!decorated) continue;
          int gx = ((cx % 3) + 3) % 3, gz = ((cz % 3) + 3) % 3;
          if (gz * 3 + gx != pass) continue;
          list[n++] = lz * halo + lx;
          count++;
        }
      }
      if (!count) {
        free(list);
        continue;
      }
      feat_decorate_parallel(batch, halo_slots, halo, list, n);
      free(list);
    }
  }

  /* lighting (Cinder's own) + checksum */
  for (int lz = 0; lz < batch->grid; lz++) {
    for (int lx = 0; lx < batch->grid; lx++) {
      FeatureChunk *slot = batch->slots[lz * batch->grid + lx];
      Chunk c;
      memset(&c, 0, sizeof c);
      c.cx = slot->cx;
      c.cz = slot->cz;
      c.b = slot->b;
      gen_light(g, &c);
      uint32_t s = (uint32_t)(c.cx * 131 + c.cz);
      for (int i = 0; i < 16 * 384 * 16; i++) s += slot->b[i];
      for (int i = 0; i < BIOME_CELLS; i++) s += slot->biome[i] * 17u;
      batch->sums[lz * batch->grid + lx] = s;
    }
  }

  /* free the halo chunks (the batch slots are freed by the caller after dump) */
  for (int lz = 0; lz < halo; lz++) {
    for (int lx = 0; lx < halo; lx++) {
      int cx = batch->ox - RING + lx, cz = batch->oz - RING + lz;
      if (cx >= batch->ox && cz >= batch->oz && cx < batch->ox + batch->grid &&
          cz < batch->oz + batch->grid) {
        continue;
      }
      feat_chunk_free(halo_slots[lz * halo + lx]);
    }
  }
  free(halo_slots);
}

/* ------------------------------------------------------- decorate pass -- */

typedef struct {
  FeatureBatch *batch;
  FeatureChunk **halo;
  int halo_n;
  const int *list;
  int start, end;
} FeatDecorateJob;

static void *feat_decorate_worker(void *p) {
  FeatDecorateJob *j = (FeatDecorateJob *)p;
  Gen *g = j->batch->g;
  for (int i = j->start; i < j->end; i++) {
    int idx = j->list[i]; /* halo index of the centre chunk */
    int hx = idx % j->halo_n, hz = idx / j->halo_n;
    FeatureChunk *centre = j->halo[idx];
    FeatureChunk *win[3][3];
    for (int dz = 0; dz < 3; dz++) {
      for (int dx = 0; dx < 3; dx++) {
        win[dz][dx] = j->halo[(hz + dz - 1) * j->halo_n + (hx + dx - 1)];
      }
    }
    ChunkEval ce;
    memset(&ce, 0, sizeof ce);
    chunk_eval_begin(g, &ce, centre->cx, centre->cz);
    gen_features_window(g, &ce, win, centre);
    chunk_eval_free(&ce);
  }
  return 0;
}

/* Decorate every chunk in `list` (all pairwise >= 3 apart) over the pool. */
static void feat_decorate_parallel(FeatureBatch *batch, FeatureChunk **halo, int halo_n,
                                   const int *list, int n) {
  int nt = batch->threads;
  if (nt > n) nt = n;
  if (nt < 1) nt = 1;
  if (nt == 1) {
    FeatDecorateJob one;
    one.batch = batch;
    one.halo = halo;
    one.halo_n = halo_n;
    one.list = list;
    one.start = 0;
    one.end = n;
    feat_decorate_worker(&one);
    return;
  }
  pthread_t th[32];
  FeatDecorateJob jobs[32];
  int per = (n + nt - 1) / nt;
  int started = 0;
  for (int t = 0; t < nt; t++) {
    jobs[t].batch = batch;
    jobs[t].halo = halo;
    jobs[t].halo_n = halo_n;
    jobs[t].list = list;
    jobs[t].start = t * per;
    jobs[t].end = jobs[t].start + per > n ? n : jobs[t].start + per;
    if (jobs[t].start >= jobs[t].end) break;
    pthread_create(&th[t], 0, feat_decorate_worker, &jobs[t]);
    started++;
  }
  for (int t = 0; t < started; t++) pthread_join(th[t], 0);
}

/* ------------------------------------------------------------- entry -- */

/* The whole pipeline for a grid*grid batch: terrain (parallel), features
 * (sequential, halo-aware) and lighting (parallel). */
void feat_run_batch(Gen *g, int grid, int ox, int oz, FeatureChunk **slots, int threads) {
  FeatureBatch batch;
  batch.g = g;
  batch.grid = grid;
  batch.ox = ox;
  batch.oz = oz;
  batch.slots = slots;
  batch.threads = threads;
  batch.sums = (uint32_t *)calloc((size_t)grid * grid, sizeof(uint32_t));
  feature_batch_run(&batch);
  free(batch.sums);
}
