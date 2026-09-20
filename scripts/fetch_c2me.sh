#!/usr/bin/env bash
set -euo pipefail
DIR=/home/nataxcan/dev/c2me-run
mkdir -p "$DIR/mods"
cd "$DIR"

# Fabric dedicated-server launcher (pulls vanilla 26.2 on first boot).
curl -fL -A "cinder-bench/1.0" \
  -o fabric-server.jar \
  "https://meta.fabricmc.net/v2/versions/loader/26.2/0.19.5/1.1.2/server/jar"

curl -fL -A "cinder-bench/1.0" \
  -o mods/c2me-fabric-mc26.2-0.4.2-alpha.0.52.jar \
  "https://cdn.modrinth.com/data/VSNURh3q/versions/LmKTn6Yc/c2me-fabric-mc26.2-0.4.2-alpha.0.52.jar"

curl -fL -A "cinder-bench/1.0" \
  -o "mods/fabric-api-0.161.0+26.2.jar" \
  "https://cdn.modrinth.com/data/P7dR8mSH/versions/ewUK83HI/fabric-api-0.161.0%2B26.2.jar"

# C2ME README: pair with Lithium. Folia already ships Paper equivalents.
curl -fL -A "cinder-bench/1.0" \
  -o "mods/lithium-fabric-0.25.3+mc26.2.jar" \
  "https://cdn.modrinth.com/data/gvQqBUqZ/versions/f7vZ0VWU/lithium-fabric-0.25.3%2Bmc26.2.jar"

ls -lh fabric-server.jar mods/
echo eula=true > eula.txt
