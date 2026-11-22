# Changelog

## 0.4.0-beta — 2026-11-22

### Changed

- Split `AppController` (1,923 -> 977 lines) into `AuthController`,
  `LibraryController`, `PlaybackController` and `SettingsController`.
- Player OSD focus and season actions now use named action IDs instead of magic
  numbers. Source comments are English, with centralized translation, quality
  and OSD constants.
- Reproducible release builds: buildstamp, tar ownership/mtime and zip
  timestamps come from `SOURCE_DATE_EPOCH`; CI and scripts use unpredictable
  temporary paths.

### Security

- Player redirects are followed manually only for the same host (HTTP -> HTTPS
  upgrade allowed); cross-host redirects and playlist URLs are rejected, so the
  Authorization token cannot leak and the player cannot be used for SSRF.
- Token stripping covers all case variants of `api_key`/`apikey`/`api-key`,
  `access_token`, `token`, `secret`, `pw` and `password` without rewriting the
  rest of the query string.
- Hard limits for playlist/segment downloads, segment queue, PES/access units,
  video frames and PCM; PTS-derived silence is bounded; FFmpeg allocations and
  integer sizes are checked; non-finite framerates are rejected.
- Shared player state is protected by a mutex/atomics; a QThread is deleted
  only after a successful join; the 20 s stop timeout prevents a second player
  from starting while the old one is alive.
- The QML password is cleared after login, keyboard close and cancel; discovery
  and library lists have hard caps.
- The release gate checks more secrets and inspects the app `.tgz`; CI checks
  git history for private keys and sensitive paths; log documentation now
  matches the default kit.

### Fixed

- Video queue overflow no longer drops the oldest frames (and therefore
  keyframes): fixes black/corrupted video and accelerated-looking playback.
- Library/favorites grid navigation is row-aware: left/right stop at the row
  edges instead of wrapping to the adjacent row.
- Includes the 0.3.3 clean-room video-plane refresh/exit cleanup and all 0.3.2
  security fixes.

## 0.3.3-beta — 2026-11-22

### Changed

- Replaced the small overlay-flush workaround derived from Moonlight (GPLv3)
  with an independent clean-room implementation (`PlaneRefresh`) built only on
  the public `SLVideo` API. No Moonlight code is included. Added
  `PROVENANCE.md`; Jellyfin is used only through its public HTTP API.
- Translated all source comments to English.

### Fixed

- The video plane is refreshed when a stream closes: no more previous-frame
  flash between videos and no manual pause/play to start the new one.
- The video context is released before the app exits: no leftover player or
  black screen after leaving PolypLink.

## 0.3.2-beta — 2026-11-22

### Security

- Removed plaintext `user=`/`pass=` support from `jellyfin-server.txt`; old
  directives are ignored. Use the login screen or Quick Connect.
- The settings file and its directory are now owner-only (`0600`/`0700`), so
  the Jellyfin token is not readable by other local users/processes.
- All log messages, including QML warnings, are redacted centrally before they
  are written to stderr or the USB log.
- Manual server entry uses HTTPS by default for public hostnames; HTTP remains
  for private/LAN hosts and is marked as insecure in the log.
- Added the FFmpeg source offer and copyright notice for LGPL compliance.
- Quick Connect secrets are now redacted from error/timeout logs.
- Authenticated API/player requests no longer follow redirects, so the
  `Authorization` token cannot be forwarded to another host. Image requests
  use the header through the QML network manager; `api_key` was removed from
  image and HLS URLs.
- Logout now revokes the Jellyfin session server-side; changing server clears
  the old token before reuse.
- `./build.sh` no longer includes the personal SSH hook or enables SSH by
  default; use `--dev-ssh` for that kit.
- TS demuxer: a truncated PES with the PTS flag set no longer reads past the
  buffer; PTS/DTS header sizes are validated before parsing.
- Discovery: the host advertised in Jellyfin UDP replies is ignored and
  replaced with the datagram's source IP, so a spoofed reply cannot point the
  app at an arbitrary host. Non-HTTP(S) schemes, userinfo and query/fragment
  are rejected.
- Log redaction now covers `Pw=`, `password=`, JSON fields such as
  `"AccessToken": "..."` / `"Password": "..."` and playback errors returned by
  the server.
- The sign-in screen shows a warning when the server uses plain HTTP to a
  non-loopback host.
- Documentation now states explicitly that `--dev-ssh` sets the root password
  to `steamlink123` and installs the personal SSH key, and that the private key
  must stay outside the repository and release archives.

### Added

- Player: **Ritardo audio** popup after Quality and a corresponding default in
  **Settings → Ritardo audio**, with offsets from `-2000` to `+2000` ms in
  50 ms steps. The value is saved and applied at the next playback; negative
  values advance the audio, positive values delay it.

### Fixed

- The "server unreachable" screen now shows the controller hints
  **A · Retry** and **B · Exit app**.

### Changed

- Single-instance guard: a second Polyplink launch exits instead of competing
  for EGLFS and the controller; `run.sh` also uses `flock` so it cannot truncate
  the running instance's log.
- QML refactor: shared `Theme` singleton for colors/sizes, plus reusable
  `Toast` and `EmptyState` components. Hardcoded colors removed from the main
  view and components.
- Header bar and login flow (server selection, credentials, Quick Connect)
  extracted from `main.qml` into `HeaderBar`, `ServerPage`, `AuthPage` and
  `QuickConnectPage`.
- Home rows and the library/favorites grid extracted into `HomePage` and
  `LibraryPage`.
- Season and episode pages extracted into `SeasonsPage` and `EpisodesPage`.
- Top bar: library tabs after Favorites, in the same order as Jellyfin; Account
  and Settings follow the libraries.
- Settings and Account layouts extracted into a shared `MenuPage` component.
- Player OSD and the main overlays extracted into `PlayerPage`,
  `OverviewOverlay`, `SettingsChoiceOverlay` and `LanguageOverlay`.

## 0.3.1-beta — 2026-11-17

Hardening release: security, tests, migrations and packaging.

### Security

- Redact `ApiKey`, `token`, `access_token` and `Authorization` values from logs.

### Added

- GitHub Actions checks: QML lint, translation completeness, settings migration
  and log-redaction tests.
- `scripts/package-release.sh`: builds the USB kit, creates the release zip/tgz
  and writes `SHA256SUMS`.
- Settings migration extracted to `scripts/migrate-settings.sh` and covered by a
  test.
- License notices (MIT, FFmpeg LGPL, third-party) are now included in the app
  package and in the USB release zip.
- README with badges, screenshot placeholders, roadmap and development notes.

### Fixed

- Removed unused player variables/functions; the build is clean under
  `-Wall -Wextra` again.

## 0.3.0-beta — 2026-11-16

PolypLink rebrand and player tuning.

### Changed

- Renamed the app from JellyLink to **PolypLink**, with new logos.
- New package and binary name: `polyplink`. Existing settings are migrated on
  the first start.
- Home icon and header logo use the new PolypLink artwork (thin border).
- Settings: **Bitrate massimo** replaces the old quality entry and is chosen
  from a list (4–100 Mbps) instead of cycling.
- Settings: **Dissolvenza OSD** is chosen from a list (3/5/8 s).
- Removed the placeholder Settings entries *Risoluzione massima*, *Audio
  preferito*, *Precaricamento* and *Seek e accelerazione*.
- Removed *Ripresa*, *Prossimo episodio*, *Statistiche di visione* and the
  duplicate *Quick Connect* from Account.
- Placeholder entries now show **Funzionalità in arrivo**.
- Seek acceleration only starts while holding the analog stick; single taps
  always use the base step. Maximum acceleration is now 3 minutes.
- The player uses the selected bitrate for the decoder target; the stats panel
  shows the real bitrate.

### Fixed

- **4K sources**: the transcode is capped to 1920×1080, so the server no longer
  sends 2560×1072 frames that SLVideo rejected with *stream macroblock error*.

## 0.2.2-beta — 2026-11-16

Player OSD update.

### Added

- Visible **connection-lost card** after 10 seconds without playback
  progress, with *Resume* (A) and exit (B). The check is independent of the
  player's internal stall signal.
- **Next-episode card** in the last minute of an episode: thumbnail, title and
  season/episode; A starts it, B dismisses it.
- **Dedicated dropdown menus** for audio, subtitles and quality. Selecting the
  current option no longer restarts the stream.
- OSD opens with the focus on **play/pause** instead of the seek bar.
- The **seek is confirmed only with A**; waiting on the bar no longer jumps.
  The OSD stays visible while the seek bar has focus.
- Focus stays on the **Watched** button when the layout changes after marking
  an item.
- Replay icon now points **counter-clockwise**.

### Changed

- Removed the Y settings panel; the dropdown menus replace it.
- Removed the loading spinner and the audio/subtitle/quality toasts.

## 0.2.1-beta — 2026-11-15

### Added

- Interface in 21 languages, with a language selector in Settings; the choice
  is saved and applied without restarting the app.
- Languages: Italian, English, Spanish, French, German, Portuguese (Brazil and
  Portugal), Russian, Dutch, Polish, Swedish, Danish, Norwegian Bokmål, Czech,
  Ukrainian, Catalan, Turkish, Romanian, Greek, Hungarian, Finnish.
- On first start the interface follows the system language (English fallback).
- Qt translation files (`i18n/polyplink_*.ts`), ready for more languages.

## 0.2.0-beta — 2026-11-15

UI update: settings, account and detail pages.

### Added

- Two-column Settings and Account: menu on the left, detail panel on the right.
- Account profile: avatar, username, server and library stats (movies, series,
  episodes, favorites).
- Compact action buttons: icon only, expanding to icon + label on focus.
- Episode detail: *Go to full series* and *Previous episode*.
- Full plot view (*Read more*) with scrolling.
- Watched icon: double check when the item is watched.
- Favorite icon: empty outline or filled red heart.
- Persistent default quality and OSD duration; connection test; reset
  defaults; change server.
- Action toasts and empty states for library, seasons and episodes.
- **Up** from the first Settings/Account entry returns to the top bar.

### Changed

- Removed the A/B/Y hint texts across the UI; they remain only on the server
  selection screen.
- Season page actions use the compact icon buttons.
- Settings and Account entries now show their current value.

### Fixed

- Opening a favorite no longer shows the previously selected title.
- Removed the duplicate `seasonHeader` id that prevented QML from loading.
- Network errors stay visible; action feedback fades after a few seconds.

Same known limitations as 0.1.0-beta.

## 0.1.0-beta — 2026-11-15

First public beta.

### Added

- Native Jellyfin client for Steam Link: no browser, no Chromium.
- Home with *Continue watching*, *Next up*, *Recently added* (with the source
  library) and libraries.
- Full browsing: movies, series, seasons, episodes, detail page with metadata.
- Watched / unwatched and favorites, with home refresh.
- Resume position, *Resume* and *Play from beginning*.
- Series and seasons: *Play* (next episode), *Seasons*, watched and favorite;
  season page with buttons, overview and episode list.
- HLS player with hardware H.264 decoding (`SLVideo`) and AAC audio
  (`SLAudio`), audio/video sync.
- Navigable OSD: seek bar with cursor and time preview, acceleration
  (30 s → 1 min → 5 min), pause, -10/+30 s, audio track, burn-in subtitles,
  quality (4/8/12/20 Mbps), stats.
- Sign-in with manual address, username/password (on-screen keyboard) and Quick
  Connect.
- Server discovery with Jellyfin UDP and a local subnet scan.
- Top bar with Home, Favorites, Account and Settings.
- 1280x720 scaled UI designed for the remote.

### Fixes (before publication)

- If the server does not respond, starting playback is aborted with a message:
  no more 8-second black screen.
- If the connection drops during playback, the OSD shows "Connection lost…
  retrying"; when the buffer runs out the player closes with an error instead of
  freezing on a still frame.
- Reaching the natural end of a movie closes the player and records the
  position.
- Opening another title no longer keeps the previous detail page: if the server
  does not respond, "Unable to load" is shown with the reason.
- Offline home: clear message ("Server unreachable") and **A** to retry.
- API request timeout reduced to 10 s; progress reports no longer pile up (one
  at a time).
- Incremental demux: removed the micro-stutter on every 3-second segment.
- Sign-in: the saved server stays at the top, *Sign out* and *Switch user* work,
  a 401 returns to the login screen.
- Correct session reports: the Jellyfin dashboard shows *Transcode* and the
  right MediaSourceId.
- Seasons grid uses posters, like the movie grid.

### Known limitations

- Seeking and changing settings restart the stream (~2 s).
- Burn-in subtitles only.
- No search, playback queue or next-episode autoplay.
- No trickplay thumbnails on the seek bar.
- Settings and Account are pages with placeholder entries.
- H.264 8-bit only: no HEVC, HDR or AV1.
- Starting from a series/season uses the default audio track until the episode
  detail is opened.
- Automatic discovery covers the local /24 only.
