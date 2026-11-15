# Componenti di terze parti

## FFmpeg (LGPL 2.1 o successiva)

Il client include una build ridotta di FFmpeg usata **solo per decodificare
l'audio AAC** e convertirlo in PCM:

- `third_party/ffmpeg/lib/libavcodec.so` (60)
- `third_party/ffmpeg/lib/libavutil.so` (58)
- `third_party/ffmpeg/lib/libswresample.so` (4)

Sono librerie dinamiche, senza componenti GPL. Il testo della licenza è in
`third_party/ffmpeg/LICENSE.LGPLv2.1`. Sorgenti: <https://ffmpeg.org/>.
La ricetta di compilazione è in [docs/FFMPEG.md](docs/FFMPEG.md).

## Valve Steam Link SDK

Il progetto **non redistribuisce** l'SDK Valve. Per compilare servono
toolchain, Qt 5.14.1 e le librerie `SLVideo`/`SLAudio` fornite dall'SDK e
presenti nel firmware del dispositivo. I diritti restano di Valve.

## Qt 5.14.1

Fornito dal firmware del Link. JellyLink usa Qt Quick, Qt Network e Qt Gui
secondo la licenza con cui Qt è distribuito nel firmware (LGPL/GPL/commerciale).

## Jellyfin: nome e logo

Il nome *Jellyfin* e il logo sono marchi del progetto Jellyfin. Sono usati qui
solo per indicare che JellyLink è un client compatibile con Jellyfin.
**JellyLink è un progetto indipendente, non affiliato né approvato dal progetto
Jellyfin.**

## Font

L'interfaccia usa i font di sistema del firmware (Noto Sans). Nessun font è
ridistribuito.
