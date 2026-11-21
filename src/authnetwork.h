#ifndef AUTHNETWORK_H
#define AUTHNETWORK_H

#include <QQmlNetworkAccessManagerFactory>

class AppController;
class QNetworkAccessManager;

// Rete per le immagini QML: aggiunge l'header Authorization alle richieste
// verso il server Jellyfin, così il token non deve stare nell'URL.
class AuthNetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory {
 public:
  explicit AuthNetworkAccessManagerFactory(AppController* app) : app_(app) {}

  QNetworkAccessManager* create(QObject* parent) override;

 private:
  AppController* app_;
};

#endif  // AUTHNETWORK_H
