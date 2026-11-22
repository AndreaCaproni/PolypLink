#ifndef DISCOVERY_ADDRESS_H
#define DISCOVERY_ADDRESS_H

#include <QHostAddress>
#include <QString>
#include <QUrl>

// Jellyfin UDP is not authenticated: anyone on the LAN can answer the
// broadcast and advertise an arbitrary Address. To keep discovery from
// becoming a way to point the app at another host, we keep only the
// scheme, port and path and replace the host with the sender's IP.
//
// Returns an empty string if the response is not a valid http(s) address.
inline QString NormalizeDiscoveredAddress(const QString& advertised,
                                          const QHostAddress& sender) {
  if (sender.isNull() || sender == QHostAddress(QHostAddress::AnyIPv4) ||
      sender == QHostAddress(QHostAddress::AnyIPv6) || sender.isMulticast()) {
    return QString();
  }

  // An IPv4-mapped sender (::ffff:a.b.c.d) must be treated as IPv4.
  QHostAddress host = sender;
  bool isIpv4 = false;
  const quint32 v4 = sender.toIPv4Address(&isIpv4);
  if (isIpv4) host = QHostAddress(v4);

  QUrl url(advertised);
  if (!url.isValid()) return QString();
  const QString scheme = url.scheme().toLower();
  if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
    return QString();
  }
  if (url.host().isEmpty() || !url.userInfo().isEmpty()) return QString();
  if (url.hasQuery() || url.hasFragment()) return QString();

  url.setHost(host.toString());
  QString out = url.toString(QUrl::FullyEncoded);
  while (out.endsWith(QLatin1Char('/'))) out.chop(1);
  return out;
}

#endif  // DISCOVERY_ADDRESS_H
