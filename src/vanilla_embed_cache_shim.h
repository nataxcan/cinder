// Embed JSON cache for the feature stage.
//
// The feature stage walks hundreds of placed features per chunk and each walk
// needs parsed JSON: the placed feature, the biome's feature lists, the
// configured feature and the nested placed features the tree/patch features
// reference. Parsing those on every chunk was both slow and thread-unsafe (the
// world arena is shared), and the feature pass needs to run on many threads.
//
// Every key the stage can ask for is known statically (the pf:, cf: and biome:
// embed keys), so they are parsed once at init and looked up by key afterwards.
#ifndef CINDER_INC_VANILLA_EMBED_CACHE_C
#define CINDER_INC_VANILLA_EMBED_CACHE_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_embed_cache.c"
#endif
#include CINDER_INC_VANILLA_EMBED_CACHE_C
