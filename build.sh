#!/usr/bin/env bash
#
# build.sh — compila il client Qt Quick per lo Steam Link e prepara il pacchetto USB
#
#   ./build.sh
#   ./build.sh --sdk ~/steamlink-sdk
#   ./build.sh --install /media/tuo_utente/USB
#   ./build.sh --clean
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APPNAME="jellyfin-link"
BUILD="$ROOT/build"
DIST="$ROOT/dist"
SDK="${STEAMLINK_SDK:-$ROOT/../steamlink-sdk}"
INSTALL_TO=""
CLEAN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sdk)     SDK="$2"; shift 2 ;;
    --install) INSTALL_TO="$2"; shift 2 ;;
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
if [[ "$SDK_REAL" == *" "* ]]; then
  SDK_LINK="/tmp/jellyfin-link-sdk"
  rm -f "$SDK_LINK"
  ln -s "$SDK_REAL" "$SDK_LINK"
  SDK_REAL="$SDK_LINK"
  echo "==> path con spazi: uso il symlink $SDK_LINK"
fi

echo "==> SDK: $SDK_REAL"
set +u
# shellcheck disable=SC1090
source "$SDK_REAL/setenv.sh" >/dev/null
set -u

[[ -n "${MARVELL_ROOTFS:-}" && -d "$MARVELL_ROOTFS" ]] || die "rootfs dello SDK non valida"
command -v qmake >/dev/null || die "qmake non trovato: setenv.sh non ha aggiunto Qt al PATH"

echo "==> spec qmake: $(qmake -query QMAKE_SPEC)"

mkdir -p "$BUILD"
cd "$BUILD"
qmake "$ROOT/client.pro" >/dev/null

echo "==> build"
python3 - "$ROOT/src/buildstamp.h" <<'PYEOF'
import datetime, sys
open(sys.argv[1], "w").write('#pragma once\n#define BUILD_STAMP "%s"\n'
    % datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
PYEOF
make -j"$(nproc)"

"$STRIP" "$BUILD/$APPNAME" 2>/dev/null || true

echo "==> package"
STAGE="$ROOT/build/pkg"
APP="$STAGE/$APPNAME"
rm -rf "$STAGE"
mkdir -p "$APP/bin"
cp "$BUILD/$APPNAME" "$APP/bin/$APPNAME"

# librerie ffmpeg con il nome SONAME richiesto dal linker dinamico
mkdir -p "$APP/lib"
for lib in "$ROOT/third_party/ffmpeg/lib/"*.so; do
  soname=$(readelf -d "$lib" 2>/dev/null | sed -n 's/.*SONAME.*\[\(.*\)\].*/\1/p')
  if [[ -n "$soname" ]]; then
    cp "$lib" "$APP/lib/$soname"
  fi
done
cp "$ROOT/deploy/run.sh" "$APP/run.sh"
chmod +x "$APP/run.sh"
cp "$ROOT/packaging/icon.png" "$APP/icon.png"
cat > "$APP/toc.txt" <<EOF
name=JellyLink
icon=icon.png
run=run.sh
EOF

# Formato identico a quello usato dal port Kodi: un .tgz dentro steamlink/apps/
# con la cartella dell'app alla radice. E' il formato che l'installer del Link
# gestisce di sicuro (una cartella nuda non viene installata).
rm -rf "$DIST"
mkdir -p "$DIST/steamlink/apps" "$DIST/steamlink/config/system" \
         "$DIST/steamlink/factory_test"
( cd "$STAGE" && tar -czf "$DIST/steamlink/apps/$APPNAME.tgz" "$APPNAME" )
echo 1 > "$DIST/steamlink/config/system/enable_ssh.txt"

# Hook eseguito dal firmware a ogni avvio, con la chiavetta ancora montata:
# abilita SSH e copia i log dell'esecuzione precedente sulla chiavetta.
# L'hook puo' contenere dati personali (chiave SSH): in git c'e' solo
# l'esempio. Se run.sh esiste, ha la precedenza.
HOOK="$ROOT/deploy/factory_test/run.sh"
[[ -f "$HOOK" ]] || HOOK="$ROOT/deploy/factory_test/run.sh.example"
[[ -f "$HOOK" ]] || die "manca deploy/factory_test/run.sh(.example)"
cp "$HOOK" "$DIST/steamlink/factory_test/run.sh"
chmod +x "$DIST/steamlink/factory_test/run.sh"

echo
echo "==> fatto"
echo "    pacchetto: $DIST/steamlink/apps/$APPNAME.tgz ($(du -h "$DIST/steamlink/apps/$APPNAME.tgz" | cut -f1))"

if [[ -n "$INSTALL_TO" ]]; then
  [[ -d "$INSTALL_TO" ]] || die "'$INSTALL_TO' non è una directory"
  cp -r "$DIST/steamlink" "$INSTALL_TO/"
  sync
  echo "    installato in $INSTALL_TO/steamlink/"
fi

cat <<EOF

Sulla chiavetta:
  steamlink/apps/$APPNAME.tgz
  steamlink/config/system/enable_ssh.txt
  steamlink/factory_test/run.sh     (abilita SSH e copia i log al boot)

Oppure, direttamente:
  ./build.sh --install /media/tuo_utente/USB

Log sul dispositivo: /home/apps/jellyfin-link/jellyfin-link-run.log
EOF
