# FFmpeg minimale (solo AAC)

## Perché

Il firmware del Link non include FFmpeg. L'unica cosa che serve al client è
**decodificare l'audio AAC** e convertirlo in PCM per `SLAudio`; il video lo
decodifica `SLVideo` in hardware. Quindi la build è ridotta all'osso.

Le librerie già compilate sono in `third_party/ffmpeg/lib/` e vengono incluse
nel pacchetto con il nome SONAME richiesto dal linker (`libavcodec.so.60`,
`libavutil.so.58`, `libswresample.so.4`). **Non serve ricompilarle** per usare
il repository.

## Versione

FFmpeg 6.1.x (libavcodec 60, libavutil 58, libswresample 4).

## Ricetta usata

Con lo Steam Link SDK caricato (`source setenv.sh`) e `MARVELL_ROOTFS` puntato
alla rootfs ARM:

```bash
./configure \
  --enable-cross-compile \
  --arch=arm \
  --target-os=linux \
  --cross-prefix=armv7a-cros-linux-gnueabi- \
  --sysroot="$MARVELL_ROOTFS" \
  --enable-shared --disable-static \
  --disable-programs --disable-doc \
  --disable-avdevice --disable-swscale --disable-avfilter \
  --disable-network --disable-encoders --disable-muxers \
  --disable-demuxers --disable-filters \
  --disable-everything \
  --enable-decoder=aac,aac_latm \
  --enable-parser=aac \
  --enable-small \
  --extra-cflags="-mfloat-abi=hard -O2" \
  --extra-ldflags="-mfloat-abi=hard"

make -j"$(nproc)"
```

Poi copia le librerie in `third_party/ffmpeg/lib/` e gli header in
`third_party/ffmpeg/include/`.

## Licenza

FFmpeg è distribuito con licenza **LGPL 2.1 o successiva**. La build qui
inclusa non abilita componenti GPL. Il testo della licenza è in
`third_party/ffmpeg/LICENSE.LGPLv2.1`; i sorgenti sono su
<https://ffmpeg.org/>.
