# Architettura e scelta del client

> Documento di progetto, scritto prima dell'implementazione. Le milestone in
> fondo sono storiche: lo stato attuale è in [../README.md](../README.md) e
> [../CHANGELOG.md](../CHANGELOG.md).

---

## 1. Requisiti, tradotti in vincoli

| Cosa vuoi | Vincolo tecnico |
|---|---|
| App **indipendente** sul Steam Link | binario ARMv7 che parte dalla home, nessun PC, nessun browser |
| **Leggera** | niente Chromium/Electron; RAM < ~150 MB, CPU quasi tutta libera per il demux |
| Installabile **via USB** come Kodi/Moonlight | cartella `steamlink/apps/<nome>/` con `toc.txt`, icona, script di avvio |
| **Tutta la libreria** | API Jellyfin: login, viste, griglia, dettaglio, ripresa |
| HD | H.264 1080p, che è l'unico codec che il box decodifica in hardware |
| Transcodifica lato server | scontata: il client chiede sempre un formato unico |

Conseguenza: il client non deve saper leggere MKV/HEVC/4K. Deve saper **chiedere** a
Jellyfin un H.264 1080p + AAC in HLS e **consumarlo**. Questo taglia il 40% del lavoro
e quasi tutti i bug di compatibilità.

---

## 2. Perché il client ufficiale non si può usare

Jellyfin Media Player (e Jellyfin Web/Vue) sono **jellyfin-web dentro Chromium**
(QtWebEngine / browser). Su un single-core 1 GHz con 256 MB di RAM non è "lento":
non parte. Stessa cosa per webOS/Tizen/Roku: sono runtime web o piattaforme diverse.

---

## 3. Rassegna dei client esistenti

| Client | Stack | Portabile sul Link? |
|---|---|---|
| Jellyfin Media Player | C++ + QtWebEngine (Chromium) | **No**: è Chromium |
| Jellyfin Web / Vue | JavaScript | **No**: serve un browser |
| Jellyfin Android / Android TV (Findroid, Wholphin, Void) | Kotlin + Media3 | **No**: Android, non Linux/glibc |
| Jellyfin iOS / tvOS | Swift | **No** |
| Jellyfin Roku / Tizen / webOS | BrightScript / web | **No** |
| Streamyfin / Fladder | React Native / Flutter | **No**: runtime pesante, target mobile/desktop |
| Jellyfin for Kodi / JellyCon | Python (addon) | **Sì, ma dentro Kodi**: Python 2 su 1 GHz è il collo di bottiglia |
| Kodi (port garbear) | C++ | **Sì, già fatto**: ma fermo al 2019, UI pesante, A/V sync mai risolto |
| jellyfin-mpv-shim | Python + mpv | **No**: Python 3 + mpv da portare, e mpv non ha un output video per SLVideo |

**Conclusione della rassegna**: non esiste un client Jellyfin leggero e nativo per
Linux ARMv7. L'unico che gira su questo hardware è Kodi, ed è il motivo per cui
esiste questo progetto.

---

## 4. La scoperta utile: Moonlight

Moonlight (game streaming) ha un'app **nativa ufficiale per lo Steam Link hardware**,
mantenuta, con lo stesso identico problema: ricevere H.264, decodificarlo in hardware,
mostrare una UI e leggere il controller su un box da 1 GHz.

Dal suo codice (GPLv3, `moonlight-stream/moonlight-qt`) si impara la ricetta esatta:

| Lezione | Dettaglio |
|---|---|
| Build | `qmake` + Qt 5.14.1 dello SDK, mkspec `devices/linux-marvell-g++` |
| UI | Qt Quick su **EGLFS** (plugin `eglfs`, integrazione Vivante) — niente X, niente Wayland |
| Video | `SLVideo_CreateStream(..., bLowLatency=1)` per I/P, `SetStreamTargetFramerate`, BT.709 |
| Overlay | `SLVideo_CreateOverlay` + `SetOverlayDisplayArea` (ARGB) per OSD/sottotitoli |
| Audio | `SLAudio`, blocchi PCM, coda massima ~40 ms per stereo |
| Launcher | `HOME` persistente, `renice` del processo `PE_Single_CPU`, `nice -10`, log in `/tmp` |
| Limiti | max 1920×1080, niente HDR, solo H.264 |

Questo **elimina le due incognite più grosse** del progetto: che Qt Quick vada bene
su questa GPU, e che l'integrazione video/audio sia possibile in un'app di terzi.
Non è teoria: è un'app che la gente installa dalla chiavetta.

Nota: Moonlight non è un client Jellyfin e non lo diventerà — il suo protocollo è
GameStream. Si riusa la **piattaforma**, non il prodotto.

---

## 5. Decisione

**App nostra, in C++/Qt Quick, costruita sulla ricetta di Moonlight.** Non un fork:
il livello protocollo è totalmente diverso (HLS vs GameStream) e un fork
significherebbe smontare più codice di quanto se ne scriva.

Licenza: il nostro codice resta MIT. Le API `SLVideo`/`SLAudio` sono zlib (header
allegati allo SDK). Se in futuro copiassimo codice da Moonlight, l'app dovrebbe
diventare GPLv3: preferiamo riscrivere le ~100 righe di glue, che sono documentate
dagli header.

### Stack

```
┌─────────────────────────── app (ARMv7, ~3 MB) ───────────────────────────┐
│ Qt Quick (EGLFS)        griglia, dettaglio, OSD, impostazioni            │
│ Qt Network              HTTPS verso Jellyfin, immagini, HLS              │
│ ApiClient (C++)         login / Quick Connect / Items / PlaybackInfo     │
│ HlsClient (C++)         master.m3u8 → variante → segmenti .ts            │
│ TsDemux (C++)           PAT/PMT → PES → H.264 Annex-B + AAC ADTS         │
│ AacDecode               libavcodec minimale (solo decoder AAC)           │
│ SLVideo                 decode H.264 hardware + overlay ARGB             │
│ SLAudio                 PCM 16 bit 48 kHz                                │
└──────────────────────────────────────────────────────────────────────────┘
```

Dipendenze già presenti nel firmware: Qt 5.14.1 (Quick, Qml, Network, Gui),
OpenSSL/GnuTLS, freetype, Noto Sans, SDL2 (per il controller), libSLVideo, libSLAudio.
**Unica cosa da compilare**: un `libavcodec` ridotto al solo decoder AAC
(`--disable-everything --enable-decoder=aac,aac_latm --enable-parser=aac --enable-small`),
perché il firmware non include ffmpeg.

---

## 6. Flusso utente

1. **Primo avvio**: schermata server → indirizzo (es. `http://192.168.1.10:8096`).
2. **Login senza digitare**: Quick Connect. L'app mostra un codice di 6 cifre;
   lo inserisci dal telefono/PC su Jellyfin e sei dentro. Nessuna password col
   telecomando: su una console è la differenza tra usabile e inusabile.
3. **Home**: "Continua a guardare", "Aggiunti di recente", le librerie (Film, Serie).
4. **Libreria**: griglia di copertine con scorrimento; A apre il dettaglio.
5. **Dettaglio**: trama, anno, durata, tracce; A = riproduci / riprendi.
6. **Player**: video a schermo intero, OSD con pausa/seek/tracce/sottotitoli,
   B = indietro, riprende dal punto salvato.

Navigazione con D-pad + A/B; niente mouse, niente tastiera.

---

## 7. Milestone

| # | Contenuto | Stato | Gate |
|---|---|---|---|
| M1 | decode + audio + sync da file locale | **codice fatto**, da misurare sul Link | drift < 40 ms, zero frame persi |
| M2 | HLS di Jellyfin: playlist, segmenti TS, AAC | da fare | 10 minuti di film senza crash |
| M3 | shell Qt Quick + griglia + EGLFS | **benchmark fatto**, da misurare | ≥ 30 fps, idealmente 60 |
| M4 | API Jellyfin: login, viste, immagini, ripresa | da fare | libreria navigabile |
| M5 | player integrato: seek, tracce, sottotitoli, OSD | da fare | usabile da divano |
| M6 | robustezza: rete, errori, cache, impostazioni, autostart | da fare | 2 h senza interventi |

Stima onesta: **M1–M5 ≈ 100–150 h**; prodotto rifinito (M6 + dettagli) ≈ 250 h.
Le due milestone che possono far fallire tutto sono M1 e M3, ed entrambe hanno un
test pronto da mettere sulla chiavetta.

---

## 8. Rischi residui

| Rischio | Probabilità | Mitigazione |
|---|---|---|
| Qt Quick lento sulla GPU Vivante | media | benchmark M3; fallback: UI in GLES2 a mano (più codice, meno RAM) |
| A/V sync instabile con B-frame | media | `bLowLatency=0` + clock audio master + coda video controllata (già in M1) |
| AAC decoder: build incrociata ffmpeg | bassa | build script dedicato, solo decoder, statico |
| RAM (256 MB) con Qt + cache copertine | media | cache su disco, poster a 300 px, massimo 20 item in memoria |
| Sottotitoli: overlay unico alla volta | bassa | burn-in lato server per PGS/ASS, overlay solo per SRT |

---

## Riferimenti

- SDK: <https://github.com/ValveSoftware/steamlink-sdk>
- Moonlight per Steam Link: <https://github.com/moonlight-stream/moonlight-qt>
  (`app/streaming/video/slvid.cpp`, `app/streaming/audio/renderers/slaud.cpp`,
  `scripts/build-steamlink-app.sh`, `app/deploy/steamlink/`)
- Port Kodi: <https://github.com/garbear/kodi-steamlink>
- API Jellyfin: <https://api.jellyfin.org>
