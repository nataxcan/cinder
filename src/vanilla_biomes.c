// Biomes: the vanilla overworld multi-noise parameter list and its RTree.
//
// Mirrors OverworldBiomeBuilder (data in vanilla_biomes.h, generated from the
// real server jar) plus Climate.ParameterList / Climate.RTree semantics:
// building the tree exactly like vanilla (same comparator chains, same stable
// sorts, same bucketization, same bounding boxes) so that ties between equally
// fit parameter points resolve the same way, and searching with the previous
// leaf as the initial candidate (vanilla keeps that in an RTree thread-local;
// we chain it per chunk).
/* Bend inlines this file into a temporary directory, so sibling includes are
 * absolute - the same convention the generated embed.h include already uses. */
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H

#ifndef CINDER_INC_VANILLA_BIOMES_H
#define CINDER_INC_VANILLA_BIOMES_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_biomes.h"
#endif
#include CINDER_INC_VANILLA_BIOMES_H


const char *biome_name(int biome) {
  if (biome < 0 || biome >= BIO_COUNT) return "minecraft:plains";
  return BIOME_NAMES[biome];
}

/* --------------------------------------------------------------- parameter */

static inline int64_t param_distance(int64_t pmin, int64_t pmax, int64_t target) {
  int64_t above = target - pmax;
  int64_t below = pmin - target;
  return above > 0 ? above : (below > 0 ? below : 0);
}

static inline int64_t sq(int64_t v) { return v * v; }

/* ------------------------------------------------------------- search tree */

typedef struct BNode {
  int is_leaf;
  int64_t pmin[7], pmax[7];
  int biome;
  int leaf_index;
  struct BNode **children;
  int nchildren;
} BNode;

typedef struct Brtree {
  BNode **arena;
  int n, cap;
  BNode *root;
  BNode **leaves;
  int leaf_count;
} Brtree;

typedef struct {
  BNode **kids;
  int n;
  int64_t pmin[7], pmax[7];
} Bucket;

static BNode *bnode_new(Brtree *t) {
  if (t->n >= t->cap) {
    t->cap = t->cap ? t->cap * 2 : 1024;
    t->arena = (BNode **)realloc(t->arena, (size_t)t->cap * sizeof(BNode *));
  }
  BNode *n = (BNode *)calloc(1, sizeof(BNode));
  t->arena[t->n++] = n;
  return n;
}

static void leaf_space(BNode *n, const BiomeParamPoint *p) {
  n->pmin[0] = p->temperature[0];
  n->pmax[0] = p->temperature[1];
  n->pmin[1] = p->humidity[0];
  n->pmax[1] = p->humidity[1];
  n->pmin[2] = p->continentalness[0];
  n->pmax[2] = p->continentalness[1];
  n->pmin[3] = p->erosion[0];
  n->pmax[3] = p->erosion[1];
  n->pmin[4] = p->depth[0];
  n->pmax[4] = p->depth[1];
  n->pmin[5] = p->weirdness[0];
  n->pmax[5] = p->weirdness[1];
  n->pmin[6] = p->offset;
  n->pmax[6] = p->offset;
}

static void span_into(int64_t *pmin, int64_t *pmax, const BNode *child) {
  for (int d = 0; d < 7; d++) {
    if (child->pmin[d] < pmin[d]) pmin[d] = child->pmin[d];
    if (child->pmax[d] > pmax[d]) pmax[d] = child->pmax[d];
  }
}

static BNode *subtree_new(Brtree *t, BNode **children, int count) {
  BNode *n = bnode_new(t);
  n->is_leaf = 0;
  n->children = (BNode **)malloc((size_t)count * sizeof(BNode *));
  n->nchildren = count;
  memcpy(n->children, children, (size_t)count * sizeof(BNode *));
  for (int d = 0; d < 7; d++) {
    n->pmin[d] = children[0]->pmin[d];
    n->pmax[d] = children[0]->pmax[d];
  }
  for (int i = 1; i < count; i++) span_into(n->pmin, n->pmax, children[i]);
  return n;
}

/* Stable merge sorts; Java's List.sort is stable, qsort is not. */
static void merge_sort_dim(BNode **a, BNode **tmp, int lo, int hi, int start_dim, int absolute) {
  if (hi - lo < 2) return;
  int mid = lo + (hi - lo) / 2;
  merge_sort_dim(a, tmp, lo, mid, start_dim, absolute);
  merge_sort_dim(a, tmp, mid, hi, start_dim, absolute);
  int i = lo, j = mid, k = lo;
  while (i < mid && j < hi) {
    int cmp = 0;
    for (int step = 0; step < 7; step++) {
      int d = (start_dim + step) % 7;
      int64_t ca = (a[i]->pmin[d] + a[i]->pmax[d]) / 2;
      int64_t cb = (a[j]->pmin[d] + a[j]->pmax[d]) / 2;
      if (absolute) {
        if (ca < 0) ca = -ca;
        if (cb < 0) cb = -cb;
      }
      if (ca != cb) {
        cmp = ca < cb ? -1 : 1;
        break;
      }
    }
    if (cmp <= 0) {
      tmp[k++] = a[i++];
    } else {
      tmp[k++] = a[j++];
    }
  }
  while (i < mid) tmp[k++] = a[i++];
  while (j < hi) tmp[k++] = a[j++];
  for (int x = lo; x < hi; x++) a[x] = tmp[x];
}

static void sort_dim(BNode **a, int count, int dim, int absolute) {
  BNode **tmp = (BNode **)malloc((size_t)count * sizeof(BNode *));
  merge_sort_dim(a, tmp, 0, count, dim, absolute);
  free(tmp);
}

static int64_t total_magnitude(const BNode *n) {
  int64_t total = 0;
  for (int d = 0; d < 7; d++) total += llabs((n->pmin[d] + n->pmax[d]) / 2);
  return total;
}

static void sort_magnitude(BNode **a, int count) {
  if (count < 2) return;
  BNode **tmp = (BNode **)malloc((size_t)count * sizeof(BNode *));
  int width = 1;
  while (width < count) {
    for (int lo = 0; lo < count; lo += 2 * width) {
      int mid = lo + width < count ? lo + width : count;
      int hi = lo + 2 * width < count ? lo + 2 * width : count;
      int i = lo, j = mid, k = lo;
      while (i < mid && j < hi) {
        if (total_magnitude(a[i]) <= total_magnitude(a[j])) {
          tmp[k++] = a[i++];
        } else {
          tmp[k++] = a[j++];
        }
      }
      while (i < mid) tmp[k++] = a[i++];
      while (j < hi) tmp[k++] = a[j++];
      for (int x = lo; x < hi; x++) a[x] = tmp[x];
    }
    width *= 2;
  }
  free(tmp);
}

static void sort_buckets(Bucket *b, int count, int dim, int absolute) {
  Bucket *tmp = (Bucket *)malloc((size_t)count * sizeof(Bucket));
  int width = 1;
  while (width < count) {
    for (int lo = 0; lo < count; lo += 2 * width) {
      int mid = lo + width < count ? lo + width : count;
      int hi = lo + 2 * width < count ? lo + 2 * width : count;
      int i = lo, j = mid, k = lo;
      while (i < mid && j < hi) {
        int cmp = 0;
        for (int step = 0; step < 7; step++) {
          int d = (dim + step) % 7;
          int64_t ca = (b[i].pmin[d] + b[i].pmax[d]) / 2;
          int64_t cb = (b[j].pmin[d] + b[j].pmax[d]) / 2;
          if (absolute) {
            if (ca < 0) ca = -ca;
            if (cb < 0) cb = -cb;
          }
          if (ca != cb) {
            cmp = ca < cb ? -1 : 1;
            break;
          }
        }
        if (cmp <= 0) {
          tmp[k++] = b[i++];
        } else {
          tmp[k++] = b[j++];
        }
      }
      while (i < mid) tmp[k++] = b[i++];
      while (j < hi) tmp[k++] = b[j++];
      for (int x = lo; x < hi; x++) b[x] = tmp[x];
    }
    width *= 2;
  }
  free(tmp);
}

static BNode *build_tree(Brtree *t, BNode **children, int count) {
  if (count == 1) return children[0];
  if (count <= 6) {
    sort_magnitude(children, count);
    return subtree_new(t, children, count);
  }

  int64_t min_cost = INT64_MAX;
  int min_dim = -1;
  Bucket *best = 0;
  int best_n = 0;

  int expected = (int)pow(6.0, floor(log((double)count - 0.01) / log(6.0)));
  if (expected < 1) expected = 1;

  for (int dim = 0; dim < 7; dim++) {
    sort_dim(children, count, dim, 0);
    int nbuckets = (count + expected - 1) / expected;
    Bucket *buckets = (Bucket *)calloc((size_t)nbuckets, sizeof(Bucket));
    int64_t total_cost = 0;
    int off = 0;
    for (int b = 0; b < nbuckets; b++) {
      int len = count - off;
      if (len > expected) len = expected;
      buckets[b].kids = (BNode **)malloc((size_t)len * sizeof(BNode *));
      memcpy(buckets[b].kids, children + off, (size_t)len * sizeof(BNode *));
      buckets[b].n = len;
      for (int d = 0; d < 7; d++) {
        buckets[b].pmin[d] = buckets[b].kids[0]->pmin[d];
        buckets[b].pmax[d] = buckets[b].kids[0]->pmax[d];
      }
      for (int i = 1; i < len; i++) span_into(buckets[b].pmin, buckets[b].pmax, buckets[b].kids[i]);
      for (int d = 0; d < 7; d++) total_cost += llabs(buckets[b].pmax[d] - buckets[b].pmin[d]);
      off += len;
    }
    if (min_cost > total_cost) {
      if (best) {
        for (int b = 0; b < best_n; b++) free(best[b].kids);
        free(best);
      }
      min_cost = total_cost;
      min_dim = dim;
      best = buckets;
      best_n = nbuckets;
    } else {
      for (int b = 0; b < nbuckets; b++) free(buckets[b].kids);
      free(buckets);
    }
  }

  sort_buckets(best, best_n, min_dim, 1);

  BNode **built = (BNode **)malloc((size_t)best_n * sizeof(BNode *));
  for (int b = 0; b < best_n; b++) built[b] = build_tree(t, best[b].kids, best[b].n);
  BNode *node = subtree_new(t, built, best_n);
  free(built);
  for (int b = 0; b < best_n; b++) free(best[b].kids);
  free(best);
  return node;
}

/* -------------------------------------------------------------- searching */

static int64_t node_distance(const BNode *n, const int64_t target[7]) {
  int64_t dist = 0;
  for (int d = 0; d < 7; d++) dist += sq(param_distance(n->pmin[d], n->pmax[d], target[d]));
  return dist;
}

static const BNode *search_node(const BNode *n, const int64_t target[7], const BNode *candidate) {
  if (n->is_leaf) return n;
  int64_t min_distance = candidate ? node_distance(candidate, target) : INT64_MAX;
  const BNode *closest = candidate;
  for (int i = 0; i < n->nchildren; i++) {
    const BNode *child = n->children[i];
    int64_t child_distance = node_distance(child, target);
    if (min_distance > child_distance) {
      const BNode *leaf = search_node(child, target, closest);
      int64_t leaf_distance = child == leaf ? child_distance : node_distance(leaf, target);
      if (min_distance > leaf_distance) {
        min_distance = leaf_distance;
        closest = leaf;
      }
    }
  }
  return closest;
}

/* --------------------------------------------------------------------- api */

static Brtree *tree_create(const BiomeParamPoint *params, int count) {
  Brtree *t = (Brtree *)calloc(1, sizeof(Brtree));
  BNode **leaves = (BNode **)malloc((size_t)count * sizeof(BNode *));
  for (int i = 0; i < count; i++) {
    BNode *n = bnode_new(t);
    n->is_leaf = 1;
    n->biome = params[i].biome;
    n->leaf_index = i;
    leaf_space(n, &params[i]);
    leaves[i] = n;
  }
  t->leaves = leaves;
  t->leaf_count = count;
  t->root = build_tree(t, leaves, count);
  return t;
}

void biome_tree_init(Gen *g) {
  g->btree = (struct Brtree *)tree_create(BIOME_PARAMS, BIOME_PARAM_COUNT);
}

int biome_pick(Gen *g, const int64_t target[7], int *last_leaf) {
  Brtree *t = (Brtree *)g->btree;
  const BNode *candidate = 0;
  if (last_leaf && *last_leaf >= 0 && *last_leaf < t->leaf_count) {
    candidate = t->leaves[*last_leaf];
  }
  const BNode *leaf = search_node(t->root, target, candidate);
  if (last_leaf) *last_leaf = leaf->leaf_index;
  return leaf->biome;
}

static int biome_at_quart_chain(Gen *g, ChunkEval *ce, int quart_x, int quart_y, int quart_z,
                                int *last_leaf) {
  int x = quart_x * 4, y = quart_y * 4, z = quart_z * 4;
  // Climate.Sampler.sample quantizes (float)df values: (long)(v * 10000.0F).
  float t = (float)df_eval_at(g, ce, g->temp, x, y, z);
  float h = (float)df_eval_at(g, ce, g->veg, x, y, z);
  float c = (float)df_eval_at(g, ce, g->cont, x, y, z);
  float e = (float)df_eval_at(g, ce, g->eros, x, y, z);
  float d = (float)df_eval_at(g, ce, g->depth_d, x, y, z);
  float w = (float)df_eval_at(g, ce, g->ridges, x, y, z);
  int64_t target[7];
  target[0] = (int64_t)(t * 10000.0f);
  target[1] = (int64_t)(h * 10000.0f);
  target[2] = (int64_t)(c * 10000.0f);
  target[3] = (int64_t)(e * 10000.0f);
  target[4] = (int64_t)(d * 10000.0f);
  target[5] = (int64_t)(w * 10000.0f);
  target[6] = 0;
  return biome_pick(g, target, last_leaf);
}

int biome_at_quart(Gen *g, ChunkEval *ce, int quart_x, int quart_y, int quart_z) {
  int last = -1;
  return biome_at_quart_chain(g, ce, quart_x, quart_y, quart_z, &last);
}

int biome_at_block(Gen *g, ChunkEval *ce, int x, int y, int z) {
  return biome_at_quart(g, ce, x >> 2, y >> 2, z >> 2);
}

// ChunkAccess.fillBiomesFromNoise + LevelChunkSection.fillBiomesFromNoise:
// sections ascending, then x, y, z inside a section, sampled at quart coords.
void biome_fill_chunk(Gen *g, Chunk *c, ChunkEval *ce) {
  int quart_min_x = c->cx * 4;
  int quart_min_z = c->cz * 4;
  int last_leaf = -1;
  for (int sec = 0; sec < 24; sec++) {
    int quart_min_y = (sec - 4) * 4;
    for (int x = 0; x < 4; x++) {
      for (int y = 0; y < 4; y++) {
        for (int z = 0; z < 4; z++) {
          int biome = biome_at_quart_chain(g, ce, quart_min_x + x, quart_min_y + y, quart_min_z + z,
                                           &last_leaf);
          c->biome[sec * 64 + y * 16 + z * 4 + x] = (uint8_t)biome;
        }
      }
    }
  }
}

/* ------------------------------------------------------- biome temperature */

static double BIOME_TEMP[BIO_COUNT];
static int BIOME_TEMP_READY;

void biome_init(Gen *g) {
  biome_tree_init(g);
  if (BIOME_TEMP_READY) return;
  for (int b = 0; b < BIO_COUNT; b++) {
    const char *name = biome_name(b);
    const char *path = strchr(name, ':');
    path = path ? path + 1 : name;
    char key[96];
    snprintf(key, sizeof key, "biome:%s", path);
    const char *json = cinder_embed_get(key);
    double temp = 0.8;
    if (json) {
      Arena tmp = {0};
      Jv *root = jparse(&tmp, json);
      temp = jnum(jget(root, "temperature"), 0.8);
      free(tmp.arena);
    }
    BIOME_TEMP[b] = temp;
  }
  BIOME_TEMP_READY = 1;
}

double biome_temperature(Gen *g, int biome) {
  (void)g;
  if (biome < 0 || biome >= BIO_COUNT) return 0.8;
  return BIOME_TEMP[biome];
}
