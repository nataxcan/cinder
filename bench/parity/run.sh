#!/usr/bin/env bash
# Dump the vanilla overworld multi-noise parameter list to biome_params.json.
#
# Needs the deobfuscated vanilla 26.2 server jar plus its libraries (the jar is
# taken from the Fabric run dir used by the other benches).
set -euo pipefail

JAR="${JAR:-/home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar}"
LIBS="${LIBS:-/home/nataxcan/dev/c2me-run/libraries}"
JAVA="${JAVA:-$HOME/jdk-25/bin/java}"
JAVAC="${JAVAC:-$HOME/jdk-25/bin/javac}"

here="$(cd "$(dirname "$0")" && pwd)"
out="$here/biome_params.json"
work="$(mktemp -d)"

CP="$(find "$LIBS" -name '*.jar' -print | paste -sd: -):$JAR"
"$JAVAC" -nowarn -cp "$CP" -d "$work" "$here/biome_params.java"
"$JAVA" -cp "$work:$CP" biome_params > "$out"
rm -rf "$work"

python3 - "$out" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
print(f"{sys.argv[1]}: {d['count']} parameter points")
biomes = sorted({e["biome"].split(":")[1] for e in d["entries"]})
print(f"distinct biomes: {len(biomes)}")
PY
