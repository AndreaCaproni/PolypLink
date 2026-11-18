#!/usr/bin/env bash
# Controlli rapidi senza compilare: sintassi degli script e QML.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="${STEAMLINK_SDK:-$ROOT/../steamlink-sdk}"
QMLLINT="$SDK/rootfs/usr/local/Qt-5.14.1/bin/qmllint"

echo "==> sintassi script"
for f in "$ROOT/build.sh" "$ROOT/deploy/run.sh" \
         "$ROOT/deploy/factory_test/run.sh.example" "$ROOT/scripts/check.sh"; do
  [[ -f "$f" ]] || continue
  bash -n "$f"
  echo "    ok $(basename "$f")"
done

echo "==> QML"
if [[ -x "$QMLLINT" ]]; then
  "$QMLLINT" "$ROOT"/qml/*.qml
  echo "    qmllint ok"
else
  echo "    qmllint non trovato (imposta STEAMLINK_SDK); salto"
fi

echo "==> librerie FFmpeg"
( cd "$ROOT/third_party/ffmpeg" && sha256sum -c SHA256SUMS )

echo "==> test"
python3 "$ROOT/tests/check_translations.py"
sh "$ROOT/tests/test_settings_migration.sh"

if command -v g++ >/dev/null 2>&1 && command -v pkg-config >/dev/null 2>&1 &&
   pkg-config --exists Qt5Core Qt5Network; then
  OUT="/tmp/test_serverurl.$$"
  g++ -std=c++17 -fPIC "$ROOT/tests/test_serverurl.cpp" "$ROOT/src/serverurl.cpp" \
      $(pkg-config --cflags --libs Qt5Core Qt5Network) -o "$OUT"
  "$OUT"
  rm -f "$OUT"
else
  echo "    server URL: salto (Qt5 host non disponibile)"
fi

echo "fatto."
