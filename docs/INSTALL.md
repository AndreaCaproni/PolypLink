# Installazione su Steam Link

## Cosa serve

- Valve Steam Link (hardware 2015) con firmware aggiornato.
- Una chiavetta USB (FAT32 consigliato, EXT4 va bene).
- Un server Jellyfin raggiungibile dalla rete del Link.
- Il pacchetto `steamlink/apps/jellyfin-link.tgz` (lo produce `./build.sh`).

## Contenuto della chiavetta

```
steamlink/
├── apps/
│   └── jellyfin-link.tgz
├── config/system/
│   ├── enable_ssh.txt          (opzionale, per il debug)
│   └── jellyfin-server.txt     (opzionale)
└── factory_test/
    └── run.sh                  (opzionale, hook di boot)
```

- `jellyfin-server.txt` contiene l'indirizzo del server, es.
  `http://192.168.1.20:8096`. Se manca, l'app lo chiede al primo avvio.
- `factory_test/run.sh` è l'hook che il firmware esegue a ogni boot. Serve solo
  per SSH/debug: senza, l'installazione funziona lo stesso.

## Passi

1. Formatta la chiavetta (FAT32) e copia la cartella `steamlink/` nella
   **radice**.
2. Spegni il Link, inserisci la chiavetta, accendi.
3. L'installazione dura 1–2 minuti; la chiavetta viene smontata subito dopo.
4. Nella home del Link compare **JellyLink**.

## Primo avvio

1. **Server**: se non c'è `jellyfin-server.txt`, l'app cerca i Jellyfin sulla
   rete (UDP e scansione della subnet locale sulla porta 8096). Se non trova
   nulla, scegli *Inserisci l'indirizzo del server* e scrivilo con la tastiera
   a schermo (frecce + A). La porta `8096` viene aggiunta se la ometti.
2. **Accesso**: *Nome utente e password* (tastiera a schermo, con MAIUSC e
   simboli) oppure **Quick Connect** (codice di 6 cifre da inserire da
   Jellyfin, Impostazioni → Quick Connect).
3. Sei sulla home.

## SSH e debug (opzionale)

`deploy/factory_test/run.sh` viene eseguito come root a ogni avvio, con la
chiavetta montata. Il repository contiene solo un **esempio**
(`run.sh.example`): copialo in `run.sh` e aggiungi la tua chiave pubblica.
`run.sh` è ignorato da git perché contiene dati personali.

Log dell'app:

- sul dispositivo: `/home/apps/jellyfin-link/jellyfin-link-run.log`
- sulla chiavetta, al boot successivo: file di log nella radice della USB.

## Aggiornare o disinstallare

- **Aggiornare**: ricrea il pacchetto con `./build.sh`, ricopia `steamlink/`
  sulla chiavetta e riavvia il Link con la chiavetta inserita.
- **Disinstallare**: rimuovi `jellyfin-link.tgz` dalla chiavetta e reinstalla,
  oppure elimina `/home/apps/jellyfin-link` dal dispositivo via SSH.

## Problemi comuni

| Sintomo | Causa probabile |
|---|---|
| L'app non compare nella home | `steamlink/` non è nella radice della chiavetta, o il `.tgz` non ha la cartella dell'app come primo livello |
| "caricamento…" infinito | server non raggiungibile, IP/porta sbagliati, firewall |
| Il video parte dopo diversi secondi | il server sta preparando la transcodifica (normale, 1–3 s) |
| Sottotitoli indesiderati | scegli *Sottotitoli → Spenti* nel player (Y o pulsante dedicato) |
