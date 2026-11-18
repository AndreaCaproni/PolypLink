# FFmpeg 6.1.2 — source offer

PolypLink ships the following FFmpeg shared libraries, built from unmodified
upstream FFmpeg 6.1.2:

- `libavcodec.so.60`
- `libavutil.so.58`
- `libswresample.so.4`

They are dynamically linked and used only to decode AAC audio and convert it to
PCM.

## Source code

The complete corresponding source code is the upstream FFmpeg 6.1.2 release:

    https://ffmpeg.org/releases/ffmpeg-6.1.2.tar.xz

SHA-256:

    3b624649725ecdc565c903ca6643d41f33bd49239922e45c9b1442c63dca4e38

The build configuration is recorded in `docs/FFMPEG.md` in the source
repository.

## Written offer

For at least three years from the date of the PolypLink release that included
these binaries, the maintainer will provide the complete corresponding source
code on request. Open an issue at:

    https://github.com/AndreaCaproni/PolypLink/issues

## License

FFmpeg is Copyright (c) 2000-2024 the FFmpeg developers and is licensed under
the GNU Lesser General Public License, version 2.1 or later. The full license
text is in `FFMPEG-LGPL-2.1.txt`.
