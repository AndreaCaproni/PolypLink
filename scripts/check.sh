#!/usr/bin/env bash
# Quick checks without compiling: script syntax and QML.
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
  OUT="$(mktemp "${TMPDIR:-/tmp}/test_serverurl.XXXXXX")"
  g++ -std=c++17 -fPIC "$ROOT/tests/test_serverurl.cpp" "$ROOT/src/serverurl.cpp" \
      $(pkg-config --cflags --libs Qt5Core Qt5Network) -o "$OUT"
  "$OUT"
  rm -f "$OUT"

  OUT="$(mktemp "${TMPDIR:-/tmp}/test_discovery_address.XXXXXX")"
  g++ -std=c++17 -fPIC -I"$ROOT/src" "$ROOT/tests/test_discovery_address.cpp" \
      $(pkg-config --cflags --libs Qt5Core Qt5Network) -o "$OUT"
  "$OUT"
  rm -f "$OUT"

  OUT="$(mktemp "${TMPDIR:-/tmp}/test_inputmap.XXXXXX")"
  g++ -std=c++17 -fPIC -I"$ROOT/src" "$ROOT/tests/test_inputmap.cpp" \
      "$ROOT/src/inputmap.cpp" $(pkg-config --cflags --libs Qt5Core) -o "$OUT"
  "$OUT"
  rm -f "$OUT"
else
  echo "    server URL: salto (Qt5 host non disponibile)"
fi

if command -v g++ >/dev/null 2>&1; then
  OUT="$(mktemp "${TMPDIR:-/tmp}/test_tsdemux.XXXXXX")"
  g++ -std=c++17 -fPIC -fsanitize=address,undefined -fno-omit-frame-pointer \
      -I"$ROOT/src" "$ROOT/tests/test_tsdemux.cpp" "$ROOT/src/tsdemux.cpp" -o "$OUT"
  "$OUT"
  rm -f "$OUT"
else
  echo "    tsdemux: salto (g++ non disponibile)"
fi

echo "fatto."
