#!/bin/bash
cd /mnt/c/Users/nataxcan/Documents/dev/cinder/.tags/data/minecraft/tags/block || exit 1
for t in supports_vegetation supports_dry_vegetation supports_cactus supports_sugar_cane supports_big_dripleaf supports_small_dripleaf overrides_mushroom_light_requirement substrate_overworld supports_sugar_cane_adjacently; do
  printf '%s: ' "$t"
  tr -d '\n ' < "$t.json" 2>/dev/null
  echo
done
