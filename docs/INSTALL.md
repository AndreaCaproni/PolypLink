# Installing on Steam Link

## What you need

- Valve Steam Link (2015 hardware) with updated firmware.
- A USB drive (FAT32 recommended, EXT4 works).
- A Jellyfin server reachable from the Link's network, with **transcoding
  enabled**: JellyLink always requests an H.264/AAC HLS transcode and does not
  direct-play.
- The `steamlink/apps/jellyfin-link.tgz` package (produced by `./build.sh`).

## USB layout

```
steamlink/
├── apps/
│   └── jellyfin-link.tgz
├── config/system/
│   ├── enable_ssh.txt          (optional, for debugging)
│   └── jellyfin-server.txt     (optional)
└── factory_test/
    └── run.sh                  (optional boot hook)
```

- `jellyfin-server.txt` contains the server address, e.g.
  `http://192.168.1.20:8096`. If missing, the app asks for it on first start.
- `factory_test/run.sh` is the hook the firmware runs at every boot. It is only
  needed for SSH/debugging; the app installs fine without it.

## Steps

1. Format the USB drive (FAT32) and copy the `steamlink/` folder to its
   **root**.
2. Power off the Link, insert the drive, power it on.
3. Installation takes 1–2 minutes; the drive is unmounted right after.
4. **JellyLink** appears on the Link home screen.

## First start

1. **Server**: if there is no `jellyfin-server.txt`, the app scans the network
   (Jellyfin UDP + local subnet on port 8096). If nothing is found, choose
   *Enter server address* and type it with the on-screen keyboard (arrows + A).
   Port `8096` is added automatically if you omit it.
2. **Sign in**: *Username and password* (on-screen keyboard with SHIFT and
   symbols) or **Quick Connect** (6-digit code entered in Jellyfin, Settings →
   Quick Connect).
3. You are on the home screen.

## SSH and debugging (optional)

`deploy/factory_test/run.sh` runs as root at every boot, with the USB drive
mounted. The repository only ships an **example** (`run.sh.example`): copy it
to `run.sh` and add your own public key. `run.sh` is ignored by git because it
contains personal data.

App logs:

- on the device: `/home/apps/jellyfin-link/jellyfin-link-run.log`
- on the USB drive, at the next boot: log files in the drive root.

## Updating or uninstalling

- **Update**: rebuild with `./build.sh`, copy `steamlink/` to the USB drive and
  boot the Link with the drive inserted.
- **Uninstall**: remove `jellyfin-link.tgz` from the drive and reinstall, or
  delete `/home/apps/jellyfin-link` on the device over SSH.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| The app does not appear on the home screen | `steamlink/` is not at the drive root, or the `.tgz` does not have the app folder at its top level |
| "loading…" forever | server unreachable, wrong IP/port, firewall |
| Video starts after a few seconds | the server is preparing the transcode (normal, 1–3 s) |
| Unwanted subtitles | choose *Subtitles → Off* in the player (Y or the dedicated button) |
