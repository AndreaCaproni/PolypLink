# Minimal FFmpeg (AAC only)

## Why

The Link firmware does not include FFmpeg. The client only needs to **decode
AAC audio** and turn it into PCM for `SLAudio`; video is decoded by `SLVideo`
in hardware. The build is therefore cut down to the bone.

Prebuilt libraries live in `third_party/ffmpeg/lib/` and are included in the
package under the SONAME the dynamic linker expects (`libavcodec.so.60`,
`libavutil.so.58`, `libswresample.so.4`). **You do not need to rebuild them** to
use this repository.

## Version

FFmpeg 6.1.x (libavcodec 60, libavutil 58, libswresample 4).

## Recipe used

With the Steam Link SDK loaded (`source setenv.sh`) and `MARVELL_ROOTFS`
pointing at the ARM rootfs:

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

Then copy the libraries into `third_party/ffmpeg/lib/` and the headers into
`third_party/ffmpeg/include/`.

## License

FFmpeg is distributed under the **LGPL 2.1 or later**. The build included here
does not enable GPL components. The license text is in
`third_party/ffmpeg/LICENSE.LGPLv2.1`; sources are at <https://ffmpeg.org/>.
