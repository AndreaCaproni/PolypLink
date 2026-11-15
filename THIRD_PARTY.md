# Third-party components

## FFmpeg (LGPL 2.1 or later)

The client ships a reduced FFmpeg build used **only to decode AAC audio** and
convert it to PCM:

- `third_party/ffmpeg/lib/libavcodec.so` (60)
- `third_party/ffmpeg/lib/libavutil.so` (58)
- `third_party/ffmpeg/lib/libswresample.so` (4)

They are dynamically linked libraries without GPL components. The license text
is in `third_party/ffmpeg/LICENSE.LGPLv2.1`. Sources:
<https://ffmpeg.org/>. The build recipe is in [FFMPEG.md](FFMPEG.md).

## Valve Steam Link SDK

This project does **not** redistribute Valve's SDK. Building requires the
toolchain, Qt 5.14.1 and the `SLVideo`/`SLAudio` libraries provided by the SDK
and present in the device firmware. All rights remain with Valve.

## Qt 5.14.1

Provided by the Link firmware. JellyLink uses Qt Quick, Qt Network and Qt Gui
under the license Qt is distributed with in the firmware (LGPL/GPL/commercial).

## Jellyfin name and logo

The *Jellyfin* name and logo are trademarks of the Jellyfin project. They are
used here only to indicate that JellyLink is a Jellyfin-compatible client and
are not JellyLink's own branding. No endorsement is implied. **JellyLink is an
independent, unofficial project, not affiliated with, endorsed by, sponsored
by, or supported by the Jellyfin project.** If the Jellyfin project requests a
change or removal, the project will comply.

## Fonts

The UI uses the firmware's system fonts (Noto Sans). No fonts are
redistributed.
