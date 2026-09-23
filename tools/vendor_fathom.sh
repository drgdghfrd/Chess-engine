#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/third_party/Fathom"
REPO="https://github.com/jdart1/Fathom.git"
COMMIT="c6cf6e8f2f4275e91e03c3612b8d17fe64253c5e"

if [[ -f "$DEST/SOURCE.lock" && -f "$DEST/src/tbprobe.c" && -f "$DEST/src/tbprobe.h" ]]; then
  echo "Fathom source already present; verifying: $DEST"
  "$ROOT/tools/verify_fathom_vendor.sh"
  exit 0
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

echo "Cloning Fathom $COMMIT ..."
git clone --filter=blob:none --no-checkout "$REPO" "$tmp/Fathom"
git -C "$tmp/Fathom" fetch --depth 1 origin "$COMMIT"
git -C "$tmp/Fathom" checkout --detach "$COMMIT"

rm -rf "$DEST/src"
mkdir -p "$DEST"
cp -a "$tmp/Fathom/src" "$DEST/src"
cp -f "$tmp/Fathom/LICENSE" "$DEST/LICENSE"
cp -f "$tmp/Fathom/LICENSE" "$DEST/LICENSE.txt"

for f in tbprobe.c tbprobe.h tbconfig.h tbchess.c stdendian.h; do
  test -f "$DEST/src/$f" || { echo "missing vendored source: $f" >&2; exit 3; }
done

echo "Vendored Fathom at $DEST"
"$ROOT/tools/verify_fathom_vendor.sh"
