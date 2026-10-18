#!/bin/bash
# publish-art.sh — makes a folder of HD pictures (assets/hd by default) the set every game
# fetches. Only the pictures the server does not have yet are uploaded; then the set is
# published as the next version. Going back is running it on the older pictures (from an
# older checkout): they are all still there, so only the set is written.
#
#   ENCORE_API      the Worker, e.g. https://thebestpinball.com
#   PUBLISH_TOKEN   the Worker's secret of the same name
#   ART_FORMAT      optional, kArtFormat in src/game/Art.h if not given: what a game must
#                   understand to use these pictures; older games keep the last set they can use
#   publish-art.sh [folder]
set -euo pipefail

: "${ENCORE_API:?}" "${PUBLISH_TOKEN:?}"
dir="${1:-$(dirname "$0")/../assets/hd}"
shopt -s nullglob
pictures=("$dir"/*.png)
[ ${#pictures[@]} -gt 0 ] || { echo "no pictures in $dir" >&2; exit 1; }

files="[]"
uploaded=0
for f in "${pictures[@]}"; do
  name=$(basename "$f")
  hash=$(shasum -a 256 "$f" | cut -d' ' -f1)
  size=$(wc -c < "$f" | tr -d ' ')
  if ! curl -fsS -o /dev/null -I "$ENCORE_API/v1/art/$hash.png" 2>/dev/null; then
    echo "uploading $name ($size bytes)"
    curl -fsS -X PUT -H "Authorization: Bearer $PUBLISH_TOKEN" -H "Content-Type: image/png" \
      --data-binary @"$f" "$ENCORE_API/v1/publish/art/$hash" > /dev/null
    uploaded=$((uploaded + 1))
  fi
  files=$(jq -c --arg n "$name" --arg h "$hash" --argjson s "$size" '. + [{name: $n, sha256: $h, size: $s}]' <<< "$files")
done
echo "$uploaded of ${#pictures[@]} pictures uploaded, the rest were there already"

format="${ART_FORMAT:-$(sed -n 's/^constexpr int kArtFormat = \([0-9]*\);.*/\1/p' "$(dirname "$0")/../src/game/Art.h")}"
[ -n "$format" ] || { echo "no ART_FORMAT, and none found in src/game/Art.h" >&2; exit 1; }
jq -c --argjson f "$format" '{format: $f, files: .}' <<< "$files" |
  curl -fsS -X PUT -H "Authorization: Bearer $PUBLISH_TOKEN" -H "Content-Type: application/json" \
    --data-binary @- "$ENCORE_API/v1/publish/art" |
  jq -r 'if .unchanged then "nothing changed: still version \(.version)"
          else "published version \(.version) (format \(.format)): \(.files) pictures, \(.bytes / 1048576 | floor) MB" end'
