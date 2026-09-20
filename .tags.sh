#!/bin/bash
set -e
D=/mnt/c/Users/nataxcan/Documents/dev/cinder/.tags
rm -rf "$D"; mkdir -p "$D"
cd "$D"
unzip -o -q /home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar 'data/minecraft/tags/block/*' >/dev/null
cd data/minecraft/tags/block
for t in prevents_nearby_leaf_decay air dirt sand supports_bamboo beneath_bamboo_podzol_replaceable cannot_support_snow_layer support_override_snow_layer logs leaves replaceable_by_trees cannot_replace_below_tree_trunk azalea_grows_on azalea_root_replaceable mangrove_logs_can_grow_through mangrove_roots_can_grow_through moss_replaceable lush_ground_replaceable dripstone_replaceable_blocks beneath_tree_podzol_replaceable sulfur_spike_replaceable_blocks supports_mangrove_propagule; do
  printf '%s: ' "$t"
  tr -d '\n ' < "$t.json" 2>/dev/null | sed 's/.*"values":\[//;s/\]}//'
  echo
done
