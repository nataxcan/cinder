#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="${HOME}/.bend/bin:${PATH}"

echo "== proofs =="
bend PROOF.bend

echo "== tests =="
bend tests/codec.bend
bend tests/world.bend
bend tests/proto.bend
bend tests/status.bend

echo "== native server =="
if [ ! -f src/vanilla/embed.h ]; then
  echo "extract vanilla 26.2 worldgen JSON first: python3 scripts/extract_vanilla_worldgen.py"
  exit 1
fi
bend src/main.bend -o cinder

echo "== worldgen bench =="
bend src/bench.bend -o cinder-bench

echo "built ./cinder and ./cinder-bench"
echo "run: ./cinder --threads 8"
echo "ping: python3 scripts/ping.py 127.0.0.1 25565"
