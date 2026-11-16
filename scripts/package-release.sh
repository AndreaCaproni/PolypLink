#!/usr/bin/env bash
# Crea gli asset di release da packaging/release/ e calcola i checksum.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP="polyplink"
VERSION="$(sed -n 's/#define APP_VERSION "\(.*\)"/\1/p' "$ROOT/src/version.h")"
VERSION_SHORT="${VERSION%-beta}"
[[ -n "$VERSION" ]] || { echo "versione non trovata in src/version.h" >&2; exit 1; }

"$ROOT/build.sh"

RELEASE="$ROOT/dist/release"
rm -rf "$RELEASE"
mkdir -p "$RELEASE/usb/steamlink/apps"
cp "$ROOT/dist/steamlink/apps/$APP.tgz" "$RELEASE/usb/steamlink/apps/"
cp "$ROOT/dist/steamlink/apps/$APP.tgz" "$RELEASE/$APP-$VERSION.tgz"

for doc in LEGGIMI.txt README.txt; do
  sed -e "s/@VERSION@/$VERSION/g" -e "s/@VERSION_SHORT@/$VERSION_SHORT/g" \
      "$ROOT/packaging/release/$doc" > "$RELEASE/usb/$doc"
done
sed -e "s/@VERSION@/$VERSION/g" -e "s/@VERSION_SHORT@/$VERSION_SHORT/g" \
    "$ROOT/packaging/release/RELEASE_NOTES.md" > "$RELEASE/RELEASE_NOTES.md"

python3 - "$RELEASE" "$APP" "$VERSION" <<'PY'
import os, sys, zipfile
release, app, version = sys.argv[1], sys.argv[2], sys.argv[3]
out = os.path.join(release, f"{app}-{version}-usb.zip")
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for base, _, files in os.walk(os.path.join(release, "usb")):
        for name in files:
            path = os.path.join(base, name)
            z.write(path, os.path.relpath(path, os.path.join(release, "usb")))
print("zip:", out)
PY

if grep -RInE "192\.168\.|api_key=|ApiKey=|BEGIN OPENSSH|ssh-rsa|ssh-ed25519" \
    "$RELEASE/usb" "$RELEASE/RELEASE_NOTES.md" >/dev/null; then
  echo "errore: dati sensibili o IP negli asset di release" >&2
  exit 1
fi

(
  cd "$RELEASE"
  sha256sum "$APP-$VERSION-usb.zip" "$APP-$VERSION.tgz" > SHA256SUMS
  cat SHA256SUMS
)
echo "asset pronti in $RELEASE"
