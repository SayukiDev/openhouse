#!/usr/bin/env bash
# Points the Nix flake's default package (nix/package-bin.nix) at a published release: downloads
# its Linux tarball and writes the version and the tarball's SHA-256 to nix/release.json.
#   scripts/bump-nix-release.sh v1.0.5      (or 1.0.5)
# Run after the release is published, then commit nix/release.json.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
version=${1:?usage: $0 vX.Y.Z}
version=${version#v}
url="https://github.com/YusufOzmen01/openhouse/releases/download/v$version/openhouse-linux-x64.tar.gz"
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT
echo "Downloading $url"
curl -fsSL -o "$tmp" "$url"
sha=$(sha256sum "$tmp" | cut -d' ' -f1)
printf '{\n  "version": "%s",\n  "sha256": "%s"\n}\n' "$version" "$sha" > "$repo/nix/release.json"
echo "nix/release.json: v$version, sha256 $sha"
