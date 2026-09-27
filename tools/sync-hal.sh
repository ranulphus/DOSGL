#!/usr/bin/env bash
# Refresh third_party/mgahal from MGA-Glide (plan decision V1): export the
# shared HAL, harness, bench tooling and 86Box patches with MGA-Glide's
# tools/hal-export.sh and replace the vendored copy. The vendored tree is
# never edited in place; HAL changes are made in MGA-Glide (V2).
#
#   tools/sync-hal.sh [MGA_GLIDE_DIR]     (default ~/MGA-Glide)
#   tools/sync-hal.sh --check             verify the vendored copy against its MANIFEST
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
dest=$root/third_party/mgahal
if [ "${1:-}" = --check ]; then
  (cd "$dest" && sha256sum --quiet -c MANIFEST) || { echo "sync-hal: third_party/mgahal differs from its MANIFEST" >&2; exit 1; }
  # build/ and out/ are generated there (make dostools, setup); anything else is an edit.
  extra=$(cd "$dest" && comm -13 <(awk '{print $2}' MANIFEST | sort) \
          <(find . -type f ! -name MANIFEST ! -path './build/*' ! -path './out/*' ! -path './dist/*' | sort))
  [ -z "$extra" ] || { echo "sync-hal: files not in MANIFEST:" >&2; echo "$extra" >&2; exit 1; }
  echo "sync-hal: third_party/mgahal matches MANIFEST ($(cat "$dest/VERSION"))"
  exit 0
fi
src=${1:-$HOME/MGA-Glide}
if [ -n "$(git -C "$src" status --porcelain -- hal tests tools docs 2>/dev/null)" ]; then
  echo "sync-hal: $src has uncommitted changes; commit them in MGA-Glide first" >&2
  exit 1
fi
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
"$src/tools/hal-export.sh" "$tmp/mgahal" >/dev/null
rm -rf "$dest"
mkdir -p "$(dirname "$dest")"
cp -a "$tmp/mgahal" "$dest"
echo "sync-hal: third_party/mgahal = MGA-Glide $(cat "$dest/VERSION")"
