<h1 align="center">
  <img src="docs/images/polyplink-logo.svg" width="64" alt="PolypLink logo" align="middle">
  PolypLink
</h1>

> English: [README.md](README.md)

[![checks](https://github.com/AndreaCaproni/PolypLink/actions/workflows/check.yml/badge.svg)](https://github.com/AndreaCaproni/PolypLink/actions/workflows/check.yml)
[![release](https://img.shields.io/github/v/release/AndreaCaproni/PolypLink?include_prereleases&label=release)](https://github.com/AndreaCaproni/PolypLink/releases)
[![license](https://img.shields.io/github/license/AndreaCaproni/PolypLink)](LICENSE)
![platform](https://img.shields.io/badge/platform-Steam%20Link%202015-1b2838)

**Client Jellyfin nativo per Valve Steam Link (2015).**
Niente Chromium, niente Electron, niente browser: un'app Qt Quick/C++ che parla
direttamente con il decoder hardware del Link.

> **Stato: beta 0.4.2** — già usabile per guardare film e serie, ancora in
> rifinitura. Progetto indipendente, non affiliato al progetto Jellyfin.

## Screenshot

| Home | Dettaglio |
|:---:|:---:|
| ![Home](docs/images/screenshot-home.png) | ![Dettaglio](docs/images/screenshot-detail.png) |
| **Player OSD** | **Impostazioni** |
| ![Player OSD](docs/images/screenshot-player.png) | ![Impostazioni](docs/images/screenshot-settings.png) |

[Altri screenshot](docs/SCREENSHOTS.md)

---

## Perché esiste

I client ufficiali (Jellyfin Media Player, Jellyfin Web) sono **jellyfin-web
dentro Chromium**. Sul Link — single-core, 256 MB di RAM — non partono.

PolypLink usa l'SDK Valve e le API `SLVideo`/`SLAudio` del firmware per
decodificare H.264 in hardware e riprodurre l'audio AAC. Il server fa il lavoro
pesante: chiediamo **sempre** una transcodifica H.264 + AAC in HLS, così il box
non deve saper leggere MKV, HEVC, 4K o HDR.

---

## Cosa sa fare

- **Home** con *Continua a guardare*, *Prossimo episodio*, *Aggiunti di
  recente* (con la libreria di provenienza) e le librerie.
- **Libreria completa**: film, serie, stagioni, episodi; scheda con trama,
  anno, durata, voto, visto/preferito.
- **Serie e stagioni**: *Riproduci* (prossimo episodio), *Stagioni*,
  visto/preferito; pagina stagione con pulsanti, trama ed elenco episodi.
- **Ripresa**: *Riprendi* e *Riproduci dall'inizio*, posizione sincronizzata
  col server.
- **Player** con OSD navigabile: barra di seek con cursore e anteprima del
  tempo, passo accelerato fino a 3 minuti, pausa, -10/+30 secondi, traccia
  audio, sottotitoli, qualità con menu dedicati, offset del ritardo audio,
  statistiche, cartello connessione persa e scheda prossimo episodio.
- **Accesso**: ricerca automatica del server (UDP Jellyfin + scansione della
  LAN), indirizzo manuale, utente/password con tastiera a schermo, Quick
  Connect.
- **Preferiti** come scheda nella barra superiore.
- **Errori di rete chiari**: timeout, *server non raggiungibile* con **A** per
  riprovare, *connessione persa* durante la riproduzione.
- **Account** e **Impostazioni**: statistiche account e impostazioni reali per
  qualità predefinita, durata OSD e ritardo audio; le altre voci sono segnaposto
  per la versione stabile.
- **Interfaccia in 21 lingue**, selezionabili dalle Impostazioni e memorizzate
  tra un avvio e l'altro.

---

## Roadmap

Verso una 1.0 stabile, in ordine indicativo:

- **Ricerca** tra film, serie ed episodi.
- **Righe della home**: mostra/nascondi e riordina.
- **Autoplay del prossimo episodio** usando la scheda countdown già presente.
- **Trickplay**: miniature sulla barra di seek.
- **Seek in-stream** senza riavviare la transcodifica.
- **Completare le impostazioni segnaposto** e le restanti voci dell'Account.
- **Revisione madrelingua** delle 21 lingue.
- Test più ampi su server e hardware prima della 1.0.

Completato di recente nella serie 0.4.x: contesti di traduzione QML corretti,
bundle CA Mozilla, ritorno alla lista server con **B**, split dei controller,
irrobustimento della riproduzione e screenshot reali dal demo.

---

## Requisiti

- Valve **Steam Link** (hardware 2015) con firmware aggiornato.
- Un server **Jellyfin** raggiungibile in rete locale (testato con Jellyfin
  12.4).
- **La transcodifica DEVE essere abilitata sul server Jellyfin.** PolypLink
  chiede sempre una transcodifica H.264 + AAC in HLS e non usa il direct play;
  senza transcodifica lato server i video non partono.
- Una **chiavetta USB** per l'installazione.
- Per compilare: **Steam Link SDK** (Qt 5.14.1 + toolchain ARM), non incluso
  nel repository.

---

## Sicurezza

- **HTTPS è raccomandato.** Il client verifica i certificati TLS e rifiuta
  quelli self-signed; usa un certificato valido o un reverse proxy. HTTP
  funziona ancora in una LAN fidata, ma credenziali e token di riproduzione
  viaggiano senza cifratura. Se il server usa HTTP verso un host che non è il
  loopback, la schermata di accesso mostra un avviso esplicito.
- Gli indirizzi inseriti a mano usano **HTTPS di default** per gli host
  pubblici e HTTP solo per host privati/LAN.
- **La scoperta non è autenticata.** Qualsiasi dispositivo sulla LAN può
  falsificare una risposta UDP di Jellyfin. L'app ignora l'host annunciato e
  usa l'IP del mittente, e mostra sempre l'indirizzo completo; resta comunque
  necessario accedere solo a server riconosciuti. La scansione parallela prova
  la porta 8096 sulla `/24` locale (serve per Docker/bridge).
- Il token Jellyfin è salvato nella configurazione con permessi solo-proprietario
  (`0600`); i log, inclusi i warning QML, vengono redatti prima di essere
  scritti su stderr o nel log del dispositivo.
- Il logout chiede a Jellyfin di revocare la sessione lato server, ma è
  best-effort: se il server non è raggiungibile o l'app viene chiusa subito, la
  sessione può restare attiva finché non viene revocata dalla dashboard di
  Jellyfin.
- Le vecchie direttive `user=`/`pass=` in `jellyfin-server.txt` non sono più
  supportate. L'accesso si fa dall'app o con Quick Connect.
- Un kit creato con `--dev-ssh` è un artefatto di sviluppo: include
  `factory_test/run.sh`, che autorizza la chiave SSH personale per `root` e
  `steam` e reimposta la password root a `steamlink123`. Non condividerlo mai;
  tieni le chiavi private fuori dal repository e dagli archivi di release.

---

## Installazione

La guida passo-passo è in **[docs/INSTALL.md](docs/INSTALL.md)**.
In breve: copia la cartella `steamlink/` nella radice della chiavetta, inserisci
la chiavetta con il Link spento e accendilo. Dopo 1–2 minuti l'app compare
nella home.

Il pacchetto si crea con:

```bash
./build.sh                        # compila e prepara dist/steamlink/
./build.sh --install /media/USB   # compila e copia direttamente sulla chiavetta
./build.sh --dev-ssh              # include l'hook SSH personale (non condividerlo)
```

Asset di release (zip USB, tgz e `SHA256SUMS`):

```bash
./scripts/package-release.sh
```

---

## Comandi

Elenco completo in **[docs/CONTROLS.md](docs/CONTROLS.md)**. In sintesi:

| Dove | Comando |
|---|---|
| Home | frecce per muoversi, **A** apre, **B** indietro, **su** dalla prima riga va alla barra superiore |
| Barra superiore | sinistra/destra scelgono Home/Preferiti/Account/Impostazioni, **A** apre, **giù** torna al contenuto |
| Player | **A** conferma/seleziona, **sinistra/destra** spostano il cursore, **su** nasconde l'OSD, **giù** va ai pulsanti, **B** nasconde/esce, **Start** statistiche |

---

## Come funziona

```
Server Jellyfin                       Steam Link
───────────────                       ──────────
mkv / hevc / 4k  ──►  ffmpeg  ──►  HLS (H.264 + AAC)  ──►  demux TS
                                                          ├─► SLVideo (H.264 HW)
                                                          └─► AAC → SLAudio
```

- Il client costruisce una `DeviceProfile` che obbliga H.264/AAC in HLS e
  disattiva il copia-incolla dei flussi (`AllowVideoStreamCopy=false`).
- Il player scarica i segmenti del playlist, ricostruisce gli access unit
  H.264 dai NAL, li passa a `SLVideo`, decodifica l'AAC con una build minima di
  FFmpeg e sincronizza l'audio con `SLAudio`.
- La posizione di seek non si chiede con `StartTimeTicks`: il player parte dal
  segmento HLS che copre quel tempo e il server riavvia la transcodifica lì.

Dettagli e scelte tecniche: **[docs/ARCHITETTURA.md](docs/ARCHITETTURA.md)**.

---

## Compilare

Serve lo **Steam Link SDK** di Valve (non incluso). Di default il build lo
cerca in `../steamlink-sdk`, oppure:

```bash
./build.sh --sdk ~/steamlink-sdk
```

Requisiti: `make`, `qmake` dall'SDK, toolchain ARM dell'SDK.

Le librerie FFmpeg per l'AAC sono già incluse in `third_party/ffmpeg/`; la
ricetta per ricompilarle è in **[docs/FFMPEG.md](docs/FFMPEG.md)**.

Controllo rapido di QML e script:

```bash
./scripts/check.sh
```

---

## Struttura del progetto

```
src/            C++: controller UI, client API, player HLS, demux TS
qml/            interfaccia Qt Quick (1280x720 virtuale, scalata)
deploy/         launcher e hook di boot per la chiavetta
packaging/      icona dell'app
third_party/    FFmpeg minimale (solo AAC) + licenza LGPL
docs/           installazione, comandi, architettura, ffmpeg
build.sh        build + pacchetto USB
```

---

## Limiti noti della beta

- **Seek e cambio impostazioni riavviano il flusso** (~2 secondi): il passo
  successivo è farlo dentro il player.
- **Sottotitoli solo in burn-in** (incisi dal server), nessun sottotitolo
  esterno o stilizzato.
- **I certificati HTTPS self-signed non sono accettati.** Usa un certificato
  valido o un reverse proxy; altrimenti HTTP è limitato a una LAN fidata.
- **Niente ricerca**, niente coda di riproduzione, niente autoplay del
  prossimo episodio, niente miniature trickplay sulla barra.
- Paginazione libreria limitata a 300 elementi per vista.
- Solo **H.264 8-bit**: niente HEVC, HDR, AV1; il Link decodifica solo quello.
- **La qualità arriva fino a 100 Mbps** come esperimento. Il Link ha Ethernet
  100 Mbit, quindi 80–100 Mbps possono saturare il collegamento; sopra ~40 Mbps
  il guadagno visivo è minimo e gli scatti diventano probabili.
- Avviando un episodio dalla pagina serie/stagione il pannello **Audio** usa la
  traccia predefinita del server; le tracce dell'episodio si caricano
  aprendone la scheda.
- La ricerca automatica del server copre la **/24 locale**; server su altre
  VLAN si inseriscono a mano.
- **Le richieste API e del player autenticate non seguono i redirect.** Un
  reverse proxy deve servire direttamente l'URL finale: un redirect HTTP→HTTPS
  sulle richieste API o di riproduzione non viene seguito, così il token non
  può essere inoltrato a un altro host.
- **Interfaccia in 21 lingue**: italiano, inglese, spagnolo, francese, tedesco,
  portoghese (Brasile e Portogallo), russo, olandese, polacco, svedese, danese,
  norvegese bokmål, ceco, ucraino, catalano, turco, rumeno, greco, ungherese e
  finlandese. La lingua si cambia dalle Impostazioni e resta memorizzata tra un
  avvio e l'altro; al primo avvio segue la lingua di sistema, con l'inglese come
  fallback. Le traduzioni sono un primo passaggio, non revisionate da
  madrelingua.
- **Titoli, trame e generi seguono la lingua dei metadati del server**, impostata
  per libreria in Jellyfin. La lingua dell'interfaccia non traduce i metadati.

---

## Sviluppo con AI

PolypLink è sviluppato con un uso esteso di assistenti di programmazione AI per
codice, documentazione, traduzioni e revisioni. Ogni modifica viene compilata,
provata su hardware Steam Link reale e rivista prima di essere pubblicata;
l'output AI non viene pubblicato senza controllo. Le parti sensibili alla
sicurezza sono state inoltre sottoposte ad audit, con i problemi corretti prima
della beta.

## Note di sviluppo

Costruito e curato su hardware Steam Link reale. È una beta: aspettati qualche
spigolo e allega log e dettagli hardware nelle segnalazioni.

Nota tecnica: le richieste delle immagini QML ricevono l'header `Authorization`
di Jellyfin solo se l'URL coincide con schema, host e porta del server. Oggi
tutti gli URL delle immagini sono generati dall'app; una CDN o un host esterno
avrebbe bisogno di un proprio percorso di autenticazione.

## Contribuire

Questo è il repository canonico. Bug report e pull request sono benvenuti qui:
meglio mandare i miglioramenti upstream che pubblicare fork separati.

---

## Licenza

Codice: **MIT**, vedi [LICENSE](LICENSE).

Provenienza e licenze di terze parti: [PROVENANCE.md](PROVENANCE.md) e
[THIRD_PARTY.md](THIRD_PARTY.md) (FFmpeg LGPL 2.1+, SDK Valve, Qt).

I pacchetti di release includono una cartella `licenses/` con licenza MIT,
testo LGPL di FFmpeg, offerta della sorgente FFmpeg e note di terze parti.

Jellyfin: <https://jellyfin.org>. PolypLink è un progetto indipendente, non
affiliato al progetto Jellyfin.
