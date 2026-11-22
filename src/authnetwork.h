#ifndef AUTHNETWORK_H
#define AUTHNETWORK_H

#include <QQmlNetworkAccessManagerFactory>

class AppController;
class QNetworkAccessManager;

// Network for QML images: adds the Authorization header to requests
// to the Jellyfin server, so the token does not have to be in the URL.
class AuthNetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory {
 public:
  explicit AuthNetworkAccessManagerFactory(AppController* app) : app_(app) {}

  QNetworkAccessManager* create(QObject* parent) override;

 private:
  AppController* app_;
};

#endif  // AUTHNETWORK_H
