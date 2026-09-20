// Embed JSON cache for the feature stage.
//
// The feature stage walks every placed feature of a chunk and each walk needs
// parsed JSON: the placed feature, the biome's feature lists, the configured
// feature and the nested placed features the tree and patch features reference.
// Cinder's embed lookup is a linear scan over ~660 entries with a char-by-char
// compare, and the JSON was re-parsed for every one of those lookups - which was
// both the feature stage's dominant cost and a thread-safety problem (jparse
// appends to the shared world arena).
//
// Both go away here: one hash table over the embed keys, and one parsed Jv per
// key, built up front. After embed_cache_build() the tables are read-only, so
// the feature pass can run on any number of threads.
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H

/* The generated embed table (see src/vanilla/embed.h). */
const char *cinder_embed_get(const char *key);

typedef struct {
  const char *key;
  const char *json;
  Jv *parsed; /* non-null once the JSON has been parsed into the world arena */
} EmbedEnt;

typedef struct {
  EmbedEnt *ent;
  int count;
  int mask; /* (bucket_count - 1) */
  int *buckets;
} EmbedCache;

static EmbedCache EMBED;

static unsigned embed_hash(const char *s) {
  unsigned h = 2166136261u;
  while (*s) {
    h ^= (unsigned char)*s++;
    h *= 16777619u;
  }
  return h;
}

static void embed_put(const char *key, const char *json) {
  unsigned h = embed_hash(key) & (unsigned)EMBED.mask;
  while (EMBED.buckets[h] >= 0) {
    if (!strcmp(EMBED.ent[EMBED.buckets[h]].key, key)) return;
    h = (h + 1) & (unsigned)EMBED.mask;
  }
  int i = EMBED.count++;
  EMBED.ent[i].key = key;
  EMBED.ent[i].json = json;
  EMBED.ent[i].parsed = 0;
  EMBED.buckets[h] = i;
}

/* One pass over the generated table, then one parse per feature-stage key. */
void embed_cache_build(Gen *g);
