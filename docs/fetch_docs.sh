#!/usr/bin/env bash
# Build a local, greppable copy of the reference docs for the XIAO MG24 + Hubble
# work under docs/local/. Nothing it writes is committed (see .gitignore): the
# PDFs and upstream docs are large and regenerable, so only this script is
# tracked. Re-run it to refresh.
#
#   - every *.pdf in the repo  -> docs/local/datasheets/<name>.txt (pdftotext -layout)
#   - Hubble Device SDK        -> docs/local/hubble-device-sdk/<ref>/ (docs/*.rst,
#                                 include/ headers, port/, samples/)
#   - hubble.com/docs          -> docs/local/hubble.com/*.txt (the site's own
#                                 llms.txt plain-text bundles)
#
# Requires git, curl, pdftotext (poppler).
set -euo pipefail

REPO="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
OUT="$REPO/docs/local"

# release/3.1-branch matches the published docs we target; main carries the
# Silicon Labs (sisdk) port and quickstart, which 3.1 predates and the MG24 needs.
SDK_REFS=(${HUBBLE_SDK_REFS:-release/3.1-branch main})
SDK_URL=https://github.com/HubbleNetwork/hubble-device-sdk.git
HUBBLE_DOCS=https://hubble.com/docs

for tool in git curl pdftotext; do
  command -v "$tool" >/dev/null || { echo "missing: $tool" >&2; exit 1; }
done

mkdir -p "$OUT/datasheets" "$OUT/hubble-device-sdk" "$OUT/hubble.com"

echo "== datasheets"
find "$REPO" -iname '*.pdf' -not -path "$OUT/*" -not -path "$REPO/.git/*" -print0 |
  while IFS= read -r -d '' pdf; do
    txt="$OUT/datasheets/$(basename "${pdf%.*}").txt"
    pdftotext -layout "$pdf" "$txt"
    echo "  ${pdf#"$REPO"/} -> ${txt#"$REPO"/}"
  done

echo "== hubble-device-sdk"
for ref in "${SDK_REFS[@]}"; do
  dest="$OUT/hubble-device-sdk/${ref//\//_}"
  rm -rf "$dest"
  git clone --quiet --depth 1 --branch "$ref" "$SDK_URL" "$dest"
  # Record the commit, then drop .git so the copy greps as plain files and
  # isn't mistaken for a nested repo.
  git -C "$dest" rev-parse HEAD > "$dest/.commit"
  rm -rf "$dest/.git"
  echo "  $ref @ $(cut -c1-12 "$dest/.commit")"
done

echo "== hubble.com/docs"
curl -fsSL "$HUBBLE_DOCS/llms.txt" -o "$OUT/hubble.com/llms.txt"
grep -o "$HUBBLE_DOCS/llms-[A-Za-z0-9-]*\.txt" "$OUT/hubble.com/llms.txt" | sort -u |
  while read -r url; do
    curl -fsSL "$url" -o "$OUT/hubble.com/$(basename "$url")"
    echo "  $(basename "$url")"
  done

date -u +%Y-%m-%dT%H:%M:%SZ > "$OUT/.fetched"
echo "done -> ${OUT#"$REPO"/}"
