#!/bin/sh
# Copia le impostazioni della vecchia app JellyLink nella nuova PolypLink.
# Uso: migrate-settings.sh [OLD_HOME] [NEW_HOME]
set -eu

OLD_HOME="${1:-/usr/local/jellyfin-link}"
NEW_HOME="${2:-/usr/local/polyplink}"
OLD_CONF="$OLD_HOME/.config/jellyfin-link/Jellyfin Link.conf"
NEW_CONF="$NEW_HOME/.config/polyplink/PolypLink.conf"

# Non sovrascrivere una configurazione nuova o gia' migrata.
if [ -f "$NEW_CONF" ] || [ ! -f "$OLD_CONF" ]; then
    exit 0
fi

mkdir -p "$(dirname "$NEW_CONF")"
cp "$OLD_CONF" "$NEW_CONF"
echo "impostazioni migrate: $OLD_CONF -> $NEW_CONF"
