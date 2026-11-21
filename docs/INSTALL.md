# Installing on Steam Link

## What you need

- Valve Steam Link (2015 hardware) with updated firmware.
- A USB drive (FAT32 recommended, EXT4 works).
- A Jellyfin server reachable from the Link's network, with **transcoding
  enabled**: PolypLink always requests an H.264/AAC HLS transcode and does not
  direct-play.
- The `steamlink/apps/polyplink.tgz` package (produced by `./build.sh`).

## USB layout

```
steamlink/
├── apps/
│   └── polyplink.tgz
├── config/system/
│   ├── enable_ssh.txt          (optional, for debugging)
│   └── jellyfin-server.txt     (optional)
└── factory_test/
    └── run.sh                  (optional boot hook)
```

- `jellyfin-server.txt` contains only the server address, e.g.
  `https://jellyfin.example.com` or `http://192.168.1.20:8096` for a trusted
  LAN. If missing, the app asks for it on first start. Old `user=`/`pass=`
  directives are ignored: credentials are entered in the app or with Quick
  Connect.
- `factory_test/run.sh` is the hook the firmware runs at every boot. It is only
  needed for SSH/debugging; the app installs fine without it. The generated
  `dist/steamlink/` can contain your public key: do not share that folder.

## Steps

1. Format the USB drive (FAT32) and copy the `steamlink/` folder to its
   **root**.
2. Power off the Link, insert the drive, power it on.
3. Installation takes 1–2 minutes; the drive is unmounted right after.
4. **PolypLink** appears on the Link home screen.

## First start

1. **Server**: if there is no `jellyfin-server.txt`, the app scans the network
   (Jellyfin UDP + local subnet on port 8096). If nothing is found, choose
   *Enter server address* and type it with the on-screen keyboard (arrows + A).
   Public hostnames default to HTTPS; private/LAN addresses default to HTTP and
   get port `8096` automatically. For HTTPS use the port your certificate is
   served on (usually 443, or 8920 for Jellyfin's built-in HTTPS).
   Self-signed certificates are rejected.
2. **Sign in**: *Username and password* (on-screen keyboard with SHIFT and
   symbols) or **Quick Connect** (6-digit code entered in Jellyfin, Settings →
   Quick Connect).
3. You are on the home screen.

## Security

- **Use HTTPS when the server is not on a trusted LAN.** With HTTP, the
  username/password exchange and the Jellyfin token are not encrypted.
- The client verifies TLS certificates. Self-signed certificates are rejected:
  use a certificate from a trusted CA or a reverse proxy.
- The token is stored in the app configuration with owner-only permissions
  (`0600`). Logs are redacted, including QML image-loading warnings.
- Do not share the generated `dist/steamlink/` folder: it may include your
  personal `factory_test/run.sh` with an SSH public key.

## SSH and debugging (optional)

The normal `./build.sh` does **not** include SSH or the personal boot hook.
For development use `./build.sh --dev-ssh`: it includes `factory_test/run.sh`
and enables SSH. The repository only ships an **example**
(`run.sh.example`): copy it to `run.sh` and add your own public key. `run.sh`
is ignored by git because it contains personal data, and a `--dev-ssh` kit must
not be shared.

App logs:

- on the device: `/home/apps/polyplink/polyplink-run.log`
- on the USB drive, at the next boot: log files in the drive root.

## Updating or uninstalling

- **Update**: rebuild with `./build.sh`, copy `steamlink/` to the USB drive and
  boot the Link with the drive inserted.
- **Uninstall**: remove `polyplink.tgz` from the drive and reinstall, or
  delete `/home/apps/polyplink` on the device over SSH.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| The app does not appear on the home screen | `steamlink/` is not at the drive root, or the `.tgz` does not have the app folder at its top level |
| "loading…" forever | server unreachable, wrong IP/port, firewall |
| Video starts after a few seconds | the server is preparing the transcode (normal, 1–3 s) |
| A second launch does nothing | another Polyplink instance is already running; the single-instance guard exits instead of competing for input/display |
| Unwanted subtitles | choose *Subtitles → Off* in the player (Y or the dedicated button) |
