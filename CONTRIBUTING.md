# Contributing

Thanks for helping. This is the canonical repository: please send fixes here
instead of publishing separate forks.

## Before you start

- Open an issue for a large change or a player refactor, so the approach can be
  agreed first.
- Keep changes focused. Behaviour-preserving refactors are preferred unless the
  issue is specifically about behaviour.
- Do not copy code from Moonlight or other projects with incompatible
  licensing. PolypLink is MIT and clean-room; see [PROVENANCE.md](PROVENANCE.md).
- Never commit keys, tokens, passwords, personal server addresses, `dist/`
  packages or `deploy/factory_test/run.sh`.
- Report security issues privately as described in [SECURITY.md](SECURITY.md),
  not in a public issue.

## Build and test

You need Valve's **Steam Link SDK** (not included). The build looks for it in
`../steamlink-sdk`, or pass it explicitly:

```bash
./build.sh --sdk ~/steamlink-sdk
./scripts/check.sh
```

`scripts/check.sh` covers translations, settings migration, log redaction,
server-URL normalisation and the TS demuxer tests. GitHub Actions runs the same
checks plus QML lint and secret-history checks.

## Pull requests

- Describe the problem and the change, and link the issue when there is one.
- Test player and UI changes on a real Steam Link; include firmware, media and
  reproduction details.
- UI changes need a screenshot or a short recording.
- New interface strings must be translated in all 21 `i18n/*.ts` files. The
  package is rejected by the translation check otherwise.
- Keep the existing structure, naming and formatting of the surrounding code.
- Contributions are accepted under the project's MIT license.
