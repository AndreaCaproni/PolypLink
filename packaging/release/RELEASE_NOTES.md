PolypLink @VERSION@ — a native Jellyfin client for the Valve Steam Link (2015).

No Chromium: Qt Quick/C++ with hardware H.264 decoding (SLVideo) and AAC audio
(SLAudio). The server transcodes to H.264/AAC over HLS.

## The app is now PolypLink

JellyLink has been renamed **PolypLink**. The package and binary are now
`polyplink`; existing settings (server, token, language, quality) are migrated
on the first start. New logo for the home screen and header.

## Player and quality

- **Bitrate maximum** can be selected from a list: **4, 8, 12, 20, 30, 40, 50,
  60, 80, 100 Mbps** (default 12).
- **OSD fade** can be selected from a list: 3, 5 or 8 seconds.
- The player uses the selected bitrate for the decoder target and the stats
  panel shows the real bitrate.
- **4K sources are capped to 1920×1080** in the transcode. This fixes the
  `SLVideo` *stream macroblock error* caused by the server sending 2560×1072.
- Seek acceleration starts only while holding the analog stick; single taps
  always use the base step. Maximum acceleration is now 3 minutes.
- Seeking is confirmed only with A; the OSD stays visible while the bar has
  focus.

## Settings cleanup

- **Bitrate massimo** replaces the old quality entry.
- **Dissolvenza OSD** replaces “OSD”.
- Removed *Risoluzione massima*, *Audio preferito*, *Precaricamento* and
  *Seek e accelerazione* from Settings.
- Removed *Ripresa*, *Prossimo episodio*, *Statistiche di visione* and the
  duplicate *Quick Connect* from Account.
- Placeholder entries now show **Funzionalità in arrivo**.
- The next-episode card, connection-lost card, track menus and 21-language
  interface from 0.2.x are included.

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

Code: MIT. FFmpeg: LGPL 2.1+ (see `THIRD_PARTY.md`). Translations are original
machine-assisted work. Not affiliated with the Jellyfin project or Valve.
