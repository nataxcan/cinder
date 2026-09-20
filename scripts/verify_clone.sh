#!/usr/bin/env bash
# Clone the published repo into a scratch dir and check it stands on its own:
# relocate the includes, run the checker tests, and compile the worldgen TU.
set -uo pipefail
export PATH="$HOME/.bend/bin:$PATH"
WORK="${CLONE_DIR:-/tmp/cinder-clone}"

rm -rf "$WORK"
git clone -q "${REPO_URL:-https://github.com/nataxcan/cinder.git}" "$WORK" || exit 1
cd "$WORK" || exit 1
echo "== cloned $(git log --oneline -1)"

echo "== files"
git ls-files | wc -l | sed 's/^/   tracked: /'

echo "== relocate includes"
python3 scripts/relocate.py | tail -2

echo "== checker tests"
for t in tests/codec.bend tests/world.bend tests/proto.bend tests/status.bend; do
  [ -f "$t" ] || { echo "   $t missing"; continue; }
  printf '   %-22s ' "$t"
  bend "$t" 2>&1 | tail -1
done

echo "== proofs"
bend PROOF.bend 2>&1 | tail -1

echo "== worldgen C compiles"
bash scripts/check_c.sh 2>&1 | grep -c '^ok:' | sed 's/^/   modules ok: /'
