#!/bin/bash
B=/mnt/c/Users/nataxcan/Documents/dev/mc-src-26.2/net/minecraft/world/level/block
cd "$B" || exit 1
for f in VegetationBlock TallGrassBlock ShortPlantBlock FlowerBlock DoublePlantBlock DryVegetationBlock LeafLitterBlock MushroomBlock WaterlilyBlock SeagrassBlock TallSeagrassBlock SeaPickleBlock BambooStalkBlock KelpBlock GrowingPlantHeadBlock CactusBlock SugarCaneBlock SporeBlossomBlock SweetBerryBushBlock BushBlock FireflyBushBlock MelonBlock PumpkinBlock CocoaBlock HangingRootsBlock MossyCarpetBlock MultifaceBlock VineBlock SnowLayerBlock CaveVinesBlock SmallDripleafBlock BigDripleafBlock BigDripleafStemBlock PointedDripstoneBlock GlowLichenBlock SculkVeinBlock MangrovePropaguleBlock MangroveRootsBlock SaplingBlock; do
  echo "===== $f"
  [ -f "$f.java" ] || { echo "(missing)"; continue; }
  grep -n '^public class\|^public abstract class' "$f.java"
  grep -n 'mayPlaceOn\|protected boolean canSurvive\|public boolean canSurvive' "$f.java"
done
