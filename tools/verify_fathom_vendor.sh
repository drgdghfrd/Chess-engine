#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${CHESSZERO_FATHOM_SOURCE_DIR:-$ROOT/third_party/Fathom}"
LOCK="$DEST/SOURCE.lock"

if [[ ! -f "$LOCK" ]]; then
  echo "error: Fathom SOURCE.lock not found: $LOCK" >&2
  exit 2
fi

# SOURCE.lock records Git blob SHA-1s (not SHA-256 file digests), matching the
# upstream Git objects at the pinned Fathom commit.
git_blob_sha1() {
  local file="$1"
  if command -v git >/dev/null 2>&1; then
    git hash-object -- "$file"
    return
  fi
  python3 - "$file" <<'PY'
import hashlib, pathlib, sys
p = pathlib.Path(sys.argv[1])
data = p.read_bytes()
h = hashlib.sha1()
h.update(f"blob {len(data)}\0".encode())
h.update(data)
print(h.hexdigest())
PY
}

fail=0
while read -r path sha size; do
  [[ -z "${path:-}" || "${path:0:1}" == "#" ]] && continue
  file="$DEST/$path"
  if [[ ! -f "$file" ]]; then
    echo "missing: $path" >&2
    fail=1
    continue
  fi
  actual_size="$(wc -c < "$file" | tr -d ' ')"
  actual_sha="$(git_blob_sha1 "$file")"
  [[ "$actual_size" == "$size" ]] || { echo "size mismatch: $path expected=$size actual=$actual_size" >&2; fail=1; }
  [[ "$actual_sha" == "$sha" ]] || { echo "git blob SHA-1 mismatch: $path expected=$sha actual=$actual_sha" >&2; fail=1; }
done < "$LOCK"

if (( fail )); then
  exit 3
fi
echo "Fathom vendored-source verification: PASS"
