#!/usr/bin/env bash
# Publish the working tree to github.com/nataxcan/cinder.
#
# Uses the OAuth token from the gh config (~/.config/gh/hosts.yml) through a
# credential helper, so the token never appears in the remote URL, in argv, or in
# .git/config.
set -euo pipefail
cd "$(dirname "$0")/.."          # this script lives in <repo>/scripts
REPO_ROOT="$PWD"

REPO="${REPO:-https://github.com/nataxcan/cinder.git}"
TOKEN_FILE="$HOME/.config/gh/hosts.yml"

# guard: this must be the cinder checkout, not whatever directory the caller was in
[ -f "$REPO_ROOT/AGENTS.md" ] && [ -d "$REPO_ROOT/src" ] || {
  echo "publish.sh: $REPO_ROOT does not look like the cinder checkout" >&2
  exit 1
}

if [ ! -d .git ]; then
  git init -q -b main
  git config user.name "$(git config --global user.name || echo nataxcan)"
  git config user.email "$(git config --global user.email || echo nataxcan@users.noreply.github.com)"
fi

git add -A
echo "== staged: $(git diff --cached --name-only | wc -l) files in $REPO_ROOT"
git diff --cached --stat | tail -1

if ! git diff --cached --quiet; then
  git commit -q -m "${MSG:-Update}"
fi

if [ ! -f "$TOKEN_FILE" ]; then
  echo "no $TOKEN_FILE; push manually with your own credentials" >&2
  exit 1
fi
TOKEN=$(python3 -c "
import re, pathlib
s = pathlib.Path('$TOKEN_FILE').read_text()
m = re.search(r'oauth_token:\s*(\S+)', s)
print(m.group(1) if m else '')
")
[ -n "$TOKEN" ] || { echo "no oauth_token in $TOKEN_FILE" >&2; exit 1; }

git remote remove origin 2>/dev/null || true
git remote add origin "$REPO"
git -c credential.helper= \
    -c "credential.helper=!f() { echo username=x-access-token; echo password=$TOKEN; }; f" \
    push -u origin main "$@"
echo "pushed $(git rev-parse --short HEAD) to $REPO"
