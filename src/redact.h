#ifndef REDACT_H
#define REDACT_H

#include <QString>

// Rimuove token e chiavi API da URL e messaggi destinati ai log.
QString RedactSecrets(QString text);

#endif  // REDACT_H
