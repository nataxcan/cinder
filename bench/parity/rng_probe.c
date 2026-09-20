// Cinder's side of the RNG comparison: prints the decoration seed and the first
// draws of the feature RNG for the same (seed, chunk) as bench/parity/RngProbe.java.
#include "vanilla_gen.c"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Gen g;
  memset(&g, 0, sizeof g);
  g.world_seed = 1;
  uint64_t decoration = feature_decoration_seed(&g, 256, 128);
  printf("decorationSeed=%llu\n", (unsigned long long)decoration);
  int steps[] = {0, 1, 2, 6, 9, 10};
  int indices[] = {0, 1, 5, 10, 20, 33, 40};
  for (size_t s = 0; s < sizeof steps / sizeof steps[0]; s++) {
    for (size_t i = 0; i < sizeof indices / sizeof indices[0]; i++) {
      LegacyRng r;
      legacy_set_seed(&r, decoration + (uint64_t)indices[i] + (uint64_t)(10000 * steps[s]));
      char buf[512];
      int n = 0;
      n += snprintf(buf + n, sizeof buf - n, "step=%d index=%d ", steps[s], indices[i]);
      for (int k = 0; k < 5; k++) n += snprintf(buf + n, sizeof buf - n, "%d ", legacy_next_bound(&r, 16));
      n += snprintf(buf + n, sizeof buf - n, "| floats");
      for (int k = 0; k < 3; k++) { float f = legacy_next_float(&r); uint32_t bits; memcpy(&bits, &f, 4); n += snprintf(buf + n, sizeof buf - n, " %u", bits); }
      n += snprintf(buf + n, sizeof buf - n, " | longs ");
      for (int k = 0; k < 2; k++) n += snprintf(buf + n, sizeof buf - n, " %lld", (long long)legacy_next_long(&r));
      printf("%s\n", buf);
    }
  }
  return 0;
}
