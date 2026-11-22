#include "redact.h"

#include <QRegularExpression>

QString RedactSecrets(QString text) {
  // Credenziali in un URL (http://utente:password@host): non devono finire
  // nei log, anche se l'app non le usa.
  static const QRegularExpression userInfo(
      QStringLiteral("(?i)(https?://)([^/@\\s]+):([^/@\\s]+)@"));
  text.replace(userInfo, QStringLiteral("\\1\\2:***@"));

  // Token="..." nell'header MediaBrowser. Va fatto prima della regex sulle
  // query string, altrimenti il valore tra virgolette viene tagliato male.
  static const QRegularExpression tokenField(
      QStringLiteral("(?i)(\\bToken\\s*=\\s*)(\"?)([^\"\\s,;&]+)(\\2)"));
  text.replace(tokenField, QStringLiteral("\\1\\2***\\4"));

  // Header alternativo usato da alcune integrazioni Jellyfin/Emby.
  static const QRegularExpression embyToken(
      QStringLiteral("(?i)(x-(?:emby|mediabrowser)-token\\s*:\\s*)([^\\r\\n]+)"));
  text.replace(embyToken, QStringLiteral("\\1***"));

  // Campi JSON: "AccessToken": "abc", "Password": "..." (anche con spazio
  // dopo i due punti, come nelle risposte di alcuni server/proxy).
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
