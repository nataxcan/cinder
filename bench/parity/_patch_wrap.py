import pathlib

p = pathlib.Path('src/vanilla_placements.c')
s = p.read_text()

addition = r'''
/* ------------------------------------------------ public helper wrappers -- */

int feat_int_provider(Jv *p, LegacyRng *r) { return int_provider_sample(p, r, MIN_Y, HEIGHT); }

/* FloatProvider.sample (uniform / trapezoid / clamped_normal / constant) */
double feat_float_provider(Jv *p, LegacyRng *r) {
  if (!p) return 0.0;
  if (p->kind == J_NUM) return p->num;
  const char *type = jstr(jget(p, "type"));
  if (!type) return jnum(p, 0.0);
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  if (!strcmp(type, "constant")) return jnum(jget(p, "value"), 0.0);
  if (!strcmp(type, "uniform")) {
    float lo = (float)jnum(jget(p, "min_inclusive"), 0.0);
    float hi = (float)jnum(jget(p, "max_exclusive"), 1.0);
    return (double)(lo + legacy_next_float(r) * (hi - lo));
  }
  if (!strcmp(type, "trapezoid")) {
    float lo = (float)jnum(jget(p, "min"), 0.0);
    float hi = (float)jnum(jget(p, "max"), 1.0);
    float plateau = (float)jnum(jget(p, "plateau"), 0.0);
    float range = hi - lo;
    if (range <= 0.0f) return (double)lo;
    if (plateau >= range) return (double)(lo + legacy_next_float(r) * range);
    float plateau_start = (range - plateau) / 2.0f;
    float plateau_end = range - plateau_start;
    return (double)(lo + legacy_next_float(r) * plateau_end + legacy_next_float(r) * plateau_start);
  }
  if (!strcmp(type, "clamped_normal")) {
    float mean = (float)jnum(jget(p, "mean"), 0.0);
    float dev = (float)jnum(jget(p, "deviation"), 1.0);
    float lo = (float)jnum(jget(p, "min"), 0.0);
    float hi = (float)jnum(jget(p, "max"), 1.0);
    float normal = mean + (float)legacy_next_gaussian(r) * dev;
    if (normal < lo) normal = lo;
    if (normal > hi) normal = hi;
    return (double)normal;
  }
  return jnum(p, 0.0);
}

/* PlacedFeature.place(level, generator, random, pos) for a nested placed
 * feature: `placed_json` is {"feature": <inline or cf: name>, "placement": []}. */
int feat_apply_placed(FeatureCtx *fc, Jv *placed_json, int x, int y, int z) {
  if (!placed_json) return 0;
  Jv *saved = fc->placed;
  int placed_any = 0;
  fc->placed = placed_json;
  Jv *placement = jget(placed_json, "placement");
  /* the emit callback (defined in vanilla_features.c) applies the configured
   * feature and records success through fc->nested_placed_any */
  fc->nested_placed_any = 0;
  place_chain(fc, placement ? placement->child : 0, x, y, z, feat_nested_emit);
  placed_any = fc->nested_placed_any;
  fc->placed = saved;
  return placed_any;
}
'''

marker = '/* ----------------------------------------------------- placement modifiers */'
assert marker in s
s = s.replace(marker, addition + '\n' + marker, 1)
p.write_text(s)
print('wrappers added')
