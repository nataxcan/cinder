#!/bin/bash
B=/mnt/c/Users/nataxcan/Documents/dev/mc-src-26.2/net/minecraft/world/level/block
cd "$B" || exit 1
for f in CocoaBlock HangingMossBlock MangrovePropaguleBlock MangroveRootsBlock MossyCarpetBlock GrowingPlantHeadBlock GrowingPlantBodyBlock KelpBlock CaveVinesBlock CaveVinesPlantBlock BigDripleafStemBlock; do
  echo "===== $f"
  [ -f "$f.java" ] || { echo "(missing)"; continue; }
  grep -n 'class .* extends' "$f.java" | head -2
  awk '/protected boolean canSurvive|public boolean canSurvive|protected BlockState updateShape|protected boolean mayPlaceOn|protected boolean isValidBonemealTarget/,/^   }/' "$f.java" | head -50
done
