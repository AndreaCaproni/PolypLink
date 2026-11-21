#!/bin/sh
# Launcher di PolypLink sul Valve Steam Link.
#
# Scrive un log dettagliato nella cartella dell'app (che sta su /home/apps, una
# immagine disco persistente). La chiavetta NON e' montata mentre l'app gira:
# il firmware la smonta dopo l'installazione. I log vengono copiati sulla
# chiavetta all'avvio successivo dallo script factory_test/run.sh.

APP_DIR=$(dirname "$0")
cd "$APP_DIR" 2>/dev/null || exit 1

# Una sola istanza: evita che due launcher tronchino lo stesso log e che due
# processi leggano il controller nello stesso momento.
if command -v flock >/dev/null 2>&1; then
    exec 9>/tmp/polyplink-run.lock
    if ! flock -n 9; then
        echo "Polyplink e' gia' in esecuzione; non avvio una seconda istanza."
        exit 0
    fi
fi

# Il token Jellyfin e' salvato nel file di configurazione: creiamo file e
# directory leggibili solo da root (l'app gira come root sul Link).
umask 077

# librerie ffmpeg (decoder AAC) incluse nel pacchetto
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

# Migra le impostazioni della vecchia app (server, token, lingua, qualita').
if [ -x "$APP_DIR/migrate-settings.sh" ]; then
    "$APP_DIR/migrate-settings.sh" /usr/local/jellyfin-link "$HOME" >>"$LOG" 2>&1 || true
fi
CONF="$HOME/.config/polyplink/PolypLink.conf"
if [ -f "$CONF" ]; then
    chmod 600 "$CONF" 2>>"$LOG" || true
fi
# Anche la vecchia configurazione JellyLink contiene il token.
OLD_CONF="/usr/local/jellyfin-link/.config/jellyfin-link/Jellyfin Link.conf"
if [ -f "$OLD_CONF" ]; then
    chmod 600 "$OLD_CONF" 2>>"$LOG" || true
fi

export QT_QPA_PLATFORM=eglfs
# EGLFS disegna un cursore software in alto a sinistra: qui non serve
export QT_QPA_EGLFS_HIDECURSOR=1
log "QT_QPA_PLATFORM=$QT_QPA_PLATFORM"

renice -10 -p "$(pidof PE_Single_CPU)" >> "$LOG" 2>&1

log "--- esecuzione ---"
nice -n -10 ./bin/polyplink >> "$LOG" 2>&1
rc=$?
log "--- uscita: $rc ($(date)) ---"
