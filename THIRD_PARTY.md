# Third-party components

## FFmpeg (LGPL 2.1 or later)

The client ships a reduced FFmpeg 6.1.2 build used **only to decode AAC audio**
and convert it to PCM:

- `third_party/ffmpeg/lib/libavcodec.so` (60)
- `third_party/ffmpeg/lib/libavutil.so` (58)
- `third_party/ffmpeg/lib/libswresample.so` (4)

They are dynamically linked libraries without GPL components. FFmpeg is
Copyright (c) 2000-2024 the FFmpeg developers and licensed under the GNU Lesser
General Public License, version 2.1 or later. The license text is in
`third_party/ffmpeg/LICENSE.LGPLv2.1`.

Exact source, SHA-256 and the written source offer are in
`FFMPEG-SOURCE-OFFER.md` (in release packages it is next to this file; in the
source repository it is under `packaging/licenses/`). The committed library
checksums are in `third_party/ffmpeg/SHA256SUMS`. The build recipe is in
`docs/FFMPEG.md` in the source repository.

## Valve Steam Link SDK

This project does **not** redistribute Valve's SDK. Building requires the
toolchain, Qt 5.14.1 and the `SLVideo`/`SLAudio` libraries provided by the SDK
and present in the device firmware. All rights remain with Valve.

## Qt 5.14.1

Provided by the Link firmware and not redistributed by PolypLink. The app uses
Qt Quick, Qt Network and Qt Gui under the license Qt is distributed with in the
firmware (LGPL/GPL/commercial). Check the SDK's license files before
redistributing Qt yourself.

## SDL2

Provided by the Link firmware and not redistributed by PolypLink. It is used
only to read the controller. SDL2 is licensed under the zlib license.

## Jellyfin

Jellyfin: <https://jellyfin.org>

PolypLink is an independent project, not affiliated with the Jellyfin project.
It uses Jellyfin's public HTTP API only; no Jellyfin source code is included.
See [PROVENANCE.md](PROVENANCE.md).

## Fonts

The UI uses the firmware's system fonts (Noto Sans). No fonts are
redistributed.
