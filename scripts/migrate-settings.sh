#!/bin/sh
# Copies the settings from the old JellyLink app into the new PolypLink.
# Usage: migrate-settings.sh [OLD_HOME] [NEW_HOME]
set -eu
umask 077

OLD_HOME="${1:-/usr/local/jellyfin-link}"
NEW_HOME="${2:-/usr/local/polyplink}"
OLD_CONF="$OLD_HOME/.config/jellyfin-link/Jellyfin Link.conf"
NEW_CONF="$NEW_HOME/.config/polyplink/PolypLink.conf"

# Do not overwrite a new or already migrated configuration.
if [ -f "$NEW_CONF" ] || [ ! -f "$OLD_CONF" ]; then
    exit 0
fi

mkdir -p "$(dirname "$NEW_CONF")"
chmod 700 "$(dirname "$NEW_CONF")" 2>/dev/null || true
cp "$OLD_CONF" "$NEW_CONF"
# Both files contain the token: they must not be readable by other users.
# The old file is kept to allow a rollback if needed.
chmod 600 "$NEW_CONF" 2>/dev/null || true
chmod 600 "$OLD_CONF" 2>/dev/null || true
echo "impostazioni migrate: $OLD_CONF -> $NEW_CONF"
