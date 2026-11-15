# Changelog

## 0.1.0-beta — 2026-11-15

Prima beta pubblica.

### Aggiunto

- Client Jellyfin nativo per Steam Link: nessun browser, nessun Chromium.
- Home con *Continua a guardare*, *Prossimo episodio*, *Aggiunti di recente*
  (con libreria di provenienza) e librerie.
- Navigazione completa: film, serie, stagioni, episodi, scheda con metadati.
- Visto / non visto e preferiti, con aggiornamento della home.
- Ripresa della posizione, *Riprendi* e *Riproduci dall'inizio*.
- Player HLS con decodifica H.264 hardware (`SLVideo`) e audio AAC
  (`SLAudio`), sincronizzazione audio/video.
- OSD navigabile: barra di seek con cursore e anteprima del tempo, passo
  accelerato (30 s → 1 min → 5 min), pausa, -10/+30 s, traccia audio,
  sottotitoli in burn-in, qualità (4/8/12/20 Mbps), statistiche.
- Accesso con indirizzo manuale, utente/password (tastiera a schermo) e
  Quick Connect.
- Barra superiore con Home, Preferiti, Account e Impostazioni.
- Serie e stagioni: *Riproduci* (prossimo episodio), *Stagioni*, visto e
  preferito; pagina stagione con pulsanti, trama ed elenco episodi.
- Ricerca del server con UDP Jellyfin e scansione della subnet locale.
- Interfaccia 1280x720 scalata, pensata per il telecomando.

### Correzioni (prima della pubblicazione)

- Se il server non risponde, l'avvio della riproduzione viene annullato con un
  messaggio: niente più schermo nero per 8 secondi.
- Se la connessione cade durante la riproduzione, l'OSD mostra "Connessione
  persa… riprovo"; quando il buffer finisce il player si chiude con l'errore,
  invece di restare sul fermo immagine.
- La fine naturale di un film chiude il player e registra la posizione.
- Aprendo un altro titolo non resta più la scheda precedente: se il server non
  risponde compare "Impossibile caricare" con il motivo.
- Home offline: messaggio chiaro ("Server non raggiungibile") e **A** per
  riprovare.
- Timeout delle richieste API ridotto a 10 s; i report di avanzamento non si
  accodano più (uno alla volta).
- Demux incrementale: eliminato il microlag a ogni segmento (3 s).
- Login: il server salvato resta in cima, *Esci* e *Cambia utente* funzionano,
  un 401 riporta al login.
- Report di sessione corretti: la dashboard Jellyfin mostra *Transcode* e il
  MediaSourceId giusto.
- Griglia stagioni con locandine come quella dei film.

### Limitazioni note

- Seek e cambio impostazioni riavviano il flusso (~2 s).
- Sottotitoli solo in burn-in.
- Nessuna ricerca, coda di riproduzione o autoplay del prossimo episodio.
- Nessuna miniatura trickplay sulla barra.
- Impostazioni e Account sono pagine con voci segnaposto.
- Solo H.264 8-bit: niente HEVC, HDR o AV1.
- Avviando da una serie/stagione l'audio usa la traccia predefinita finché non
  apri la scheda dell'episodio.
- La ricerca automatica copre solo la /24 locale.
