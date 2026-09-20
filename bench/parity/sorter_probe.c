// Cinder's side of the feature-order comparison: prints the same
// features-per-step list as bench/parity/FeatureOrderProbe.java so the two can
// be diffed directly.
#include "vanilla_gen.c"

int main(void) {
  Gen g;
  memset(&g, 0, sizeof g);
  g.world_seed = 1;
  embed_cache_build(&g);
  feat_sorter_init(&g);
  /* possibleBiomes(): the distinct biomes of the parameter list, in list order */
  int seen[BIO_COUNT];
  memset(seen, 0, sizeof seen);
  int distinct = 0;
  for (int p = 0; p < BIOME_PARAM_COUNT; p++) {
    int b = BIOME_PARAMS[p].biome;
    if (!seen[b]) {
      seen[b] = 1;
      distinct++;
    }
  }
  printf("# possible biomes: %d\n", distinct);
  {
    int printed[BIO_COUNT];
    memset(printed, 0, sizeof printed);
    for (int p = 0; p < BIOME_PARAM_COUNT; p++) {
      int b = BIOME_PARAMS[p].biome;
      if (printed[b]) continue;
      printed[b] = 1;
      printf("biome %s\n", biome_name(b));
    }
  }
  printf("# steps: %d\n", SORT.nsteps);
  for (int step = 0; step < SORT.nsteps; step++) {
    printf("step %d count %d\n", step, SORT.step_counts[step]);
    for (int i = 0; i < SORT.step_counts[step]; i++) {
      /* the embed key is "pf:<name>"; Java prints "minecraft:<name>" */
      const char *key = SORT.steps[step][i].name;
      const char *bare = key;
      if (!strncmp(bare, "pf:", 3)) bare += 3;
      printf("  %d minecraft:%s\n", i, bare);
    }
  }
  return 0;
}
