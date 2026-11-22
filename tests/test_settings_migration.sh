#!/bin/sh
# JellyLink -> PolypLink settings migration test.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

OLD_HOME="$TMP/old-home"
NEW_HOME="$TMP/new-home"
OLD_CONF="$OLD_HOME/.config/jellyfin-link/Jellyfin Link.conf"
NEW_CONF="$NEW_HOME/.config/polyplink/PolypLink.conf"

mkdir -p "$(dirname "$OLD_CONF")"
printf '[General]\nserver=http://example\nlanguage=it\n' > "$OLD_CONF"

"$ROOT/scripts/migrate-settings.sh" "$OLD_HOME" "$NEW_HOME"
[ -f "$NEW_CONF" ] || { echo "FAIL: config non migrata"; exit 1; }
grep -q '^server=http://example$' "$NEW_CONF" || { echo "FAIL: contenuto errato"; exit 1; }
MODE=$(stat -c '%a' "$NEW_CONF" 2>/dev/null || echo "")
[ "$MODE" = "600" ] || { echo "FAIL: permessi config $MODE (atteso 600)"; exit 1; }
OLD_MODE=$(stat -c '%a' "$OLD_CONF" 2>/dev/null || echo "")
[ "$OLD_MODE" = "600" ] || { echo "FAIL: permessi vecchia config $OLD_MODE (atteso 600)"; exit 1; }

# A second run must not overwrite the new configuration.
printf 'keep=me\n' > "$NEW_CONF"
"$ROOT/scripts/migrate-settings.sh" "$OLD_HOME" "$NEW_HOME"
grep -q '^keep=me$' "$NEW_CONF" || { echo "FAIL: config sovrascritta"; exit 1; }

# No old config: nothing must be created.
rm -rf "$NEW_HOME" "$OLD_HOME"
"$ROOT/scripts/migrate-settings.sh" "$OLD_HOME" "$NEW_HOME"
[ ! -f "$NEW_CONF" ] || { echo "FAIL: creata config senza sorgente"; exit 1; }

echo "settings migration: ok"
