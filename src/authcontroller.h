#ifndef AUTH_CONTROLLER_H
#define AUTH_CONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "discovery.h"

class JellyfinClient;

// Server discovery, sign-in, Quick Connect and logout. The QML API stays on
// AppController; this class owns the authentication state machine and the
// discovery socket.
class AuthController : public QObject {
  Q_OBJECT

 public:
  explicit AuthController(JellyfinClient* client, QObject* parent = nullptr);

  QVariantList servers() const { return discovery_.servers(); }
  bool searching() const { return discovery_.searching(); }
  QString quickConnectCode() const { return qcCode_; }
  QString quickConnectStatus() const { return qcStatus_; }

  void discover();
  void selectServer(int index);
  void setManualServer(const QString& url);
  void loginWithPassword(const QString& user, const QString& pass);
  void beginQuickConnect();
  void cancelQuickConnect();
  void logout();
  void useSavedServer();
  void showServers();
  void applyConfigFile();

 signals:
  void stateRequested(const QString& state);
  void statusRequested(const QString& status);
  void errorRaised(const QString& message);
  void authenticated();
  void loggedOut();
  void serversChanged();
  void quickConnectChanged();

 private:
  void pollQuickConnect();

  JellyfinClient* client_;
  Discovery discovery_;
  QTimer qcTimer_;
  int qcAttempts_ = 0;
  QString qcCode_;
  QString qcStatus_;
  QString qcSecret_;
};

#endif  // AUTH_CONTROLLER_H
