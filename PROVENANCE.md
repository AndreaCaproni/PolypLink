# Provenance and licensing

PolypLink is an independent client. It is not a fork of Jellyfin, Moonlight or
any other project, and it does not vendor their source code.

## Jellyfin

PolypLink talks to a Jellyfin server through Jellyfin's public HTTP API. Endpoint
paths, query parameters, JSON field names and the `Authorization` header format
are functional interface requirements. They are not copied source code.

No Jellyfin server, web client or SDK source files are included in this
repository. Jellyfin itself is licensed separately; see
<https://jellyfin.org> and the license of the component you use.

Using the API does not make PolypLink a derivative work of the Jellyfin server.
The project is not affiliated with or endorsed by the Jellyfin project.

## Valve Steam Link SDK

PolypLink is built with the Valve Steam Link SDK and uses the `SLVideo` and
`SLAudio` libraries provided by the device firmware. Those libraries and the
SDK are not redistributed by this project. Their use is limited to the public
headers and the documented API calls needed to decode video and play audio on
the device.

## Moonlight

PolypLink contains no Moonlight source code. Moonlight was consulted as public
documentation for the Steam Link platform: the Qt/EGLFS setup, the
`SLVideo`/`SLAudio` capabilities and launcher practices. Those are platform
facts and API usage, not copied program text.

The video-plane refresh workaround is an independent clean-room implementation
built only on the public `SLVideo` API: after a stream is released, a
short-lived overlay is presented and hidden so the compositor re-evaluates the
video plane before the context is released. It contains no Moonlight source
code.

Moonlight itself is licensed under GPLv3; PolypLink does not include or link
Moonlight code.

## AI assistance

The project was developed with substantial AI assistance and then reviewed,
built and tested on real Steam Link hardware. Third-party source files are not
vendored; third-party binaries and their licenses are listed in
[THIRD_PARTY.md](THIRD_PARTY.md).

## License

Original PolypLink code is released under the MIT license; see
[LICENSE](LICENSE). Third-party components keep their own licenses.
