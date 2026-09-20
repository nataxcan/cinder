// Feature-stage helper implementations shared by every feature family.
//
// Split out of vanilla_placements.c so the family files can call the public
// helpers (block predicates, state providers, nested placed features, block
// geometry queries) without depending on placement-modifier internals.
#ifndef CINDER_INC_VANILLA_FEATURE_COMMON_H
#define CINDER_INC_VANILLA_FEATURE_COMMON_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_common.h"
#endif
#include CINDER_INC_VANILLA_FEATURE_COMMON_H

#ifndef CINDER_INC_VANILLA_BLOCK_TAGS_H
#define CINDER_INC_VANILLA_BLOCK_TAGS_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_block_tags.h"
#endif
#include CINDER_INC_VANILLA_BLOCK_TAGS_H

/* ----------------------------------------------------------- block names -- */

int block_id_for_name(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < B_COUNT; i++) {
    const char *n = block_name(i);
    if (n && !strcmp(n, name)) return i;
  }
  /* tolerate a missing namespace on either side */
  const char *bare = strchr(name, ':');
  bare = bare ? bare + 1 : name;
  for (int i = 0; i < B_COUNT; i++) {
    const char *n = block_name(i);
    if (!n) continue;
    const char *nb = strchr(n, ':');
    nb = nb ? nb + 1 : n;
    if (!strcmp(nb, bare)) return i;
  }
  return -1;
}

int block_tag_contains(const char *tag, uint16_t block) {
  if (!tag) return 0;
  const char *bare = strchr(tag, ':');
  bare = bare ? bare + 1 : tag;
  for (int t = 0; t < BLOCK_TAG_COUNT; t++) {
    const char *name = BLOCK_TAGS[t].tag;
    const char *nb = strchr(name, ':');
    nb = nb ? nb + 1 : name;
    if (strcmp(nb, bare)) continue;
    for (int i = 0; i < BLOCK_TAGS[t].count; i++) {
      if (BLOCK_TAGS[t].ids[i] == (int)block) return 1;
    }
    return 0;
  }
  return 0;
}

/* ------------------------------------------------------- canSurvive rules -- */

// BlockStateBase.canSurvive for the blocks the overworld features place. The
// rules are the ones vanilla's block classes implement (BlockBehaviour's
// default is "true", so only the blocks with real support rules appear here).
/* BlockTags.SUPPORTS_VEGETATION = #substrate_overworld + farmland. */
static int supports_vegetation_below(uint16_t below) {
  return block_tag_contains("minecraft:substrate_overworld", below) || below == B_FARMLAND;
}

int feat_would_survive(FeatureCtx *fc, uint16_t block, int x, int y, int z) {
  uint16_t below = feat_get(fc, x, y - 1, z);
  switch (block) {
    case B_SHORT_GRASS:
    case B_FERN:
    case B_TALL_GRASS:
    case B_LARGE_FERN:
    case B_ALLIUM:
    case B_AZURE_BLUET:
    case B_BLUE_ORCHID:
    case B_CORNFLOWER:
    case B_DANDELION:
    case B_LILY_OF_THE_VALLEY:
    case B_ORANGE_TULIP:
    case B_OXEYE_DAISY:
    case B_PINK_TULIP:
    case B_POPPY:
    case B_RED_TULIP:
    case B_CLOSED_EYEBLOSSOM:
    case B_SUNFLOWER:
    case B_LILAC:
    case B_PEONY:
    case B_ROSE_BUSH:
    case B_BUSH:
    case B_FIREFLY_BUSH:
    case B_SHORT_DRY_GRASS:
    case B_TALL_DRY_GRASS:
    case B_SWEET_BERRY_BUSH:
      /* VegetationBlock.mayPlaceOn: state.is(BlockTags.SUPPORTS_VEGETATION), which
       * is #substrate_overworld plus farmland - not sand or terracotta, which is
       * what the older approximated rule accepted (that let beach and desert
       * grass through). */
      return supports_vegetation_below(below);
    /* SaplingBlock extends VegetationBlock and does not override canSurvive:
     * mayPlaceOn(#supports_vegetation) on the block below, nothing else. An extra
     * water clause here accepted positions vanilla rejects and placed trees in
     * water (about a fifth of all trees in the forest verification area). */
    case B_OAK_SAPLING:
    case B_SPRUCE_SAPLING:
    case B_BIRCH_SAPLING:
    case B_JUNGLE_SAPLING:
    case B_ACACIA_SAPLING:
    case B_DARK_OAK_SAPLING:
    case B_CHERRY_SAPLING:
    case B_PALE_OAK_SAPLING:
      return supports_vegetation_below(below);
    case B_DEAD_BUSH:
      return below == B_SAND || below == B_RED_SAND || below == B_TERRACOTTA ||
             below == B_DIRT || below == B_GRASS || below == B_PODZOL ||
             below == B_COARSE_DIRT || below == B_MUD || below == B_MOSS ||
             below == B_CLAY || below == B_GRAVEL || below == B_SNOW_BLOCK ||
             below == B_SNOW;
    case B_SNOW:
      return blocks_motion(below) || below == B_SNOW || below == B_ICE ||
             below == B_PACKED_ICE || below == B_POWDER_SNOW;
    case B_CACTUS:
      return below == B_SAND || below == B_RED_SAND || below == B_CACTUS;
    case B_SUGAR_CANE: {
      if (below == B_SUGAR_CANE) return 1;
      uint16_t at = feat_get(fc, x, y, z);
      (void)at;
      return below == B_GRASS || below == B_DIRT || below == B_COARSE_DIRT ||
             below == B_PODZOL || below == B_SAND || below == B_RED_SAND ||
             below == B_MUD || below == B_MOSS || below == B_SNOW_BLOCK ||
             below == B_MYCELIUM || below == B_ROOTED_DIRT || below == B_CLAY ||
             below == B_GRAVEL;
    }
    case B_LILY:
      return below == B_WATER || below == B_ICE;
    case B_SEAGRASS:
    case B_KELP:
      return below == B_WATER || below == B_SEAGRASS || below == B_KELP ||
             below == B_SAND || below == B_DIRT || below == B_GRAVEL ||
             below == B_CLAY || below == B_MUD || below == B_MOSS ||
             below == B_STONE || below == B_DEEPSLATE;
    case B_GLOW_LICHEN:
    case B_MOSS_CARPET:
    case B_HANGING_MOSS:
    case B_CAVE_VINES:
    case B_CAVE_VINES_PLANT:
      return 1; /* multiface/carpet/vine blocks have no support requirement here */
    case B_LEAF_LITTER:
      return below == B_GRASS || below == B_DIRT || below == B_PODZOL ||
             below == B_MYCELIUM || below == B_MUD || below == B_ROOTED_DIRT ||
             below == B_COARSE_DIRT || below == B_MOSS || below == B_PALE_MOSS_BLOCK ||
             below == B_SNOW_BLOCK || below == B_CLAY || below == B_GRAVEL ||
             below == B_SAND || below == B_RED_SAND || below == B_TERRACOTTA;
    case B_POINTED_DRIPSTONE:
      return below == B_DRIPSTONE || below == B_POINTED_DRIPSTONE || below == B_STONE ||
             below == B_DEEPSLATE || below == B_TUFF || below == B_CALCITE;
    case B_AMETHYST_CLUSTER:
    case B_LARGE_AMETHYST_BUD:
    case B_MEDIUM_AMETHYST_BUD:
    case B_SMALL_AMETHYST_BUD:
      return below == B_BUDDING_AMETHYST || below == B_AMETHYST;
    default:
      return 1;
  }
}

/* -------------------------------------------------------- state providers -- */

// BlockStateProvider.getOptionalState (the overworld's provider types).
int feat_state_provider(FeatureCtx *fc, Jv *sp, int x, int y, int z, uint16_t *out) {
  if (!sp) return 0;
  const char *type = jstr(jget(sp, "type"));
  if (!type) return 0;
  if (!strncmp(type, "minecraft:", 10)) type += 10;
  if (!strcmp(type, "simple_state_provider") || !strcmp(type, "rotated_block_provider")) {
    Jv *state = jget(sp, "state");
    const char *name = state ? (state->kind == J_STR ? state->str : jstr(jget(state, "Name"))) : 0;
    int id = block_id_for_name(name);
    if (id < 0) return 0;
    *out = (uint16_t)id;
    return 1;
  }
  if (!strcmp(type, "weighted_state_provider")) {
    Jv *entries = jget(sp, "entries");
    int total = 0;
    for (Jv *e = entries ? entries->child : 0; e; e = e->next) total += (int)jnum(jget(e, "weight"), 1);
    if (total <= 0) return 0;
    int pick = legacy_next_bound(feat_rng(fc), total);
    for (Jv *e = entries ? entries->child : 0; e; e = e->next) {
      int w = (int)jnum(jget(e, "weight"), 1);
      if (pick < w) {
        Jv *data = jget(e, "data");
        const char *name = data ? (data->kind == J_STR ? data->str : jstr(jget(data, "Name"))) : 0;
        int id = block_id_for_name(name);
        if (id < 0) return 0;
        *out = (uint16_t)id;
        return 1;
      }
      pick -= w;
    }
    return 0;
  }
  if (!strcmp(type, "randomized_int_state_provider")) {
    /* RandomizedIntStateProvider.getState: draws the property value first (which
     * consumes RNG), then returns the source state with that property applied.
     * Cinder tracks the base block only, so the draw is consumed and the base
     * block returned - the RNG stream stays vanilla-identical. */
    Jv *values = jget(sp, "values");
    if (values) (void)feat_int_provider(values, feat_rng(fc));
    Jv *source = jget(sp, "source");
    return feat_state_provider(fc, source, x, y, z, out);
  }
  if (!strcmp(type, "rule_based_state_provider")) {
    Jv *rules = jget(sp, "rules");
    for (Jv *r = rules ? rules->child : 0; r; r = r->next) {
      Jv *if_true = jget(r, "if_true");
      if (if_true && !feat_block_predicate(fc, if_true, x, y, z)) continue;
      Jv *then = jget(r, "then");
      return feat_state_provider(fc, then, x, y, z, out);
    }
    Jv *fallback = jget(sp, "fallback");
    if (fallback) return feat_state_provider(fc, fallback, x, y, z, out);
    return 0;
  }
  return 0;
}
