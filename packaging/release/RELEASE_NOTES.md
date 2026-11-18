PolypLink @VERSION@ — a native Jellyfin client for the Valve Steam Link (2015).

No Chromium: Qt Quick/C++ with hardware H.264 decoding (SLVideo) and AAC audio
(SLAudio). The server transcodes to H.264/AAC over HLS.

This is a **hardening release** on top of 0.3.0: security, tests, migration and
packaging. The PolypLink rebrand and the player work from 0.3.0 are included.

## Security

- `ApiKey`, `token`, `access_token` and `Authorization` values are redacted from
  logs and stream URLs.
- Logs on the USB drive no longer expose the Jellyfin API key. Redaction now
  also covers Qt/QML warnings, including image-loading errors.
- The settings file and directory are owner-only (`0600`/`0700`).
- Plaintext `user=`/`pass=` support in `jellyfin-server.txt` was removed.
- Manual server entry uses HTTPS for public hostnames; HTTP is for trusted LANs
  only and is logged as insecure.
- FFmpeg source offer and copyright notice are included for LGPL compliance.

## Reliability and packaging

- GitHub Actions checks: QML lint, translation completeness, settings migration
  and log-redaction tests.
- Settings migration from JellyLink to PolypLink is covered by an automated
  test and shipped in the package.
- `scripts/package-release.sh` builds the USB kit, the zip/tgz and
  `SHA256SUMS`.
- MIT, FFmpeg LGPL and third-party notices are included in the app package and
  in the USB zip under `licenses/`.
- Clean `-Wall -Wextra` build.

## Included from 0.3.0

- **PolypLink rebrand** with new logos, package and binary name; existing
  settings are migrated on first start.
- **4K sources are capped to 1920×1080** in the transcode, fixing the SLVideo
  *stream macroblock error* caused by 2560×1072 frames.
- **Bitrate maximum** selectable from a list: 4, 8, 12, 20, 30, 40, 50, 60, 80,
  100 Mbps (default 12).
- **OSD fade** selectable: 3, 5 or 8 seconds.
- Dedicated dropdown menus for audio, subtitles and quality.
- Connection-lost card after 10 seconds; next-episode card in the last minute.
- Seek confirmed only with A; acceleration only while holding the stick, up to
  3 minutes.
- The 21-language interface is included.

## Install

1. Download `polyplink-@VERSION@-usb.zip` and extract it.
2. Copy the `steamlink/` folder to the **root** of a FAT32 USB drive.
3. Power the Link off, insert the drive, power it on.
4. After 1–2 minutes, **PolypLink** appears on the home screen.

On first start, enter the server address (or create
`steamlink/config/system/jellyfin-server.txt`) and sign in with
username/password or Quick Connect.

## Assets

- `polyplink-@VERSION@-usb.zip` — ready-to-copy USB kit (Italian and English
  instructions, no personal data).
- `polyplink-@VERSION@.tgz` — the app package only.
- `SHA256SUMS` — checksums for both assets.

## Known limitations

- Transcoding must be enabled on the Jellyfin server.
- Seeking and changing tracks restart the stream (~2 s).
- Burn-in subtitles only.
- No search, playback queue, next-episode autoplay or trickplay thumbnails.
- Some Settings/Account entries are placeholders.
- H.264 8-bit only; item metadata stays in the server's language.
- Bitrates above ~40 Mbps are experimental; 80–100 Mbps can saturate the
  Link's 100 Mbit Ethernet.

Full list in the README.

## Notes

Code: MIT. FFmpeg: LGPL 2.1+ (see `licenses/`). Translations are original
machine-assisted work. Not affiliated with the Jellyfin project or Valve.
