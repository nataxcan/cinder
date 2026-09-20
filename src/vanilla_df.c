// Density functions: compile vanilla's JSON graph and evaluate it exactly.
//
// Mirrors net.minecraft.world.level.levelgen.DensityFunctions plus the
// per-chunk wrapping that NoiseChunk applies (interpolated / flat_cache /
// cache_2d / cache_once / blend_density). Every arithmetic detail that can
// shift a bit is preserved: Java's lerp nesting (NoiseChunk's interpolator
// lerps y, then x, then z, unlike Mth.lerp3), the y=0 sampling of flat caches,
// and NaN-in/NaN-out min/max.
/* Bend inlines this file into a temporary directory, so sibling includes are
 * absolute - the same convention the generated embed.h include already uses. */
#ifndef CINDER_INC_VANILLA_COMMON_H
#define CINDER_INC_VANILLA_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_common.h"
#endif
#include CINDER_INC_VANILLA_COMMON_H


/* --------------------------------------------------------------- noise ids */

NormalNoise *gen_noise(Gen *g, const char *id) {
  static pthread_mutex_t noise_mu = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&noise_mu);
  for (int i = 0; i < g->nnoise; i++) {
    if (!strcmp(g->noises[i].id, id)) {
      NormalNoise *found = g->noises[i].nn;
      pthread_mutex_unlock(&noise_mu);
      return found;
    }
  }
  if (g->nnoise >= (int)(sizeof g->noises / sizeof g->noises[0])) {
    fprintf(stderr, "cinder: too many noise instances, missing %s\n", id);
    abort();
  }
  int first = -7;
  double amps[32];
  int namp = 1;
  amps[0] = 1.0;
  const char *json = cinder_embed_get(id);
  if (json) {
    Arena tmp = {0};
    Jv *root = jparse(&tmp, json);
    first = (int)jnum(jget(root, "firstOctave"), -7);
    Jv *arr = jget(root, "amplitudes");
    namp = 0;
    for (Jv *c = arr ? arr->child : 0; c && namp < 32; c = c->next) amps[namp++] = jnum(c, 1.0);
    if (namp == 0) {
      namp = 1;
      amps[0] = 1;
    }
    free(tmp.arena);
  } else {
    fprintf(stderr, "cinder: unknown noise parameters %s\n", id);
  }
  // Noises.instantiate: the noise's RandomSource is world_random.fromHashOf(id).
  Xoro random;
  xoro_from_hash(g->world_random, id, &random);
  NoiseEnt *e = &g->noises[g->nnoise++];
  snprintf(e->id, sizeof e->id, "%s", id);
  e->nn = normal_noise_create(&random, first, amps, namp);
  NormalNoise *created = e->nn;
  pthread_mutex_unlock(&noise_mu);
  return created;
}

XoroFactory gen_random_factory(Gen *g, const char *name) {
  Xoro hashed;
  xoro_from_hash(g->world_random, name, &hashed);
  return xoro_fork_positional(&hashed);
}

/* ----------------------------------------------------------- interpolation */

// NoiseChunk.NoiseInterpolator lerps y first, then x, then z.
// n[0]=000 n[1]=100 n[2]=010 n[3]=110 n[4]=001 n[5]=101 n[6]=011 n[7]=111
static inline double interp_lerp(double fz, double fx, double fy, const double n[8]) {
  double xz00 = jlerp(fy, n[0], n[2]);
  double xz10 = jlerp(fy, n[1], n[3]);
  double xz01 = jlerp(fy, n[4], n[6]);
  double xz11 = jlerp(fy, n[5], n[7]);
  double z0 = jlerp(fx, xz00, xz10);
  double z1 = jlerp(fx, xz01, xz11);
  return jlerp(fz, z0, z1);
}

#define INTERP_GRID_LEN (5 * 49 * 5)

static void interp_fill(Gen *g, ChunkEval *ce, InterpGrid *ig) {
  DfNode *n = ig->node;
  if (!ig->grid) ig->grid = (double *)calloc(INTERP_GRID_LEN, sizeof(double));
  int min_x = ce->cx * 16, min_z = ce->cz * 16;
  for (int iz = 0; iz < 5; iz++) {
    for (int iy = 0; iy < 49; iy++) {
      for (int ix = 0; ix < 5; ix++) {
        double v = df_eval_at(g, ce, n->a, min_x + ix * CELL_XZ, MIN_Y + iy * CELL_Y,
                              min_z + iz * CELL_XZ);
        ig->grid[((iy * 5) + iz) * 5 + ix] = v;
      }
    }
  }
  ig->filled = 1;
}

static double interp_compute(Gen *g, ChunkEval *ce, DfNode *n) {
  for (int i = 0; i < ce->ninterp; i++) {
    InterpGrid *ig = &ce->interp[i];
    if (ig->node != n) continue;
    if (!ig->filled) interp_fill(g, ce, ig);
    int ix = ce->x - ce->cx * 16;
    int iz = ce->z - ce->cz * 16;
    int iy = ce->y - MIN_Y;
    int cx0 = ix >= 0 ? ix / CELL_XZ : -1;
    int cz0 = iz >= 0 ? iz / CELL_XZ : -1;
    int cy0 = iy >= 0 ? iy / CELL_Y : -1;
    if (cx0 < 0 || cx0 > 3 || cz0 < 0 || cz0 > 3 || cy0 < 0 || cy0 > 47) {
      return df_eval_at(g, ce, n->a, ce->x, ce->y, ce->z);
    }
    double fx = (double)(ix - cx0 * CELL_XZ) / CELL_XZ;
    double fy = (double)(iy - cy0 * CELL_Y) / CELL_Y;
    double fz = (double)(iz - cz0 * CELL_XZ) / CELL_XZ;
    const int ox[8] = {0, 1, 0, 1, 0, 1, 0, 1};
    const int oy[8] = {0, 0, 1, 1, 0, 0, 1, 1};
    const int oz[8] = {0, 0, 0, 0, 1, 1, 1, 1};
    double c[8];
    for (int k = 0; k < 8; k++) {
      int gx = cx0 + ox[k], gy = cy0 + oy[k], gz = cz0 + oz[k];
      c[k] = ig->grid[((gy * 5) + gz) * 5 + gx];
    }
    return interp_lerp(fz, fx, fy, c);
  }
  return df_eval_at(g, ce, n->a, ce->x, ce->y, ce->z);
}

/* ------------------------------------------------------------- flat cache */

// NoiseChunk.FlatCache: 5x5 values over the chunk's quartz grid at block
// coordinates, always sampled with y = 0, and reused for every y.
static double flat_compute(Gen *g, ChunkEval *ce, DfNode *n) {
  for (int i = 0; i < ce->nflat; i++) {
    FlatEntry *fe = &ce->flat[i];
    if (fe->node != n) continue;
    int min_x = ce->cx * 16, min_z = ce->cz * 16;
    int ix = (ce->x >> 2) - (min_x >> 2);
    int iz = (ce->z >> 2) - (min_z >> 2);
    if (ix < 0 || ix > 4 || iz < 0 || iz > 4) return df_eval_at(g, ce, n->a, ce->x, ce->y, ce->z);
    int slot = iz * 5 + ix;
    if (!fe->filled[slot]) {
      fe->filled[slot] = 1;
      fe->v[slot] = df_eval_at(g, ce, n->a, (min_x >> 2) * 4 + ix * 4, 0, (min_z >> 2) * 4 + iz * 4);
    }
    return fe->v[slot];
  }
  return df_eval_at(g, ce, n->a, ce->x, ce->y, ce->z);
}

/* --------------------------------------------------------------- evaluation */

static double eval_node(Gen *g, ChunkEval *ce, DfNode *n);
static float spline_sample(Gen *g, ChunkEval *ce, const Spline *s);

double df_eval_at(Gen *g, ChunkEval *ce, DfNode *n, int x, int y, int z) {
  if (!n) return 0.0;
  int ox = ce->x, oy = ce->y, oz = ce->z;
  ce->x = x;
  ce->y = y;
  ce->z = z;
  double v = eval_node(g, ce, n);
  ce->x = ox;
  ce->y = oy;
  ce->z = oz;
  return v;
}

static double eval_node(Gen *g, ChunkEval *ce, DfNode *n) {
  switch (n->kind) {
  case DF_CONST:
    return n->c;
  case DF_ADD:
    return eval_node(g, ce, n->a) + eval_node(g, ce, n->b);
  case DF_MUL:
    return eval_node(g, ce, n->a) * eval_node(g, ce, n->b);
  case DF_MIN:
    return jmin(eval_node(g, ce, n->a), eval_node(g, ce, n->b));
  case DF_MAX:
    return jmax(eval_node(g, ce, n->a), eval_node(g, ce, n->b));
  case DF_ABS: {
    double a = eval_node(g, ce, n->a);
    return a < 0.0 ? -a : a;
  }
  case DF_SQUARE: {
    double a = eval_node(g, ce, n->a);
    return a * a;
  }
  case DF_CUBE: {
    double a = eval_node(g, ce, n->a);
    return a * a * a;
  }
  case DF_HALF_NEG: {
    double a = eval_node(g, ce, n->a);
    return a > 0.0 ? a : a * 0.5;
  }
  case DF_QUART_NEG: {
    double a = eval_node(g, ce, n->a);
    return a > 0.0 ? a : a * 0.25;
  }
  case DF_SQUEEZE: {
    double c = jclamp(eval_node(g, ce, n->a), -1.0, 1.0);
    return c / 2.0 - c * c * c / 24.0;
  }
  case DF_INVERT:
    return 1.0 / eval_node(g, ce, n->a);
  case DF_CLAMP:
    return jclamp(eval_node(g, ce, n->a), n->from_v, n->to_v);
  case DF_NOISE:
    if (!n->noise) return 0.0;
    return normal_noise_get(n->noise, ce->x * n->xz_scale, ce->y * n->y_scale, ce->z * n->xz_scale);
  case DF_SHIFTED: {
    if (!n->noise) return 0.0;
    double sx = eval_node(g, ce, n->a);
    double sy = eval_node(g, ce, n->b);
    double sz = eval_node(g, ce, n->c3);
    return normal_noise_get(n->noise, ce->x * n->xz_scale + sx, ce->y * n->y_scale + sy,
                            ce->z * n->xz_scale + sz);
  }
  case DF_SHIFT_A:
    if (!n->noise) return 0.0;
    return normal_noise_get(n->noise, ce->x * 0.25, 0.0, ce->z * 0.25) * 4.0;
  case DF_SHIFT_B:
    if (!n->noise) return 0.0;
    return normal_noise_get(n->noise, ce->z * 0.25, ce->x * 0.25, 0.0) * 4.0;
  case DF_RANGE: {
    double in = eval_node(g, ce, n->a);
    if (in >= n->from_v && in < n->to_v) return eval_node(g, ce, n->b);
    return eval_node(g, ce, n->c3);
  }
  case DF_YCLAMP:
    return jclamped_map((double)ce->y, (double)n->from_y, (double)n->to_y, n->from_v, n->to_v);
  case DF_CACHE2D: {
    int id = n->cid;
    if (ce->c2d_ok[id] && ce->c2d_x[id] == ce->x && ce->c2d_z[id] == ce->z) return ce->c2d_v[id];
    double v = eval_node(g, ce, n->a);
    ce->c2d_ok[id] = 1;
    ce->c2d_x[id] = ce->x;
    ce->c2d_z[id] = ce->z;
    ce->c2d_v[id] = v;
    return v;
  }
  case DF_CACHE_ONCE: {
    int id = n->cid;
    if (ce->c1_ok[id] && ce->c1_x[id] == ce->x && ce->c1_y[id] == ce->y && ce->c1_z[id] == ce->z) {
      return ce->c1_v[id];
    }
    double v = eval_node(g, ce, n->a);
    ce->c1_ok[id] = 1;
    ce->c1_x[id] = ce->x;
    ce->c1_y[id] = ce->y;
    ce->c1_z[id] = ce->z;
    ce->c1_v[id] = v;
    return v;
  }
  case DF_FLAT:
    return flat_compute(g, ce, n);
  case DF_INTERP:
    return interp_compute(g, ce, n);
  case DF_BLEND_DENSITY:
    return eval_node(g, ce, n->a);
  case DF_BLEND_A:
    return 1.0;
  case DF_BLEND_O:
    return 0.0;
  case DF_OLD_BLEND:
    if (!n->blend) return 0.0;
    return blended_noise_compute(n->blend, ce->x, ce->y, ce->z);
  case DF_FIND_TOP: {
    int top = jfloor(eval_node(g, ce, n->b) / n->cell_height) * n->cell_height;
    if (top <= n->lower_bound) return (double)n->lower_bound;
    for (int block_y = top; block_y >= n->lower_bound; block_y -= n->cell_height) {
      double d = df_eval_at(g, ce, n->a, ce->x, block_y, ce->z);
      if (d > 0.0) return (double)block_y;
    }
    return (double)n->lower_bound;
  }
  case DF_SPLINE:
    if (!n->spline) return 0.0;
    return (double)spline_sample(g, ce, n->spline);
  case DF_INTERVAL_SELECT: {
    double input = eval_node(g, ce, n->a);
    for (int i = 0; i < n->nselect; i++) {
      if (input < n->thresholds[i]) return eval_node(g, ce, n->fns[i]);
    }
    return eval_node(g, ce, n->fns[n->nselect]);
  }
  default:
    return 0.0;
  }
}

/* ------------------------------------------------------------ cubic spline */

static inline float jlerpf(float a, float p0, float p1) { return p0 + a * (p1 - p0); }

/* Mth.binarySearch(0, n, i -> input < locations[i]) */
static int spline_interval_start(const float *locations, int n, float input) {
  int min = 0;
  int i = n - 0;
  while (i > 0) {
    int j = i / 2;
    int k = min + j;
    if (input < locations[k]) {
      i = j;
    } else {
      min = k + 1;
      i -= j + 1;
    }
  }
  return min;
}

static Spline *spline_compile(Gen *g, Jv *v);

static float spline_sample(Gen *g, ChunkEval *ce, const Spline *s) {
  if (s->is_const) return s->value;
  float input = (float)df_eval_at(g, ce, s->coordinate, ce->x, ce->y, ce->z);
  int start = spline_interval_start(s->locations, s->n, input) - 1;
  int last = s->n - 1;
  if (start < 0) {
    float value = spline_sample(g, ce, s->values[0]);
    float d = s->derivatives[0];
    return d == 0.0f ? value : value + d * (input - s->locations[0]);
  }
  if (start == last) {
    float value = spline_sample(g, ce, s->values[last]);
    float d = s->derivatives[last];
    return d == 0.0f ? value : value + d * (input - s->locations[last]);
  }
  float x1 = s->locations[start];
  float x2 = s->locations[start + 1];
  float t = (input - x1) / (x2 - x1);
  float y1 = spline_sample(g, ce, s->values[start]);
  float y2 = spline_sample(g, ce, s->values[start + 1]);
  float d1 = s->derivatives[start];
  float d2 = s->derivatives[start + 1];
  float a = d1 * (x2 - x1) - (y2 - y1);
  float b = -d2 * (x2 - x1) + (y2 - y1);
  return jlerpf(t, y1, y2) + t * (1.0f - t) * jlerpf(t, a, b);
}

static Spline *spline_compile(Gen *g, Jv *v) {
  Spline *s = (Spline *)agrow(&g->arena, sizeof(Spline));
  memset(s, 0, sizeof(*s));
  if (!v || v->kind == J_NUM) {
    s->is_const = 1;
    s->value = (float)jnum(v, 0.0);
    return s;
  }
  s->coordinate = df_compile(g, jget(v, "coordinate"));
  Jv *pts = jget(v, "points");
  int n = jarr_len(pts);
  if (n == 0) {
    s->is_const = 1;
    return s;
  }
  s->n = n;
  s->locations = (float *)agrow(&g->arena, sizeof(float) * (size_t)n);
  s->derivatives = (float *)agrow(&g->arena, sizeof(float) * (size_t)n);
  s->values = (Spline **)agrow(&g->arena, sizeof(Spline *) * (size_t)n);
  int i = 0;
  for (Jv *c = pts->child; c; c = c->next, i++) {
    s->locations[i] = (float)jnum(jget(c, "location"), 0.0);
    s->derivatives[i] = (float)jnum(jget(c, "derivative"), 0.0);
    s->values[i] = spline_compile(g, jget(c, "value"));
  }
  return s;
}

/* --------------------------------------------------------------- compiling */

static int NEXT_CID;

typedef struct {
  char name[96];
  DfNode *node;
  int busy;
} NamedDf;

static NamedDf *NAMED;
static int NNAMED, NNAMED_CAP;

static DfNode *df_new(Gen *g) {
  DfNode *n = (DfNode *)agrow(&g->arena, sizeof(DfNode));
  memset(n, 0, sizeof(*n));
  n->cid = NEXT_CID++;
  if (NEXT_CID >= MAX_CID) {
    fprintf(stderr, "cinder: too many density function nodes (%d)\n", NEXT_CID);
    abort();
  }
  return n;
}


DfNode *df_compile_named(Gen *g, const char *name) {
  for (int i = 0; i < NNAMED; i++) {
    if (!strcmp(NAMED[i].name, name)) {
      if (NAMED[i].busy) {
        DfNode *n = df_new(g);
        n->kind = DF_CONST;
        return n;
      }
      return NAMED[i].node;
    }
  }
  if (NNAMED >= NNAMED_CAP) {
    NNAMED_CAP = NNAMED_CAP ? NNAMED_CAP * 2 : 256;
    NAMED = (NamedDf *)realloc(NAMED, (size_t)NNAMED_CAP * sizeof(NamedDf));
  }
  NamedDf *slot = &NAMED[NNAMED++];
  snprintf(slot->name, sizeof slot->name, "%s", name);
  slot->busy = 1;
  const char *json = cinder_embed_get(name);
  if (!json) {
    fprintf(stderr, "cinder: missing density function %s\n", name);
    DfNode *n = df_new(g);
    n->kind = DF_CONST;
    slot->node = n;
    slot->busy = 0;
    return n;
  }
  Jv *root = jparse(&g->arena, json);
  slot->node = df_compile(g, root);
  slot->busy = 0;
  return slot->node;
}

DfNode *df_compile(Gen *g, Jv *v) {
  if (!v) {
    DfNode *n = df_new(g);
    n->kind = DF_CONST;
    return n;
  }
  if (v->kind == J_NUM) {
    DfNode *n = df_new(g);
    n->kind = DF_CONST;
    n->c = v->num;
    return n;
  }
  if (v->kind == J_STR) return df_compile_named(g, v->str);
  if (v->kind != J_OBJ) {
    DfNode *n = df_new(g);
    n->kind = DF_CONST;
    return n;
  }
  const char *type = jstr(jget(v, "type"));
  if (!type) {
    fprintf(stderr, "cinder: density function without a type\n");
    abort();
  }
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  DfNode *n = df_new(g);
#define BIN(k)                                                                 \
  do {                                                                         \
    n->kind = k;                                                               \
    n->a = df_compile(g, jget(v, "argument1"));                                \
    n->b = df_compile(g, jget(v, "argument2"));                                \
  } while (0)
#define UN(k)                                                                  \
  do {                                                                         \
    n->kind = k;                                                               \
    n->a = df_compile(g, jget(v, "argument"));                                 \
  } while (0)
  if (!strcmp(type, "add")) BIN(DF_ADD);
  else if (!strcmp(type, "mul")) BIN(DF_MUL);
  else if (!strcmp(type, "min")) BIN(DF_MIN);
  else if (!strcmp(type, "max")) BIN(DF_MAX);
  else if (!strcmp(type, "abs")) UN(DF_ABS);
  else if (!strcmp(type, "square")) UN(DF_SQUARE);
  else if (!strcmp(type, "cube")) UN(DF_CUBE);
  else if (!strcmp(type, "half_negative")) UN(DF_HALF_NEG);
  else if (!strcmp(type, "quarter_negative")) UN(DF_QUART_NEG);
  else if (!strcmp(type, "squeeze")) UN(DF_SQUEEZE);
  else if (!strcmp(type, "invert")) UN(DF_INVERT);
  else if (!strcmp(type, "cache_2d")) UN(DF_CACHE2D);
  else if (!strcmp(type, "cache_once")) UN(DF_CACHE_ONCE);
  else if (!strcmp(type, "flat_cache")) UN(DF_FLAT);
  else if (!strcmp(type, "interpolated")) UN(DF_INTERP);
  else if (!strcmp(type, "blend_density")) UN(DF_BLEND_DENSITY);
  else if (!strcmp(type, "blend_alpha")) n->kind = DF_BLEND_A;
  else if (!strcmp(type, "blend_offset")) n->kind = DF_BLEND_O;
  else if (!strcmp(type, "shift_a") || !strcmp(type, "shiftA")) {
    n->kind = DF_SHIFT_A;
    const char *nid = jstr(jget(v, "argument"));
    if (!nid) nid = jstr(jget(v, "noise"));
    if (nid) n->noise = gen_noise(g, nid);
  } else if (!strcmp(type, "shift_b") || !strcmp(type, "shiftB")) {
    n->kind = DF_SHIFT_B;
    const char *nid = jstr(jget(v, "noise"));
    if (!nid) nid = jstr(jget(v, "argument"));
    if (nid) n->noise = gen_noise(g, nid);
  } else if (!strcmp(type, "clamp")) {
    n->kind = DF_CLAMP;
    n->a = df_compile(g, jget(v, "input"));
    n->from_v = jnum(jget(v, "min"), -1.0);
    n->to_v = jnum(jget(v, "max"), 1.0);
  } else if (!strcmp(type, "y_clamped_gradient")) {
    n->kind = DF_YCLAMP;
    n->from_y = (int)jnum(jget(v, "from_y"), -64);
    n->to_y = (int)jnum(jget(v, "to_y"), 320);
    n->from_v = jnum(jget(v, "from_value"), 1.0);
    n->to_v = jnum(jget(v, "to_value"), 0.0);
  } else if (!strcmp(type, "noise")) {
    n->kind = DF_NOISE;
    const char *nid = jstr(jget(v, "noise"));
    n->xz_scale = jnum(jget(v, "xz_scale"), 1.0);
    n->y_scale = jnum(jget(v, "y_scale"), 1.0);
    if (nid) n->noise = gen_noise(g, nid);
  } else if (!strcmp(type, "shifted_noise")) {
    n->kind = DF_SHIFTED;
    const char *nid = jstr(jget(v, "noise"));
    n->xz_scale = jnum(jget(v, "xz_scale"), 0.25);
    n->y_scale = jnum(jget(v, "y_scale"), 0.0);
    n->a = df_compile(g, jget(v, "shift_x"));
    n->b = df_compile(g, jget(v, "shift_y"));
    n->c3 = df_compile(g, jget(v, "shift_z"));
    if (nid) n->noise = gen_noise(g, nid);
  } else if (!strcmp(type, "range_choice")) {
    n->kind = DF_RANGE;
    n->a = df_compile(g, jget(v, "input"));
    n->from_v = jnum(jget(v, "min_inclusive"), 0.0);
    n->to_v = jnum(jget(v, "max_exclusive"), 0.0);
    n->b = df_compile(g, jget(v, "when_in_range"));
    n->c3 = df_compile(g, jget(v, "when_out_of_range"));
  } else if (!strcmp(type, "spline")) {
    n->kind = DF_SPLINE;
    n->spline = spline_compile(g, jget(v, "spline"));
  } else if (!strcmp(type, "interval_select")) {
    n->kind = DF_INTERVAL_SELECT;
    n->a = df_compile(g, jget(v, "input"));
    Jv *thresholds = jget(v, "thresholds");
    Jv *fns = jget(v, "functions");
    int nt = jarr_len(thresholds);
    n->nselect = nt;
    n->thresholds = (double *)agrow(&g->arena, sizeof(double) * (size_t)(nt + 1));
    n->fns = (DfNode **)agrow(&g->arena, sizeof(DfNode *) * (size_t)(nt + 1));
    for (int i = 0; i < nt; i++) n->thresholds[i] = jnum(jget_idx(thresholds, i), 0.0);
    for (int i = 0; i <= nt; i++) n->fns[i] = df_compile(g, jget_idx(fns, i));
  } else if (!strcmp(type, "old_blended_noise")) {
    n->kind = DF_OLD_BLEND;
    double xz_scale = jnum(jget(v, "xz_scale"), 0.25);
    double y_scale = jnum(jget(v, "y_scale"), 0.125);
    double xz_factor = jnum(jget(v, "xz_factor"), 80.0);
    double y_factor = jnum(jget(v, "y_factor"), 160.0);
    double smear = jnum(jget(v, "smear_scale_multiplier"), 8.0);
    Xoro terrain;
    xoro_from_hash(g->world_random, "minecraft:terrain", &terrain);
    n->blend = blended_noise_create(&terrain, xz_scale, y_scale, xz_factor, y_factor, smear);
  } else if (!strcmp(type, "find_top_surface")) {
    n->kind = DF_FIND_TOP;
    n->a = df_compile(g, jget(v, "density"));
    n->b = df_compile(g, jget(v, "upper_bound"));
    n->lower_bound = (int)jnum(jget(v, "lower_bound"), MIN_Y);
    n->cell_height = (int)jnum(jget(v, "cell_height"), 8);
  } else {
    fprintf(stderr, "cinder: unsupported density function type %s\n", type);
    abort();
  }
#undef BIN
#undef UN
  return n;
}

/* ------------------------------------------------------------ node walking */

// Register the wrapping nodes that need per-chunk state, then allocate it.
static void df_collect(Gen *g, DfNode *n, unsigned char *seen) {
  if (!n || seen[n->cid]) return;
  seen[n->cid] = 1;
  if (n->kind == DF_INTERP && g->n_interp < MAX_INTERP) g->interp_nodes[g->n_interp++] = n;
  if (n->kind == DF_FLAT && g->n_flat < MAX_FLAT) g->flat_nodes[g->n_flat++] = n;
  if (n->kind == DF_FLAT && g->n_flat >= MAX_FLAT) {
    fprintf(stderr, "cinder: too many flat caches\n");
    abort();
  }
  df_collect(g, n->a, seen);
  df_collect(g, n->b, seen);
  df_collect(g, n->c3, seen);
}

void df_register_roots(Gen *g, DfNode **roots, int nroots) {
  unsigned char seen[MAX_CID];
  memset(seen, 0, sizeof seen);
  for (int i = 0; i < nroots; i++) df_collect(g, roots[i], seen);
}

// Reset the per-chunk state; the interpolation grids are reused across chunks.
// Idempotent for the same chunk: stage code (for example the surface hook used
// by carvers) may call it mid-chunk, and that must not drop the chunk's eval
// state or the aquifer the carver is holding.
void chunk_eval_begin(Gen *g, ChunkEval *ce, int cx, int cz) {
  if (ce->ninterp == 0) {
    for (int i = 0; i < g->n_interp; i++) ce->interp[ce->ninterp++].node = g->interp_nodes[i];
    for (int i = 0; i < g->n_flat; i++) ce->flat[ce->nflat++].node = g->flat_nodes[i];
  }
  if (ce->g == g && ce->cx == cx && ce->cz == cz) return;
  ce->g = g;
  ce->cx = cx;
  ce->cz = cz;
  for (int i = 0; i < ce->ninterp; i++) ce->interp[i].filled = 0;
  for (int i = 0; i < ce->nflat; i++) memset(ce->flat[i].filled, 0, sizeof ce->flat[i].filled);
  memset(ce->c2d_ok, 0, sizeof ce->c2d_ok);
  memset(ce->c1_ok, 0, sizeof ce->c1_ok);
  /* The aquifer belongs to the pipeline (gen_fill_noise creates it, the chunk
   * driver releases it), so it is deliberately not touched here: stage code may
   * reset this context while a carver still holds the aquifer pointer. */
}

void chunk_eval_free(ChunkEval *ce) {
  for (int i = 0; i < ce->ninterp; i++) {
    free(ce->interp[i].grid);
    ce->interp[i].grid = 0;
  }
  if (ce->aquifer) {
    aquifer_free(ce->aquifer);
    ce->aquifer = 0;
  }
  free(ce->prelim.keys);
  free(ce->prelim.vals);
  free(ce->prelim.used);
  memset(&ce->prelim, 0, sizeof ce->prelim);
}
