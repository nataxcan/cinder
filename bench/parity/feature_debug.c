// TEMPORARY debug: run one chunk's features and report what landed.
#include "vanilla_gen.c"

int main(void) {
  Options o = parse_options();
  o.grid = 1;
  o.ox = 16;
  o.oz = 8;
  o.threads = 1;
  gen_init(o.seed);

  FeatureChunk *slot = feat_chunk_new(16, 8);
  feat_generate_terrain(G, 16, 8, slot);
  FeatureChunk *win[3][3];
  for (int dz = 0; dz < 3; dz++) {
    for (int dx = 0; dx < 3; dx++) {
      if (dz == 1 && dx == 1) {
        win[dz][dx] = slot;
      } else {
        win[dz][dx] = feat_chunk_new(15 + dx, 7 + dz);
        feat_generate_terrain(G, 15 + dx, 7 + dz, win[dz][dx]);
      }
    }
  }
  ChunkEval ce;
  memset(&ce, 0, sizeof ce);
  chunk_eval_begin(G, &ce, 16, 8);
  gen_features_window(G, &ce, win, slot);

  int leaves = 0, logs = 0, grass = 0, flowers = 0, coal = 0, litter = 0, ice = 0, bush = 0, mush = 0;
  for (int i = 0; i < 16 * 384 * 16; i++) {
    uint16_t b = slot->b[i];
    if (is_leaves(b)) leaves++;
    if (b == B_OAK_LOG || b == B_BIRCH_LOG) logs++;
    if (b == B_SHORT_GRASS || b == B_TALL_GRASS) grass++;
    if (b == B_DANDELION || b == B_POPPY) flowers++;
    if (b == B_COAL || b == B_DS_COAL) coal++;
    if (b == B_LEAF_LITTER) litter++;
    if (b == B_ICE) ice++;
    if (b == B_BUSH) bush++;
    if (b == B_BROWN_MUSHROOM || b == B_RED_MUSHROOM) mush++;
  }
  fprintf(stderr, "after features: leaves=%d logs=%d grass=%d flowers=%d coal=%d litter=%d\n",
          leaves, logs, grass, flowers, coal, litter);
  return 0;
}
