#include "serverurl.h"

#include <QHostAddress>
#include <QStringList>

namespace {

bool IsLocalHostName(const QString& host) {
  const QString h = host.toLower();
  if (h == QLatin1String("localhost")) return true;
  if (h.endsWith(QLatin1String(".local")) || h.endsWith(QLatin1String(".lan")) ||
      h.endsWith(QLatin1String(".home")) ||
      h.endsWith(QLatin1String(".internal"))) {
    return true;
  }
  // Un IPv6 contiene ':' e va valutato da QHostAddress, non con l'euristica
  // dei nomi senza dominio.
  if (h.contains(QLatin1Char(':'))) return false;
  // Nome singolo senza dominio: quasi sempre un host della LAN.
  return !h.contains(QLatin1Char('.'));
}

bool IsPrivateHost(const QString& rawHost) {
  QString host = rawHost;
  if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']'))) {
    host = host.mid(1, host.size() - 2);
  }
  if (IsLocalHostName(host)) return true;

  QHostAddress addr;
  if (!addr.setAddress(host)) return false;

  if (addr.isLoopback() || addr.isLinkLocal()) return true;
  return addr.isInSubnet(QHostAddress(QStringLiteral("10.0.0.0")), 8) ||
         addr.isInSubnet(QHostAddress(QStringLiteral("172.16.0.0")), 12) ||
         addr.isInSubnet(QHostAddress(QStringLiteral("192.168.0.0")), 16) ||
         addr.isInSubnet(QHostAddress(QStringLiteral("fc00::")), 7) ||
         addr.isInSubnet(QHostAddress(QStringLiteral("fe80::")), 10);
}

QString HostPart(const QString& url) {
  QString host = url;
  const int slash = host.indexOf(QLatin1Char('/'));
  if (slash >= 0) host = host.left(slash);
  const int at = host.lastIndexOf(QLatin1Char('@'));
  if (at >= 0) host = host.mid(at + 1);
  if (host.startsWith(QLatin1Char('['))) {
    const int end = host.indexOf(QLatin1Char(']'));
    if (end > 0) return host.mid(1, end - 1);
    return host;
  }
  const int colon = host.lastIndexOf(QLatin1Char(':'));
  if (colon > 0) host = host.left(colon);
  return host;
}

}  // namespace

QString NormalizeServerUrl(QString url) {
  url = url.trimmed();
  if (url.isEmpty()) return url;

  const int schemeEnd = url.indexOf(QLatin1String("://"));
  const int minLength = schemeEnd >= 0 ? schemeEnd + 3 : 0;
  while (url.size() > minLength && url.endsWith(QLatin1Char('/'))) {
    url.chop(1);
  }

  if (url.startsWith(QLatin1String("http://")) ||
      url.startsWith(QLatin1String("https://"))) {
    return url;
  }

  const QString scheme =
      IsPrivateHost(HostPart(url)) ? QStringLiteral("http") : QStringLiteral("https");
  return scheme + QStringLiteral("://") + url;
}
