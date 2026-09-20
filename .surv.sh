#!/bin/bash
B=/mnt/c/Users/nataxcan/Documents/dev/mc-src-26.2/net/minecraft/world/level/block
cd "$B" || exit 1
for f in BushBlock VegetationBlock ShortPlantBlock FlowerBlock TallFlowerBlock WaterlilyBlock MushroomBlock CactusBlock SugarCaneBlock KelpBlock SeaPickleBlock BambooStalkBlock MultifaceBlock HangingRootsBlock LeafLitterBlock SweetBerryBushBlock SporeBlossomBlock PinkPetalsBlock DryVegetationBlock FireflyBushBlock SnowLayerBlock MossyCarpetBlock CaveVinesBlock WeepingVinesBlock VineBlock MelonBlock AttachedStemBlock; do
  if [ -f "$f.java" ]; then
    echo "===== $f"
    grep -n 'class .* extends' "$f.java" | head -2
    awk '/protected boolean mayPlaceOn|protected boolean canSurvive|public boolean canSurvive/,/^   }/' "$f.java" | head -30
  fi
done
