#ifndef DISCOVERY_ADDRESS_H
#define DISCOVERY_ADDRESS_H

#include <QHostAddress>
#include <QString>
#include <QUrl>

// L'UDP di Jellyfin non e' autenticato: chiunque sulla LAN puo' rispondere al
// broadcast e dichiarare un Address arbitrario. Per non trasformare la
// scoperta in un modo per puntare l'app verso un altro host, teniamo solo
// schema, porta e path e sostituiamo l'host con l'IP del mittente.
//
// Ritorna una stringa vuota se la risposta non e' un indirizzo http(s) valido.
inline QString NormalizeDiscoveredAddress(const QString& advertised,
                                          const QHostAddress& sender) {
  if (sender.isNull() || sender == QHostAddress(QHostAddress::AnyIPv4) ||
      sender == QHostAddress(QHostAddress::AnyIPv6) || sender.isMulticast()) {
    return QString();
  }

  // Un mittente IPv4-mapped (::ffff:a.b.c.d) va trattato come IPv4.
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
