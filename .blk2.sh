#!/bin/bash
B=/mnt/c/Users/nataxcan/Documents/dev/mc-src-26.2/net/minecraft/world/level/block
cd "$B" || exit 1
for f in GrowingPlantBlock FireBlock BaseFireBlock PointedDripstoneBlock SpeleothemBlock; do
  echo "===== $f"
  [ -f "$f.java" ] || { echo "(missing)"; continue; }
  awk '/protected boolean canSurvive|public boolean canSurvive|protected boolean mayPlaceOn|protected boolean isFaceSturdy/,/^   }/' "$f.java" | head -40
done
