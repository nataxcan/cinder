import pathlib

p = pathlib.Path('src/vanilla_placements.c')
s = p.read_text()

start = s.index('static int block_predicate_test(FeatureCtx *fc, Jv *p, int x, int y, int z) {')
end = s.index('/* ----------------------------------------------------- placement modifiers */')

new = r'''int feat_block_predicate(FeatureCtx *fc, Jv *p, int x, int y, int z) {
  if (!p) return 1;
  const char *type = jstr(jget(p, "type"));
  if (!type) return 1;
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  if (!strcmp(type, "true")) return 1;
  if (!strcmp(type, "all_of")) {
    Jv *list = jget(p, "predicates");
    for (Jv *c = list ? list->child : 0; c; c = c->next) {
      if (!feat_block_predicate(fc, c, x, y, z)) return 0;
    }
    return 1;
  }
  if (!strcmp(type, "any_of")) {
    Jv *list = jget(p, "predicates");
    for (Jv *c = list ? list->child : 0; c; c = c->next) {
      if (feat_block_predicate(fc, c, x, y, z)) return 1;
    }
    return 0;
  }
  if (!strcmp(type, "not")) return !feat_block_predicate(fc, jget(p, "predicate"), x, y, z);
  if (!strcmp(type, "inside_world_bounds")) return 1;
  if (!strcmp(type, "unobstructed")) return 1;

  /* StateTestingPredicate subclasses apply "offset" (Vec3i.offsetCodec, default
   * [0,0,0]) before reading the state - e.g. cf:disk_grass's {"type":"solid",
   * "offset":[0,1,0]} tests the block ABOVE the origin. */
  int ox = 0, oy = 0, oz = 0;
  Jv *offset = jget(p, "offset");
  if (offset && offset->kind == J_ARR) {
    ox = (int)jnum(jget_idx(offset, 0), 0);
    oy = (int)jnum(jget_idx(offset, 1), 0);
    oz = (int)jnum(jget_idx(offset, 2), 0);
  }
  int px = x + ox, py = y + oy, pz = z + oz;

  /* solid: BlockStateBase.isSolid() (legacySolid). Over Cinder's palette the
   * only blocks where it differs from "collision shape is a full cube" are the
   * sandstone slab and the snow layer, and both are already non-full. */
  if (!strcmp(type, "solid")) return feat_is_collision_full(feat_get(fc, px, py, pz));
  if (!strcmp(type, "replaceable")) return feat_can_replace(feat_get(fc, px, py, pz));
  if (!strcmp(type, "matching_blocks")) {
    uint16_t here = feat_get(fc, px, py, pz);
    Jv *list = jget(p, "blocks");
    for (Jv *c = list ? list->child : 0; c; c = c->next) {
      const char *name = c->kind == J_STR ? c->str : jstr(jget(c, "Name"));
      int id = block_id_for_name(name);
      if (id >= 0 && id == here) return 1;
    }
    return 0;
  }
  if (!strcmp(type, "matching_block_tag")) {
    const char *tag = jstr(jget(p, "tag"));
    return tag && block_tag_contains(tag, feat_get(fc, px, py, pz));
  }
  if (!strcmp(type, "matching_fluids")) {
    uint16_t here = feat_get(fc, px, py, pz);
    Jv *list = jget(p, "fluids");
    for (Jv *c = list ? list->child : 0; c; c = c->next) {
      const char *name = c->kind == J_STR ? c->str : jstr(jget(c, "Name"));
      int id = block_id_for_name(name);
      if (id >= 0 && id == here) return 1;
    }
    return 0;
  }
  if (!strcmp(type, "would_survive")) {
    Jv *state = jget(p, "state");
    const char *name = state ? (state->kind == J_STR ? state->str : jstr(jget(state, "Name"))) : 0;
    int id = block_id_for_name(name);
    return id >= 0 && feat_would_survive(fc, (uint16_t)id, px, py, pz);
  }
  return 1;
}

'''
s = s[:start] + new + s[end:]
s = s.replace('block_predicate_test(fc, ', 'feat_block_predicate(fc, ')
p.write_text(s)
print('predicate offsets applied;', s.count('feat_block_predicate'), 'call sites')
