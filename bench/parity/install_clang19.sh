#!/usr/bin/env bash
# Install a clang >= 19 for bend's GPU target without root.
#
# The official LLVM release page no longer ships an x86_64 Linux tarball, and
# apt.llvm.org wants sudo, so fetch the .debs and unpack them under $PREFIX with
# dpkg -x. Bend honours $CC, so nothing system-wide changes.
#
#   bench/parity/install_clang19.sh [prefix]        # default $HOME/.cache/llvm19
set -euo pipefail

PREFIX="${1:-$HOME/.cache/llvm19}"
SUITE="${SUITE:-jammy}"
VER="${VER:-19}"
BASE="https://apt.llvm.org/$SUITE"
DIST="llvm-toolchain-$SUITE-$VER"

PACKAGES="clang-$VER libclang-cpp$VER libllvm$VER libclang-common-$VER-dev llvm-$VER-linker-tools"

mkdir -p "$PREFIX/debs"
echo "== fetching the package index ($DIST)"
idx="$PREFIX/debs/Packages"
curl -fsSL "$BASE/dists/$DIST/main/binary-amd64/Packages" -o "$idx"
echo "   $(grep -c '^Package: ' "$idx") packages"

for pkg in $PACKAGES; do
  # take the Filename: of the first stanza whose Package: is exactly $pkg
  file=$(awk -v want="$pkg" '
    /^Package: /{cur=$2}
    /^Filename: /{if (cur==want) {print $2; exit}}
  ' "$idx")
  if [ -z "$file" ]; then
    echo "   !! $pkg not found in the index"
    continue
  fi
  out="$PREFIX/debs/$(basename "$file")"
  if [ ! -f "$out" ]; then
    echo "== downloading $pkg"
    curl -fsSL "$BASE/$file" -o "$out"
  fi
  echo "== unpacking $(basename "$out")"
  dpkg -x "$out" "$PREFIX"
done

CLANG="$PREFIX/usr/lib/llvm-$VER/bin/clang"
if [ ! -x "$CLANG" ]; then
  CLANG="$PREFIX/usr/bin/clang-$VER"
fi
echo
echo "clang: $CLANG"
if [ -x "$CLANG" ]; then
  LD_LIBRARY_PATH="$PREFIX/usr/lib/x86_64-linux-gnu:$PREFIX/usr/lib/llvm-$VER/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$CLANG" --version | head -2
  echo
  echo "use it with:  CC=$CLANG bend <file.bend> -o <out>"
fi
