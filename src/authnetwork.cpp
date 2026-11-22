#include "authnetwork.h"

#include "appcontroller.h"

#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

class AuthNetworkAccessManager : public QNetworkAccessManager {
 public:
  AuthNetworkAccessManager(AppController* app, QObject* parent)
      : QNetworkAccessManager(parent), app_(app) {}

 protected:
  QNetworkReply* createRequest(Operation op, const QNetworkRequest& request,
                               QIODevice* outgoingData) override {
    QNetworkRequest r(request);
    const QUrl target = request.url();
    const QUrl base(app_->serverUrl());
    if (!app_->authHeader().isEmpty() && base.isValid() &&
        target.scheme() == base.scheme() && target.host() == base.host() &&
        target.port() == base.port()) {
      r.setRawHeader("Authorization", app_->authHeader().toUtf8());
    }
    // The token must not follow redirects to other hosts.
    r.setAttribute(QNetworkRequest::FollowRedirectsAttribute, false);
    return QNetworkAccessManager::createRequest(op, r, outgoingData);
  }

 private:
  AppController* app_;
};

}  // namespace

QNetworkAccessManager* AuthNetworkAccessManagerFactory::create(QObject* parent) {
  return new AuthNetworkAccessManager(app_, parent);
}
