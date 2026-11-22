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

// Scoperta automatica del server Jellyfin sulla rete locale.
// Il server risponde su UDP 7359 alla domanda "Who is JellyfinServer?" con un
// JSON {"Address":"http://ip:8096","Id":"...","Name":"..."}.
//
// Perche' serve: su una console non c'e' tastiera, e digitare l'URL del server
// col telecomando e' improponibile.
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
  // scansione della subnet locale: i server Jellyfin ascoltano su 8096
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

  QNetworkAccessManager nam_;
  QStringList lanHosts_;
  int lanInFlight_ = 0;
  bool udpDone_ = false;
};

#endif  // DISCOVERY_H
