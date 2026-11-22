#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUdpSocket>
#include <QVariantList>
#include <QVariantMap>

// Automatic discovery of the Jellyfin server on the local network.
// The server answers on UDP 7359 to the question "Who is JellyfinServer?" with
// the JSON {"Address":"http://ip:8096","Id":"...","Name":"..."}.
//
// Why it is needed: a console has no keyboard, and typing the server URL
// with the remote control is impractical.
class Discovery : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
  Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)

 public:
  explicit Discovery(QObject* parent = nullptr);

  QVariantList servers() const { return servers_; }
  bool searching() const { return searching_; }

  Q_INVOKABLE void start();
  Q_INVOKABLE void addManual(const QString& url, const QString& name = QString());

 signals:
  void serversChanged();
  void searchingChanged();
  void error(const QString& message);

 private:
  void onReadyRead();
  void finish();
  // local subnet scan: Jellyfin servers listen on 8096
  void scanLan();
  void startNextProbes();
  void probeHost(const QString& ip);
  void addEntry(const QString& address, const QString& name, const QString& id);

  QUdpSocket socket_;
  QTimer timeout_;
  QTimer retry_;
  QVariantList servers_;
  bool searching_ = false;
  bool warnedAddress_ = false;
  bool warnedLimit_ = false;

  QNetworkAccessManager nam_;
  QStringList lanHosts_;
  int lanInFlight_ = 0;
  bool udpDone_ = false;
};

#endif  // DISCOVERY_H
