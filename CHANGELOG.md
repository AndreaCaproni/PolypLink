# Changelog

## Unreleased

### Security

- Redact `ApiKey`, `token`, `access_token` and `Authorization` values from logs.

### Added

- GitHub Actions checks: QML lint, translation completeness, settings migration
  and log-redaction tests.
- `scripts/package-release.sh`: builds the USB kit, creates the release zip/tgz
  and writes `SHA256SUMS`.
- Settings migration extracted to `scripts/migrate-settings.sh` and covered by a
  test.

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
