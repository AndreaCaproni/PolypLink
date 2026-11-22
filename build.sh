#!/usr/bin/env bash
#
# build.sh — builds the Qt Quick client for the Steam Link and prepares the USB package
#
#   ./build.sh
#   ./build.sh --sdk ~/steamlink-sdk
#   ./build.sh --install /media/tuo_utente/USB
#   ./build.sh --dev-ssh          (include the personal SSH hook)
#   ./build.sh --clean
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APPNAME="polyplink"
BUILD="$ROOT/build"
DIST="$ROOT/dist"
SDK="${STEAMLINK_SDK:-$ROOT/../steamlink-sdk}"
INSTALL_TO=""
CLEAN=0
DEV_SSH=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sdk)     SDK="$2"; shift 2 ;;
    --install) INSTALL_TO="$2"; shift 2 ;;
    --dev-ssh) DEV_SSH=1; shift ;;
    --clean)   CLEAN=1; shift ;;
    -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
    *) echo "Opzione sconosciuta: $1" >&2; exit 2 ;;
  esac
done

die() { echo "errore: $*" >&2; exit 1; }

command -v make >/dev/null || die "serve make"

if [[ $CLEAN -eq 1 ]]; then
  rm -rf "$BUILD" "$DIST"
  echo "pulito."
fi

[[ -f "$SDK/setenv.sh" ]] || die "Steam Link SDK non trovato in '$SDK'."

SDK_REAL="$(cd "$SDK" && pwd -P)"
SDK_TMP=""
cleanup() { if [[ -n "$SDK_TMP" ]]; then rm -rf "$SDK_TMP"; fi; }
trap cleanup EXIT
if [[ "$SDK_REAL" == *" "* ]]; then
  SDK_TMP="$(mktemp -d "${TMPDIR:-/tmp}/polyplink-sdk.XXXXXX")"
  ln -s "$SDK_REAL" "$SDK_TMP/sdk"
  SDK_REAL="$SDK_TMP/sdk"
  echo "==> path con spazi: uso il symlink $SDK_REAL"
fi

echo "==> SDK: $SDK_REAL"
set +u
# shellcheck disable=SC1090
source "$SDK_REAL/setenv.sh" >/dev/null
set -u

[[ -n "${MARVELL_ROOTFS:-}" && -d "$MARVELL_ROOTFS" ]] || die "rootfs dello SDK non valida"
command -v qmake >/dev/null || die "qmake non trovato: setenv.sh non ha aggiunto Qt al PATH"

# The .ts translations are the source; the .qm must be regenerated before
# qmake, so the resource compiled into the binary is always up to date.
if [[ -d "$ROOT/i18n" ]] && compgen -G "$ROOT/i18n/*.ts" >/dev/null; then
  LRELEASE_BIN="${LRELEASE:-$(command -v lrelease || true)}"
  if [[ -n "$LRELEASE_BIN" ]]; then
    echo "==> traduzioni"
    for ts in "$ROOT"/i18n/*.ts; do
      "$LRELEASE_BIN" "$ts" -qm "${ts%.ts}.qm"
    done
  else
    echo "avviso: lrelease non trovato, uso i .qm esistenti" >&2
  fi
fi

echo "==> spec qmake: $(qmake -query QMAKE_SPEC)"

mkdir -p "$BUILD"
cd "$BUILD"
qmake "$ROOT/client.pro" >/dev/null

echo "==> build"
BUILD_EPOCH="${SOURCE_DATE_EPOCH:-$(date +%s)}"
python3 - "$ROOT/src/buildstamp.h" "$BUILD_EPOCH" <<'PYEOF'
import datetime, sys
epoch = int(sys.argv[2])
stamp = datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
open(sys.argv[1], "w").write('#pragma once\n#define BUILD_STAMP "%s"\n' % stamp)
PYEOF
make -j"$(nproc)"

"$STRIP" "$BUILD/$APPNAME" 2>/dev/null || true

echo "==> package"
STAGE="$ROOT/build/pkg"
APP="$STAGE/$APPNAME"
rm -rf "$STAGE"
mkdir -p "$APP/bin"
cp "$BUILD/$APPNAME" "$APP/bin/$APPNAME"

# ffmpeg libraries with the SONAME name required by the dynamic linker
mkdir -p "$APP/lib"
for lib in "$ROOT/third_party/ffmpeg/lib/"*.so; do
  soname=$(readelf -d "$lib" 2>/dev/null | sed -n 's/.*SONAME.*\[\(.*\)\].*/\1/p')
  if [[ -n "$soname" ]]; then
    cp "$lib" "$APP/lib/$soname"
  fi
done
cp "$ROOT/deploy/run.sh" "$APP/run.sh"
cp "$ROOT/scripts/migrate-settings.sh" "$APP/migrate-settings.sh"
chmod +x "$APP/run.sh" "$APP/migrate-settings.sh"
cp "$ROOT/packaging/icon.png" "$APP/icon.png"

# License notices that must travel with the binary.
mkdir -p "$APP/licenses"
cp "$ROOT/LICENSE" "$APP/licenses/LICENSE"
cp "$ROOT/THIRD_PARTY.md" "$APP/licenses/THIRD_PARTY.md"
cp "$ROOT/third_party/ffmpeg/LICENSE.LGPLv2.1" "$APP/licenses/FFMPEG-LGPL-2.1.txt"
cp "$ROOT/packaging/licenses/FFMPEG-SOURCE-OFFER.md" "$APP/licenses/FFMPEG-SOURCE-OFFER.md"
cp "$ROOT/packaging/licenses/README.txt" "$APP/licenses/README.txt"

cat > "$APP/toc.txt" <<EOF
name=PolypLink
icon=icon.png
run=run.sh
EOF

# Same format as the one used by the Kodi port: a .tgz inside steamlink/apps/
# with the app folder at the root. It is the format the Link installer
# definitely handles (a bare folder is not installed).
rm -rf "$DIST"
mkdir -p "$DIST/steamlink/apps"
( cd "$STAGE" && tar --sort=name --owner=0 --group=0 --numeric-owner \
    --mtime="@$BUILD_EPOCH" -czf "$DIST/steamlink/apps/$APPNAME.tgz" "$APPNAME" )

# SSH and the personal hook are NOT part of the default kit: the normal build
# must be copyable/shareable without keys or passwords. They are only needed
# for development on the device, so they require --dev-ssh.
if [[ $DEV_SSH -eq 1 ]]; then
  mkdir -p "$DIST/steamlink/config/system" "$DIST/steamlink/factory_test"
  echo 1 > "$DIST/steamlink/config/system/enable_ssh.txt"
  HOOK="$ROOT/deploy/factory_test/run.sh"
  [[ -f "$HOOK" ]] || HOOK="$ROOT/deploy/factory_test/run.sh.example"
  [[ -f "$HOOK" ]] || die "manca deploy/factory_test/run.sh(.example)"
  cp "$HOOK" "$DIST/steamlink/factory_test/run.sh"
  chmod +x "$DIST/steamlink/factory_test/run.sh"
fi

echo
echo "==> fatto"
echo "    pacchetto: $DIST/steamlink/apps/$APPNAME.tgz ($(du -h "$DIST/steamlink/apps/$APPNAME.tgz" | cut -f1))"

if [[ -n "$INSTALL_TO" ]]; then
  [[ -d "$INSTALL_TO" ]] || die "'$INSTALL_TO' non è una directory"
  cp -r "$DIST/steamlink" "$INSTALL_TO/"
  sync
  echo "    installato in $INSTALL_TO/steamlink/"
fi

echo
echo "Sulla chiavetta:"
echo "  steamlink/apps/$APPNAME.tgz"
if [[ $DEV_SSH -eq 1 ]]; then
  echo "  steamlink/config/system/enable_ssh.txt"
  echo "  steamlink/factory_test/run.sh     (hook SSH personale)"
fi
echo
echo "Oppure, direttamente:"
echo "  ./build.sh --install /media/tuo_utente/USB"
echo
echo "Log sul dispositivo: /home/apps/polyplink/polyplink-run.log"

if [[ $DEV_SSH -eq 1 && -f "$ROOT/deploy/factory_test/run.sh" ]]; then
  echo
  echo "ATTENZIONE: il kit --dev-ssh contiene la tua chiave SSH e la password root."
  echo "Non condividerlo."
fi
