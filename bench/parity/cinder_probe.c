// Standalone probe: prints Cinder's density-function / biome values so they can
// be diffed against the vanilla probe (bench/parity/vanilla_probe.java).
//
// Built from a scratch copy of the sources by scripts/build_core_test.sh, e.g.
//   /.cache/cinder-core/cinder-probe --router final_density --seed 1 --points pts.txt
// Points are block coordinates, one "x y z" per line.
#include "vanilla_gen.c"

static DfNode *pick_router(Gen *g, const char *name) {
  if (!strcmp(name, "final_density")) return g->final_d;
  if (!strcmp(name, "barrier")) return g->barrier;
  if (!strcmp(name, "fluid_level_floodedness")) return g->flood;
  if (!strcmp(name, "fluid_level_spread")) return g->spread;
  if (!strcmp(name, "lava")) return g->lava;
  if (!strcmp(name, "temperature")) return g->temp;
  if (!strcmp(name, "vegetation")) return g->veg;
  if (!strcmp(name, "continents")) return g->cont;
  if (!strcmp(name, "erosion")) return g->eros;
  if (!strcmp(name, "depth")) return g->depth_d;
  if (!strcmp(name, "ridges")) return g->ridges;
  if (!strcmp(name, "preliminary_surface_level")) return g->prelim;
  if (!strcmp(name, "vein_toggle")) return g->vein_toggle;
  if (!strcmp(name, "vein_ridged")) return g->vein_ridged;
  if (!strcmp(name, "vein_gap")) return g->vein_gap;
  return 0;
}

int main(int argc, char **argv) {
  const char *mode = "df";
  const char *what = "minecraft:overworld/sloped_cheese";
  const char *points = 0;
  uint32_t seed = 1;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--df") && i + 1 < argc) what = argv[++i];
    else if (!strcmp(argv[i], "--router") && i + 1 < argc) { mode = "router"; what = argv[++i]; }
    else if (!strcmp(argv[i], "--noise") && i + 1 < argc) { mode = "noise"; what = argv[++i]; }
    else if (!strcmp(argv[i], "--biome")) mode = "biome";
    else if (!strcmp(argv[i], "--climate")) mode = "climate";
    else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], 0, 10);
    else if (!strcmp(argv[i], "--points") && i + 1 < argc) points = argv[++i];
  }
  FILE *in = points ? fopen(points, "r") : stdin;
  if (!in) {
    fprintf(stderr, "cinder-probe: cannot open %s\n", points);
    return 2;
  }

  gen_init(seed);
  ChunkEval ce;
  memset(&ce, 0, sizeof ce);
  // A per-chunk context is only needed for wrapped nodes; use the chunk that
  // contains the point (points are expected to be sparse but chunk-local use is
  // fine because interpolated/flat caches are position-derived).
  chunk_eval_begin(G, &ce, 0, 0);

  DfNode *node = 0;
  if (!strcmp(mode, "df")) node = df_compile_named(G, what);
  else if (!strcmp(mode, "router")) node = pick_router(G, what);

  int x, y, z;
  int current_cx = INT32_MIN, current_cz = INT32_MIN;
  while (fscanf(in, "%d %d %d", &x, &y, &z) == 3) {
    int cx = x >= 0 ? x / 16 : -((-x + 15) / 16);
    int cz = z >= 0 ? z / 16 : -((-z + 15) / 16);
    if (cx != current_cx || cz != current_cz) {
      chunk_eval_begin(G, &ce, cx, cz);
      current_cx = cx;
      current_cz = cz;
    }
    if (!strcmp(mode, "biome")) {
      printf("%d %d %d %s\n", x, y, z, biome_name(biome_at_quart(G, &ce, x, y, z)));
    } else if (!strcmp(mode, "climate")) {
      /* quantized Climate.TargetPoint at quart coordinates */
      int bx = x * 4, by = y * 4, bz = z * 4;
      float t = (float)df_eval_at(G, &ce, G->temp, bx, by, bz);
      float h = (float)df_eval_at(G, &ce, G->veg, bx, by, bz);
      float c = (float)df_eval_at(G, &ce, G->cont, bx, by, bz);
      float e = (float)df_eval_at(G, &ce, G->eros, bx, by, bz);
      float d = (float)df_eval_at(G, &ce, G->depth_d, bx, by, bz);
      float w = (float)df_eval_at(G, &ce, G->ridges, bx, by, bz);
      printf("%d %d %d %lld %lld %lld %lld %lld %lld\n", x, y, z, (long long)(t * 10000.0f),
             (long long)(h * 10000.0f), (long long)(c * 10000.0f), (long long)(e * 10000.0f),
             (long long)(d * 10000.0f), (long long)(w * 10000.0f));
    } else if (!strcmp(mode, "noise")) {
      NormalNoise *nn = gen_noise(G, what);
      printf("%d %d %d %.17g\n", x, y, z, normal_noise_get(nn, (double)x, (double)y, (double)z));
    } else {
      printf("%d %d %d %.17g\n", x, y, z, df_eval_at(G, &ce, node, x, y, z));
    }
  }
  if (points) fclose(in);
  return 0;
}
