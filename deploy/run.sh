#!/bin/sh
# PolypLink launcher on the Valve Steam Link.
#
# Writes a detailed log in the app folder (which is on /home/apps, a persistent
# disk image). The USB stick is NOT mounted while the app runs: the firmware
# unmounts it after installation. The logs are copied to the USB stick on the
# next boot by the factory_test/run.sh script.

APP_DIR=$(dirname "$0")
cd "$APP_DIR" 2>/dev/null || exit 1

# Single instance: prevents two launchers from truncating the same log and two
# processes from reading the controller at the same time.
if command -v flock >/dev/null 2>&1; then
    exec 9>/tmp/polyplink-run.lock
    if ! flock -n 9; then
        echo "Polyplink e' gia' in esecuzione; non avvio una seconda istanza."
        exit 0
    fi
fi

# The Jellyfin token is stored in the configuration file: we create files and
# directories readable only by root (the app runs as root on the Link).
umask 077

# ffmpeg libraries (AAC decoder) bundled in the package
export LD_LIBRARY_PATH="$APP_DIR/lib:$LD_LIBRARY_PATH"

LOG="$APP_DIR/polyplink-run.log"
if ! ( : > "$LOG" ) 2>/dev/null; then
    LOG=/tmp/polyplink-run.log
    ( : > "$LOG" ) 2>/dev/null || LOG=/dev/null
fi

log() { echo "$@" >> "$LOG"; }

log "=== avvio $(date) ==="
log "script: $0"
log "app dir: $(pwd)"
log "uname: $(uname -a)"
log "usb montata: $(grep -c /mnt/disk /proc/mounts 2>/dev/null) (0 = no, ed e' normale)"
log "contenuto:"
ls -la >> "$LOG" 2>&1
log "binario:"
ls -l ./bin/polyplink >> "$LOG" 2>&1
log "ldd:"
ldd ./bin/polyplink >> "$LOG" 2>&1

export HOME=/usr/local/polyplink
mkdir -p "$HOME/.config/polyplink" 2>>"$LOG"
chmod 700 "$HOME" "$HOME/.config" "$HOME/.config/polyplink" 2>>"$LOG" || true

# Migrates the settings from the old app (server, token, language, quality).
if [ -x "$APP_DIR/migrate-settings.sh" ]; then
    "$APP_DIR/migrate-settings.sh" /usr/local/jellyfin-link "$HOME" >>"$LOG" 2>&1 || true
fi
CONF="$HOME/.config/polyplink/PolypLink.conf"
if [ -f "$CONF" ]; then
    chmod 600 "$CONF" 2>>"$LOG" || true
fi
# The old JellyLink configuration also contains the token.
OLD_CONF="/usr/local/jellyfin-link/.config/jellyfin-link/Jellyfin Link.conf"
if [ -f "$OLD_CONF" ]; then
    chmod 600 "$OLD_CONF" 2>>"$LOG" || true
fi

export QT_QPA_PLATFORM=eglfs
# EGLFS draws a software cursor in the top-left corner: not needed here
export QT_QPA_EGLFS_HIDECURSOR=1
log "QT_QPA_PLATFORM=$QT_QPA_PLATFORM"

renice -10 -p "$(pidof PE_Single_CPU)" >> "$LOG" 2>&1

log "--- esecuzione ---"
nice -n -10 ./bin/polyplink >> "$LOG" 2>&1
rc=$?
log "--- uscita: $rc ($(date)) ---"
