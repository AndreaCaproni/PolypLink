POLYPLINK — beta @VERSION_SHORT@
Native Jellyfin client for Steam Link
=====================================

No browser, no PC left running: the app runs on the Link and the server
does the transcoding. Independent project, not affiliated with Jellyfin.

WHAT'S INSIDE
steamlink/apps/polyplink.tgz
        The app (~860 KB). Includes the AAC decoding libraries and the
        license notices in licenses/.

INSTALL
1. USB drive, FAT32 recommended (EXT4 works).
2. Copy the "steamlink" folder to the ROOT of the drive:
       USB/steamlink/apps/polyplink.tgz
3. Plug the drive in with the Link POWERED OFF, then power it on.
4. Installation takes 1-2 minutes. "PolypLink" appears on the home screen.

The drive is unmounted right after installation: it only carries the app.

FIRST START
1. Server: the app scans the local network (UDP + subnet scan).
   If nothing is found, choose "Enter server address" and type it with the
   on-screen keyboard (e.g. https://jellyfin.example.com). For a LAN server
   without HTTPS, enter its HTTP address and port 8096; use HTTP only on a
   trusted network.
   Alternatively create:
       steamlink/config/system/jellyfin-server.txt
   containing the server address.
2. Sign in: username/password, or Quick Connect (6-digit code entered in
   Jellyfin).
3. Language: on first start it follows the system language. Change it in
   Settings → Interface language.

SERVER REQUIREMENT
------------------
Transcoding MUST be enabled on the Jellyfin server. PolypLink always requests
an H.264 + AAC HLS transcode and does not direct-play.

LANGUAGE
--------
The interface is available in 21 languages: Italian, English, Spanish,
French, German, Portuguese (Brazil and Portugal), Russian, Dutch, Polish,
Swedish, Danish, Norwegian Bokmål, Czech, Ukrainian, Catalan, Turkish,
Romanian, Greek, Hungarian and Finnish.
Translations are a first pass, not native-reviewed. Titles, plots and
genres follow the metadata language configured on the Jellyfin libraries.

PLAYER
------
- Bitrate/quality selectable from 4 to 100 Mbps.
- OSD fade selectable: 3, 5 or 8 seconds.
- 4K sources are capped to 1080p server-side to avoid decoder errors.
- Connection-lost card after 10 seconds without progress.
- Next-episode card in the last minute (A starts, B dismisses).
- Separate menus for Audio, Subtitles and Quality.
- Seeking is confirmed only with A; the OSD stays on the bar.
- Seek acceleration only while holding the stick, up to 3 minutes.

BETA NOTES
----------
- Seeking and changing tracks restart the stream (~2 seconds).
- Subtitles are burn-in only (rendered by the server).
- No search, queue or next-episode autoplay.
- H.264 only: no HEVC, HDR or AV1.
- Above ~40 Mbps is experimental; 80–100 Mbps can saturate the Link's
  100 Mbit Ethernet.
- Some Account and Settings entries show "Feature coming soon".

Device log:
  /home/apps/polyplink/polyplink-run.log
