# Security policy

## Supported versions

Only the latest beta release receives security fixes. Older releases are kept
for history and are not maintained.

## Reporting a vulnerability

Please use GitHub's private vulnerability reporting:

<https://github.com/AndreaCaproni/PolypLink/security/advisories/new>

Do not open a public issue. Do not include tokens, passwords, private keys or
personal server addresses in reports or logs.

Please include:

- PolypLink version and build stamp (footer or Settings);
- Steam Link firmware build;
- Jellyfin server version;
- the affected area (API client, player, discovery, packaging);
- reproduction steps and impact;
- redacted logs or a minimal proof of concept.

## Scope

In scope: this client, its packaging and its release scripts.

Out of scope: Jellyfin server vulnerabilities (report them upstream), Steam Link
firmware and Valve SDK issues (report them to Valve), and problems that require
an already-compromised LAN or physical access to the device.

## Response

Maintenance is best-effort and there is no bug bounty. Please allow time for a
fix before public disclosure.
