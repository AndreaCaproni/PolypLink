PolypLink @VERSION@ — a native Jellyfin client for the Valve Steam Link (2015).

No Chromium: Qt Quick/C++ with hardware H.264 decoding (SLVideo) and AAC audio
(SLAudio). The server transcodes to H.264/AAC over HLS.

This is **0.4.3**: it adds physical-keyboard navigation, media keys and a
shared input layer for the keyboard and controller. Enter/Select/Space confirm,
Esc/Backspace go back, the numeric keypad works as arrows, and holding a
direction no longer runs through menus. It also fixes the false "connection
lost" banner at the normal end of a stream and includes the 0.4.2 translation
context/avatar fixes and real demo screenshots, the 0.4.1 CA bundle and
back-to-server-list fix, the 0.4.0 refactor and playback hardening, the 0.3.3
licensing/display fixes, and the 0.3.2 security fixes. The PolypLink rebrand
and the player work from 0.3.0 are included.

## Input

- USB keyboard: arrows, numeric keypad, **Enter/Select/Space** to confirm,
  **Esc/Backspace** to go back. WASD is intentionally not mapped.
- Media keys: **play/pause**, **fast forward** (+30 s) and **rewind** (-10 s).
- Keyboard and controller share one C++ player-input implementation.
- Navigation is atomic: auto-repeat is ignored outside the seek bar and the
  audio-delay slider.
- The numeric keypad uses `KeypadModifier`; the mapping has automated tests.

## Refactor

- `AppController` is now a facade over `AuthController`, `LibraryController`,
  `PlaybackController` and `SettingsController`.
- Shared `InputMap` module and `handlePlayerKey`; seek limits and stall checks
  are centralized.
- Player OSD and season actions use named action IDs.
- Source comments are English; shared translation/quality/OSD constants.

## Security

- The player follows redirects manually only within the same host (HTTP ->
  HTTPS upgrade allowed), rejects cross-host playlist/segment URLs, and strips
  every common token query key before logging or requesting.
- Playlist/segment/PES/frame/PCM buffers have hard limits; PTS-derived silence
  is bounded; FFmpeg allocations and integer sizes are checked.
- Player state shared with the network thread is synchronized; a QThread is
  deleted only after join, and a stuck player blocks new playback instead of
  overlapping a second decoder on the global audio context.
- The QML password is cleared after login, keyboard close and cancel;
  discovery has a 64-server cap and home libraries a 32-library cap.
- Release assets are reproducible and pass an expanded secret/IP/token gate;
  CI checks git history for private keys and sensitive paths.
- The TS demuxer validates the PES header before parsing PTS/DTS; truncated or
  malformed packets are rejected instead of reading past the buffer.
- Jellyfin UDP discovery no longer trusts the advertised host: the app uses the
  IP that sent the datagram and rejects non-HTTP(S) schemes, userinfo and
  query/fragment suffixes. The full server address is always shown.
- `ApiKey`, `token`, `access_token`, `secret`, `Pw`, `password`, JSON
  `AccessToken`/`Password` fields and playback errors returned by the server
  are redacted from logs. QML image-loading warnings are covered too.
- The sign-in screen warns when the server uses plain HTTP to a non-loopback
  host: password and token are not encrypted in that case.
- Authenticated API/player requests do not follow redirects, so the
  `Authorization` token cannot be forwarded to another host.
- The settings file and directory are owner-only (`0600`/`0700`).
- Plaintext `user=`/`pass=` support in `jellyfin-server.txt` was removed.
- The default USB kit contains no SSH hook, key or root password. The personal
  development hook requires `./build.sh --dev-ssh` and must not be shared.
- Manual server entry uses HTTPS for public hostnames; HTTP is for trusted LANs
  only and is shown as insecure in the UI.
- FFmpeg source offer and copyright notice are included for LGPL compliance.

## Reliability and packaging

- At the normal end of a stream, the player no longer reports a false
  "connection lost" while the last queued frames drain.
- QML components have correct per-component translation contexts, so the top
  bar **Favorites** tab and the sign-in, server, Quick Connect, keyboard and
  poster strings follow the selected language.
- The favorites page title is translated when the page opens; the profile
  initial is hidden when the avatar image is ready.
- The sign-in screen returns to server discovery with **B**; a translated hint
  is shown.
- A current Mozilla CA bundle is loaded in addition to the firmware roots, so
  modern HTTPS certificates (Let's Encrypt) work.
- Video queue overflow drops new frames instead of the oldest (keyframe) ones:
  black/corrupted/accelerated-looking playback is fixed.
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

## Documentation

- The README shows four real screenshots taken on a Steam Link against the
  public Jellyfin demo. The full eight-image gallery is in
  `docs/SCREENSHOTS.md`.

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

## Licensing

- No Moonlight source code is included. The Steam Link integration uses Valve's
  SDK headers and API; the display-plane workaround is an independent
  clean-room implementation built on the public `SLVideo` API.
- Jellyfin is used only through its public HTTP API. No Jellyfin server, web
  client or SDK source code is included.
- See `PROVENANCE.md` in the repository for details.

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
