#!/usr/bin/env bash
# Creates the release assets from packaging/release/ and computes the checksums.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP="polyplink"
VERSION="$(sed -n 's/#define APP_VERSION "\(.*\)"/\1/p' "$ROOT/src/version.h")"
VERSION_SHORT="${VERSION%-beta}"
[[ -n "$VERSION" ]] || { echo "versione non trovata in src/version.h" >&2; exit 1; }

# Reproducible release builds: timestamp, tar owner and tar mtime come from
# the commit, not from the machine that builds the package.
if [[ -z "${SOURCE_DATE_EPOCH:-}" ]] &&
   git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  export SOURCE_DATE_EPOCH="$(git -C "$ROOT" log -1 --format=%ct)"
fi

"$ROOT/build.sh"

RELEASE="$ROOT/dist/release"
rm -rf "$RELEASE"
mkdir -p "$RELEASE/usb/steamlink/apps"
cp "$ROOT/dist/steamlink/apps/$APP.tgz" "$RELEASE/usb/steamlink/apps/"
cp "$ROOT/dist/steamlink/apps/$APP.tgz" "$RELEASE/$APP-$VERSION.tgz"

# License copies also outside the package, for those who open the USB zip.
# Do NOT copy factory_test/ or config/: they may contain SSH keys and personal
# URLs used for development on the device.
mkdir -p "$RELEASE/usb/licenses"
cp "$ROOT/LICENSE" "$RELEASE/usb/licenses/LICENSE"
cp "$ROOT/THIRD_PARTY.md" "$RELEASE/usb/licenses/THIRD_PARTY.md"
cp "$ROOT/third_party/ffmpeg/LICENSE.LGPLv2.1" "$RELEASE/usb/licenses/FFMPEG-LGPL-2.1.txt"
cp "$ROOT/packaging/licenses/FFMPEG-SOURCE-OFFER.md" "$RELEASE/usb/licenses/FFMPEG-SOURCE-OFFER.md"
cp "$ROOT/packaging/licenses/README.txt" "$RELEASE/usb/licenses/README.txt"

for doc in LEGGIMI.txt README.txt; do
  sed -e "s/@VERSION@/$VERSION/g" -e "s/@VERSION_SHORT@/$VERSION_SHORT/g" \
      "$ROOT/packaging/release/$doc" > "$RELEASE/usb/$doc"
done
sed -e "s/@VERSION@/$VERSION/g" -e "s/@VERSION_SHORT@/$VERSION_SHORT/g" \
    "$ROOT/packaging/release/RELEASE_NOTES.md" > "$RELEASE/RELEASE_NOTES.md"

python3 - "$RELEASE" "$APP" "$VERSION" <<'PY'
import os, sys, time, zipfile
release, app, version = sys.argv[1], sys.argv[2], sys.argv[3]
epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
date_time = time.gmtime(epoch)[:6] if epoch > 0 else time.localtime()[:6]
out = os.path.join(release, f"{app}-{version}-usb.zip")
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for base, _, files in os.walk(os.path.join(release, "usb")):
        for name in sorted(files):
            path = os.path.join(base, name)
            rel = os.path.relpath(path, os.path.join(release, "usb"))
            info = zipfile.ZipInfo(rel, date_time)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            with open(path, "rb") as handle:
                z.writestr(info, handle.read())
print("zip:", out)
PY

python3 - "$RELEASE" "$APP" "$VERSION" <<'PYCHECK'
import os
import re
import sys
import tarfile

release, app, version = sys.argv[1], sys.argv[2], sys.argv[3]
bad_names = ("factory_test", "enable_ssh", "jellyfin-server.txt", "steamlink_key")
strong_patterns = [
    re.compile(rb"steamlink123", re.I),
    re.compile(rb"BEGIN [A-Z ]*PRIVATE KEY", re.I),
    re.compile(rb"ssh-(?:rsa|ed25519|ecdsa) [A-Za-z0-9+/]{60,}", re.I),
]
text_patterns = strong_patterns + [
    re.compile(rb"(?:^|[^0-9])(?:10\.\d{1,3}\.|192\.168\.|172\.(?:1[6-9]|2\d|3[01])\.|127\.\d{1,3}\.|169\.254\.)"),
    re.compile(rb"(?:api[_-]?key|access[_-]?token|token|secret|password|pw)=", re.I),
]
text_ext = {".md", ".txt", ".sh", ".json", ".conf", ".xml", ".html", ".js",
            ".css", ".qml", ".cpp", ".h", ".pro", ".ini", ".yml", ".yaml"}

def fail(message):
    print("errore: " + message, file=sys.stderr)
    raise SystemExit(1)

def check_bytes(data, label):
    extension = os.path.splitext(label)[1].lower()
    patterns = text_patterns if extension in text_ext else strong_patterns
    for pattern in patterns:
        if pattern.search(data):
            fail(f"dato sensibile in {label}: {pattern.pattern!r}")

for base, dirs, files in os.walk(os.path.join(release, "usb")):
    for name in files:
        path = os.path.join(base, name)
        if any(part in name for part in bad_names):
            fail(f"file personale nel kit: {name}")
        with open(path, "rb") as handle:
            check_bytes(handle.read(), os.path.relpath(path, release))
with open(os.path.join(release, "RELEASE_NOTES.md"), "rb") as handle:
    check_bytes(handle.read(), "RELEASE_NOTES.md")

tgz = os.path.join(release, "usb", "steamlink", "apps", f"{app}.tgz")
with tarfile.open(tgz, "r:gz") as archive:
    for member in archive.getmembers():
        if any(part in member.name for part in bad_names):
            fail(f"file personale nel pacchetto: {member.name}")
        if member.isfile():
            extracted = archive.extractfile(member)
            if extracted is not None:
                check_bytes(extracted.read(), f"pacchetto:{member.name}")
PYCHECK

(
  cd "$RELEASE"
  sha256sum "$APP-$VERSION-usb.zip" "$APP-$VERSION.tgz" > SHA256SUMS
  cat SHA256SUMS
)
echo "asset pronti in $RELEASE"
