#include "redact.h"

#include <QRegularExpression>

QString RedactSecrets(QString text) {
  // Query string: api_key=..., ApiKey=..., token=..., access_token=...
  static const QRegularExpression query(
      QStringLiteral("(?i)(api[_-]?key|access[_-]?token|token)=([^&\\s]+)"));
  text.replace(query, QStringLiteral("\\1=***"));

  // Authorization header, if it ever ends up in a log line.
  static const QRegularExpression auth(
      QStringLiteral("(?i)(authorization:\\s*)([^\\r\\n]+)"));
  text.replace(auth, QStringLiteral("\\1***"));

  return text;
}
