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

  // Query string: api_key=..., ApiKey=..., token=..., access_token=...
  static const QRegularExpression query(
      QStringLiteral("(?i)(api[_-]?key|access[_-]?token|token)=([^&\\s\"':]+)"));
  text.replace(query, QStringLiteral("\\1=***"));

  // Authorization header, if it ever ends up in a log line.
  static const QRegularExpression auth(
      QStringLiteral("(?i)(authorization:\\s*)([^\\r\\n]+)"));
  text.replace(auth, QStringLiteral("\\1***"));

  return text;
}
