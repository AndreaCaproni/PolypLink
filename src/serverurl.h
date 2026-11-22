#ifndef SERVERURL_H
#define SERVERURL_H

#include <QString>

// Normalizes the Jellyfin server address.
//
// If the scheme is missing, it uses HTTPS for public hosts and HTTP for local
// network hosts (private IPs, localhost, .local/.lan/.home/.internal names or
// names without a domain). A typical LAN server listens on HTTP:8096; a public
// server must use HTTPS so credentials and tokens are not sent in the clear.
QString NormalizeServerUrl(QString url);

// True if the URL uses HTTP to a host that is not loopback: in that case
// passwords and tokens travel in the clear over the network and the UI must warn.
bool IsInsecureHttpUrl(const QString& url);

#endif  // SERVERURL_H
