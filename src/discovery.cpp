#include "discovery.h"

#include "discoveryaddress.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAddressEntry>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>

namespace {
constexpr quint16 kDiscoveryPort = 7359;
constexpr int kDiscoveryTimeoutMs = 2500;
constexpr int kDiscoveryRetryMs = 400;
constexpr int kProbeTimeoutMs = 600;
}

Discovery::Discovery(QObject* parent) : QObject(parent) {
  timeout_.setSingleShot(true);
  timeout_.setInterval(kDiscoveryTimeoutMs);
  connect(&timeout_, &QTimer::timeout, this, &Discovery::finish);

  retry_.setInterval(kDiscoveryRetryMs);
  connect(&retry_, &QTimer::timeout, this, [this]() {
    static const QByteArray question("Who is JellyfinServer?");
    socket_.writeDatagram(question, QHostAddress::Broadcast, kDiscoveryPort);
  });
}

void Discovery::start() {
  servers_.clear();
  emit serversChanged();

  udpDone_ = false;
  warnedAddress_ = false;
  warnedLimit_ = false;
  searching_ = true;
  emit searchingChanged();

  if (socket_.state() != QAbstractSocket::BoundState) {
    if (!socket_.bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress)) {
      searching_ = false;
      emit searchingChanged();
      emit error(tr("impossibile aprire la rete: %1").arg(socket_.errorString()));
      return;
    }
    socket_.setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
  }

  connect(&socket_, &QUdpSocket::readyRead, this, &Discovery::onReadyRead,
          Qt::UniqueConnection);

  static const QByteArray question("Who is JellyfinServer?");
  for (int i = 0; i < 3; ++i) {
    socket_.writeDatagram(question, QHostAddress::Broadcast, kDiscoveryPort);
  }
  retry_.start();
  timeout_.start();

  // Jellyfin UDP does not cross Docker/bridge: in parallel we try
  // all addresses of the local subnet on port 8096.
  scanLan();
}

void Discovery::addEntry(const QString& address, const QString& name,
                         const QString& id) {
  constexpr int kMaxServers = 64;
  if (servers_.size() >= kMaxServers) {
    if (!warnedLimit_) {
      warnedLimit_ = true;
      qWarning("[discovery] limite di %d server raggiunto", kMaxServers);
    }
    return;
  }
  for (const QVariant& v : servers_) {
    if (v.toMap().value(QStringLiteral("address")).toString() == address) return;
  }
  QVariantMap entry;
  entry.insert(QStringLiteral("address"), address);
  entry.insert(QStringLiteral("name"), name.isEmpty() ? address : name);
  entry.insert(QStringLiteral("id"), id);
  servers_.append(entry);
  emit serversChanged();
}

void Discovery::scanLan() {
  if (lanInFlight_ > 0 || !lanHosts_.isEmpty()) return;

  QSet<quint32> nets;
  const auto interfaces = QNetworkInterface::allInterfaces();
  for (const QNetworkInterface& iface : interfaces) {
    const auto flags = iface.flags();
    if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) ||
        (flags & QNetworkInterface::IsLoopBack)) {
      continue;
    }
    for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
      if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
      const quint32 ip = entry.ip().toIPv4Address();
      if (ip == 0) continue;
      const quint32 net = ip & 0xFFFFFF00u;  // the /24 of the local address
      if (nets.contains(net)) continue;
      nets.insert(net);
      for (int host = 1; host < 255; ++host) {
        const quint32 addr = net | static_cast<quint32>(host);
        if (addr == ip) continue;
        lanHosts_.append(QHostAddress(addr).toString());
      }
    }
  }

  if (lanHosts_.isEmpty()) return;
  qInfo("[discovery] scansione LAN: %d indirizzi",
        static_cast<int>(lanHosts_.size()));
  startNextProbes();
}

void Discovery::startNextProbes() {
  constexpr int kMaxInFlight = 64;
  while (lanInFlight_ < kMaxInFlight && !lanHosts_.isEmpty()) {
    const QString ip = lanHosts_.takeFirst();
    ++lanInFlight_;
    probeHost(ip);
  }
}

void Discovery::probeHost(const QString& ip) {
  QNetworkRequest req{QUrl(QStringLiteral("http://%1:8096/System/Info/Public").arg(ip))};
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  auto* reply = nam_.get(req);

  auto* timer = new QTimer(reply);
  timer->setSingleShot(true);
  connect(timer, &QTimer::timeout, reply, [reply]() {
    reply->setProperty("timedOut", true);
    reply->abort();
  });
  timer->start(kProbeTimeoutMs);

  connect(reply, &QNetworkReply::finished, this, [this, reply, ip]() {
    reply->deleteLater();
    --lanInFlight_;
    if (!reply->property("timedOut").toBool() &&
        reply->error() == QNetworkReply::NoError) {
      const QJsonObject o =
          QJsonDocument::fromJson(reply->readAll()).object();
      const QString product = o.value(QStringLiteral("ProductName")).toString();
      const QString id = o.value(QStringLiteral("Id")).toString();
      if (product.contains(QLatin1String("jellyfin"), Qt::CaseInsensitive) ||
          !id.isEmpty()) {
        addEntry(QStringLiteral("http://%1:8096").arg(ip),
                 o.value(QStringLiteral("ServerName")).toString(), id);
      }
    }
    if (!lanHosts_.isEmpty()) {
      startNextProbes();
    } else if (lanInFlight_ == 0 && udpDone_) {
      searching_ = false;
      emit searchingChanged();
    }
  });
}

void Discovery::onReadyRead() {
  while (socket_.hasPendingDatagrams()) {
    const QNetworkDatagram datagram = socket_.receiveDatagram();
    const QByteArray payload = datagram.data().trimmed();
    if (payload.isEmpty()) continue;

    const QJsonObject o = QJsonDocument::fromJson(payload).object();
    const QString advertised = o.value(QStringLiteral("Address")).toString();
    if (advertised.isEmpty()) continue;

    // UDP is not authenticated: the advertised host is not trustworthy.
    // We use the sender's IP and keep only scheme, port and path.
    const QString address =
        NormalizeDiscoveredAddress(advertised, datagram.senderAddress());
    if (address.isEmpty()) {
      if (!warnedAddress_) {
        warnedAddress_ = true;
        qWarning("[discovery] risposta UDP da %s ignorata: indirizzo non valido",
                 qPrintable(datagram.senderAddress().toString()));
      }
      continue;
    }

    const QString id = o.value(QStringLiteral("Id")).toString();
    const QString name = o.value(QStringLiteral("Name")).toString();

    addEntry(address, name, id);
  }
}

void Discovery::addManual(const QString& url, const QString& name) {
  QString address = url.trimmed();
  while (address.endsWith(QLatin1Char('/'))) address.chop(1);
  if (address.isEmpty()) return;
  if (!address.startsWith(QLatin1String("http://")) &&
      !address.startsWith(QLatin1String("https://"))) {
    address.prepend(QLatin1String("http://"));
  }
  addEntry(address, name, QString());
}

void Discovery::finish() {
  retry_.stop();
  udpDone_ = true;
  if (lanInFlight_ == 0 && lanHosts_.isEmpty()) {
    searching_ = false;
    emit searchingChanged();
  }
}
