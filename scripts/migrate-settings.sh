#!/bin/sh
# Copia le impostazioni della vecchia app JellyLink nella nuova PolypLink.
# Uso: migrate-settings.sh [OLD_HOME] [NEW_HOME]
set -eu
umask 077

OLD_HOME="${1:-/usr/local/jellyfin-link}"
NEW_HOME="${2:-/usr/local/polyplink}"
OLD_CONF="$OLD_HOME/.config/jellyfin-link/Jellyfin Link.conf"
NEW_CONF="$NEW_HOME/.config/polyplink/PolypLink.conf"

# Non sovrascrivere una configurazione nuova o gia' migrata.
if [ -f "$NEW_CONF" ] || [ ! -f "$OLD_CONF" ]; then
    exit 0
fi

mkdir -p "$(dirname "$NEW_CONF")"
chmod 700 "$(dirname "$NEW_CONF")" 2>/dev/null || true
cp "$OLD_CONF" "$NEW_CONF"
# Entrambi i file contengono il token: non devono essere leggibili da altri
# utenti. Il vecchio file resta per consentire un eventuale rollback.
chmod 600 "$NEW_CONF" 2>/dev/null || true
chmod 600 "$OLD_CONF" 2>/dev/null || true
echo "impostazioni migrate: $OLD_CONF -> $NEW_CONF"
