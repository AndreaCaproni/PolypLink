#include "authcontroller.h"

#include "jellyfinclient.h"
#include "redact.h"
#include "serverurl.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>

namespace {

// Keep the original translation context so existing .qm files keep working.
QString trApp(const char* text) {
  return QCoreApplication::translate("AppController", text);
}

}  // namespace

AuthController::AuthController(JellyfinClient* client, QObject* parent)
    : QObject(parent), client_(client) {
  connect(&discovery_, &Discovery::serversChanged, this,
          &AuthController::serversChanged);
  connect(&discovery_, &Discovery::error, this, [this](const QString& message) {
    emit statusRequested(message);
    emit errorRaised(message);
  });

  qcTimer_.setInterval(2000);
  connect(&qcTimer_, &QTimer::timeout, this, &AuthController::pollQuickConnect);
}

void AuthController::discover() {
  emit statusRequested(trApp("Cerco il server Jellyfin sulla rete…"));
  discovery_.start();
}

void AuthController::setManualServer(const QString& url) {
  QString u = NormalizeServerUrl(url);
  if (u.isEmpty()) return;

  // Local HTTP: Jellyfin uses 8096. Public HTTPS: leave the default
  // port (443) or the one written by the user, to avoid guessing wrong
  // behind a reverse proxy.
  const bool secure = u.startsWith(QLatin1String("https://"));
  const int schemeEnd = u.indexOf(QLatin1String("://"));
  const QString hostPart = schemeEnd >= 0 ? u.mid(schemeEnd + 3) : u;
  if (!secure && !hostPart.contains(QLatin1Char(':')) &&
      !hostPart.contains(QLatin1Char('/'))) {
    u += QLatin1String(":8096");
  }
  client_->setServer(u);
  emit stateRequested(QStringLiteral("auth"));
}

void AuthController::loginWithPassword(const QString& user, const QString& pass) {
  if (user.trimmed().isEmpty()) {
    emit statusRequested(trApp("Inserisci il nome utente"));
    return;
  }
  emit statusRequested(trApp("Accesso in corso…"));
  client_->authenticate(
      user.trimmed(), pass,
      [this](const QJsonDocument&, const QString& error) {
        if (!error.isEmpty()) {
          emit statusRequested(trApp("Accesso fallito: %1").arg(error));
          return;
        }
        emit authenticated();
      });
}

void AuthController::selectServer(int index) {
  const QVariantList list = discovery_.servers();
  if (index < 0 || index >= list.size()) return;
  const QVariantMap server = list.at(index).toMap();
  client_->setServer(server.value(QStringLiteral("address")).toString());
  client_->setServerName(server.value(QStringLiteral("name")).toString());
  emit stateRequested(QStringLiteral("auth"));
}

void AuthController::beginQuickConnect() {
  qcCode_.clear();
  qcStatus_ = trApp("Preparo il codice…");
  emit quickConnectChanged();
  emit stateRequested(QStringLiteral("quickconnect"));

  client_->quickConnectInitiate(
      [this](const QJsonDocument& doc, const QString& error) {
        if (!error.isEmpty()) {
          qcStatus_ = error;
          emit quickConnectChanged();
          return;
        }
        const QJsonObject o = doc.object();
        qcCode_ = o.value(QStringLiteral("Code")).toString();
        qcSecret_ = o.value(QStringLiteral("Secret")).toString();
        qcAttempts_ = 0;
        qcStatus_ = trApp(
            "Apri Jellyfin sul telefono o sul PC: Impostazioni → Quick Connect, "
            "e inserisci questo codice.");
        emit quickConnectChanged();
        if (!qcSecret_.isEmpty()) qcTimer_.start();
      });
}

void AuthController::cancelQuickConnect() {
  qcTimer_.stop();
  qcSecret_.clear();
  qcCode_.clear();
  emit stateRequested(QStringLiteral("login"));
}

void AuthController::logout() {
  qcTimer_.stop();
  qcSecret_.clear();
  qcCode_.clear();
  client_->logout();
  emit loggedOut();
}

void AuthController::useSavedServer() {
  if (client_->server().isEmpty()) return;
  emit stateRequested(QStringLiteral("auth"));
}

// The firmware copies steamlink/config/* to /mnt/config/ on every boot: that
// path is persistent and readable by the app. It is needed because UDP discovery
// does not cross Docker (bridge network) or networks with isolated broadcast.
//
// File format (one line, # for comments):
//   https://jellyfin.example.com
// The old user=/pass= directives are ignored: credentials must not be
// written to a text file on the USB stick. Use the login screen
// or Quick Connect.
void AuthController::applyConfigFile() {
  const QStringList candidates = {
      QStringLiteral("/mnt/config/system/jellyfin-server.txt"),
      QStringLiteral("/home/apps/polyplink/server.txt"),
  };

  QString url;
  QString used;
  bool legacyCredentials = false;

  for (const QString& path : candidates) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    const QStringList lines =
        QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
      const QString line = raw.trimmed();
      if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
      if (line.startsWith(QLatin1String("user=")) ||
          line.startsWith(QLatin1String("pass="))) {
        legacyCredentials = true;
        continue;
      }
      if (url.isEmpty()) url = line;
    }
    if (!url.isEmpty()) {
      used = path;
      break;
    }
  }

  if (legacyCredentials) {
    qWarning("[security] user=/pass= in jellyfin-server.txt ignorati: "
             "rimuovi le credenziali in chiaro dal file");
  }

  if (url.isEmpty()) {
    qInfo("[config] nessun file di configurazione, avvio la scoperta automatica");
    discover();
    return;
  }

  qInfo("[config] server da %s: %s", qPrintable(used),
        qPrintable(RedactSecrets(url)));

  client_->setServer(url, true);
  client_->setServerName(QString());
  emit stateRequested(QStringLiteral("auth"));
}

void AuthController::pollQuickConnect() {
  if (qcSecret_.isEmpty()) return;
  if (++qcAttempts_ > 90) {
    qcTimer_.stop();
    qcStatus_ = trApp("Tempo scaduto. Riprova.");
    emit quickConnectChanged();
    return;
  }

  client_->quickConnectPoll(
      qcSecret_, [this](const QJsonDocument& doc, const QString& error) {
        if (!error.isEmpty()) return;  // polling ignores transient errors
        const QJsonObject o = doc.object();
        if (!o.value(QStringLiteral("Authenticated")).toBool()) return;
        qcTimer_.stop();
        qcStatus_ = trApp("Autorizzato, accesso in corso…");
        emit quickConnectChanged();
        client_->quickConnectAuthenticate(
            qcSecret_, [this](const QJsonDocument&, const QString& authError) {
              if (!authError.isEmpty()) {
                qcStatus_ = authError;
                emit quickConnectChanged();
                return;
              }
              qcSecret_.clear();
              qcCode_.clear();
              emit authenticated();
            });
      });
}
