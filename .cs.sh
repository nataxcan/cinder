#!/bin/bash
cd /mnt/c/Users/nataxcan/Documents/dev/mc-src-26.2/net/minecraft/world/level/block || exit 1
for f in DoublePlantBlock TallSeagrassBlock SeagrassBlock SeaPickleBlock BambooStalkBlock WaterlilyBlock HangingRootsBlock SmallDripleafBlock BigDripleafBlock BushBlock MangrovePropaguleBlock CaveVinesBlock GlowLichenBlock; do
  echo "===== $f"
  awk '/canSurvive|mayPlaceOn|isValidBonemealTarget|protected boolean canSurvive/,/^   }/' "$f.java" 2>/dev/null | head -40
done
