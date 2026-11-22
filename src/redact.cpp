#include "redact.h"

#include <QRegularExpression>

QString RedactSecrets(QString text) {
  // Credentials in a URL (http://user:password@host): they must not end up
  // in the logs, even if the app does not use them.
  static const QRegularExpression userInfo(
      QStringLiteral("(?i)(https?://)([^/@\\s]+):([^/@\\s]+)@"));
  text.replace(userInfo, QStringLiteral("\\1\\2:***@"));

  // Token="..." in the MediaBrowser header. It must be done before the
  // query string regex, otherwise the quoted value is cut off incorrectly.
  static const QRegularExpression tokenField(
      QStringLiteral("(?i)(\\bToken\\s*=\\s*)(\"?)([^\"\\s,;&]+)(\\2)"));
  text.replace(tokenField, QStringLiteral("\\1\\2***\\4"));

  // Alternative header used by some Jellyfin/Emby integrations.
  static const QRegularExpression embyToken(
      QStringLiteral("(?i)(x-(?:emby|mediabrowser)-token\\s*:\\s*)([^\\r\\n]+)"));
  text.replace(embyToken, QStringLiteral("\\1***"));

  // JSON fields: "AccessToken": "abc", "Password": "..." (even with a space
  // after the colon, as in responses from some servers/proxies).
  static const QRegularExpression jsonField(
      QStringLiteral("(?i)(\"(?:access[_-]?token|token|api[_-]?key|secret|"
                     "password|pw)\"\\s*:\\s*\")([^\"]*)(\")"));
  text.replace(jsonField, QStringLiteral("\\1***\\3"));

  // Query string: api_key=..., ApiKey=..., token=..., access_token=...,
  // secret=..., password=..., Pw=...
  static const QRegularExpression query(
      QStringLiteral("(?i)(api[_-]?key|access[_-]?token|token|secret|password|pw)="
                     "([^&\\s\"':]+)"));
  text.replace(query, QStringLiteral("\\1=***"));

  // Authorization header, if it ever ends up in a log line.
  static const QRegularExpression auth(
      QStringLiteral("(?i)(authorization:\\s*)([^\\r\\n]+)"));
  text.replace(auth, QStringLiteral("\\1***"));

  return text;
}
