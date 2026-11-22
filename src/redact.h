#ifndef REDACT_H
#define REDACT_H

#include <QString>

// Removes tokens and API keys from URLs and messages destined for the logs.
QString RedactSecrets(QString text);

#endif  // REDACT_H
