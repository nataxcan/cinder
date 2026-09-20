// Embed JSON cache for the feature stage - see src/vanilla_embed_cache.h.
//
// After embed_cache_build() every table here is read-only, so the feature pass
// may run on any number of threads: no arena growth and no lazy parsing happens
// once the build is done.
#ifndef CINDER_INC_VANILLA_EMBED_CACHE_H
#define CINDER_INC_VANILLA_EMBED_CACHE_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_embed_cache.h"
#endif
#include CINDER_INC_VANILLA_EMBED_CACHE_H

/* The generated table (CINDER_EMBED / CINDER_EMBED_COUNT) must already be
 * visible: vanilla_gen.c includes it before this file, and including it again
 * here would pull in a second copy of the table under the scratch build's
 * rewritten include path. */

static int embed_index(const char *key) {
  if (!EMBED.count) return -1;
  unsigned h = embed_hash(key) & (unsigned)EMBED.mask;
  for (;;) {
    int i = EMBED.buckets[h];
    if (i < 0) return -1;
    if (!strcmp(EMBED.ent[i].key, key)) return i;
    h = (h + 1) & (unsigned)EMBED.mask;
  }
}

/* Raw JSON for a key, or 0 when the embed has no such key. */
const char *embed_str(const char *key) {
  int i = embed_index(key);
  return i < 0 ? 0 : EMBED.ent[i].json;
}

/* Parsed JSON for a key, or 0 when the embed has no such key. The parse happens
 * on the first call, which must be during embed_cache_build(). */
Jv *embed_json(Gen *g, const char *key) {
  int i = embed_index(key);
  if (i < 0) return 0;
  if (!EMBED.ent[i].parsed) EMBED.ent[i].parsed = jparse(&g->arena, EMBED.ent[i].json);
  return EMBED.ent[i].parsed;
}

static int embed_is_stage_key(const char *key) {
  return !strncmp(key, "pf:", 3) || !strncmp(key, "cf:", 3) || !strncmp(key, "biome:", 6);
}

void embed_cache_build(Gen *g) {
  if (EMBED.count) return;
  int cap = 1;
  while (cap < CINDER_EMBED_COUNT * 2) cap <<= 1;
  EMBED.ent = (EmbedEnt *)calloc((size_t)CINDER_EMBED_COUNT, sizeof(EmbedEnt));
  EMBED.buckets = (int *)malloc((size_t)cap * sizeof(int));
  EMBED.mask = cap - 1;
  for (int i = 0; i < cap; i++) EMBED.buckets[i] = -1;
  for (size_t i = 0; i < CINDER_EMBED_COUNT; i++) {
    embed_put(CINDER_EMBED[i].key, CINDER_EMBED[i].json);
  }
  /* parse the keys the feature stage can ask for, so no thread ever parses */
  Jv *roots = 0;
  (void)roots;
  for (int i = 0; i < EMBED.count; i++) {
    if (embed_is_stage_key(EMBED.ent[i].key)) {
      EMBED.ent[i].parsed = jparse(&g->arena, EMBED.ent[i].json);
    }
  }
}
