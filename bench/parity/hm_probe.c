// Are the feature-stage heightmaps the same as recomputing them from scratch?
//
// The tree decorators ask for MOTION_BLOCKING_NO_LEAVES, which the feature stage
// maintains incrementally as blocks are written. If an incremental update is
// wrong (a leaf counted, a column not walked back), the decorators reject
// placements vanilla accepts. This compares the maintained map with a fresh
// computation over the final block array, column by column.
#include "vanilla_gen.c"

static int32_t from_scratch(FeatureChunk *slot, int hm, int lx, int lz) {
  for (int y = MIN_Y + HEIGHT - 1; y >= MIN_Y; y--) {
    if (feat_hm_predicate(slot->b[idx(lx, y, lz)], hm)) return y + 1;
  }
  return MIN_Y;
}

int main(void) {
  Options o = parse_options();
  o.grid = 1;
  o.ox = 16;
  o.oz = 8;
  o.threads = 1;
  gen_init(o.seed);
  Gen *g = G;
  embed_cache_build(g);

  FeatureChunk *slot = feat_chunk_new(16, 8);
  feat_generate_terrain(g, 16, 8, slot);
  FeatureChunk *win[3][3];
  for (int dz = 0; dz < 3; dz++) {
    for (int dx = 0; dx < 3; dx++) {
      if (dz == 1 && dx == 1) {
        win[dz][dx] = slot;
      } else {
        win[dz][dx] = feat_chunk_new(15 + dx, 7 + dz);
        feat_generate_terrain(g, 15 + dx, 7 + dz, win[dz][dx]);
      }
    }
  }
  ChunkEval ce;
  memset(&ce, 0, sizeof ce);
  chunk_eval_begin(g, &ce, 16, 8);
  gen_features_window(g, &ce, win, slot);

  const char *names[FEAT_HM_COUNT] = {"OCEAN_FLOOR_WG", "WORLD_SURFACE_WG", "MOTION_BLOCKING",
                                      "MB_NO_LEAVES", "WORLD_SURFACE", "OCEAN_FLOOR"};
  int bad[FEAT_HM_COUNT];
  memset(bad, 0, sizeof bad);
  for (int hm = 0; hm < FEAT_HM_COUNT; hm++) {
    for (int lz = 0; lz < 16; lz++) {
      for (int lx = 0; lx < 16; lx++) {
        int want = from_scratch(slot, hm, lx, lz);
        int got = slot->hm[(size_t)hm * 256 + lz * 16 + lx];
        if (want != got) {
          if (bad[hm] < 5) {
            fprintf(stderr, "  %s mismatch at local (%d,%d): maintained=%d from_scratch=%d\n",
                    names[hm], lx, lz, got, want);
          }
          bad[hm]++;
        }
      }
    }
  }
  for (int hm = 0; hm < FEAT_HM_COUNT; hm++) {
    fprintf(stderr, "%-16s primed=%d columns_wrong=%d\n", names[hm], slot->hm_primed[hm], bad[hm]);
  }
  return 0;
}
