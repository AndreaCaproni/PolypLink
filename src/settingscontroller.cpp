#include "settingscontroller.h"

#include "jellyfinclient.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>

namespace {

// Keep the original translation context so existing .qm files keep working.
QString trApp(const char* text) {
  return QCoreApplication::translate("AppController", text);
}

constexpr int kDefaultQuality = 12000;
constexpr int kDefaultOsdSeconds = 5;

}  // namespace

SettingsController::SettingsController(JellyfinClient* client, QObject* parent)
    : QObject(parent), client_(client) {
  QSettings settings;
  defaultQualityKbps_ =
      settings.value(QStringLiteral("defaultQualityKbps"), kDefaultQuality).toInt();
  {
    static const int valid[] = {4000, 8000, 12000, 20000, 30000,
                                40000, 50000, 60000, 80000, 100000};
    bool ok = false;
    for (int quality : valid) {
      if (quality == defaultQualityKbps_) ok = true;
    }
    if (!ok) defaultQualityKbps_ = kDefaultQuality;
  }

  osdSeconds_ = settings.value(QStringLiteral("osdSeconds"), kDefaultOsdSeconds).toInt();
  if (osdSeconds_ != 3 && osdSeconds_ != 5 && osdSeconds_ != 8) {
    osdSeconds_ = kDefaultOsdSeconds;
  }

  audioDelayMs_ =
      std::clamp(settings.value(QStringLiteral("audioDelayMs"), 0).toInt(), -2000, 2000);
}

void SettingsController::save() {
  QSettings settings;
  settings.setValue(QStringLiteral("defaultQualityKbps"), defaultQualityKbps_);
  settings.setValue(QStringLiteral("osdSeconds"), osdSeconds_);
  settings.setValue(QStringLiteral("audioDelayMs"), audioDelayMs_);
}

void SettingsController::setDefaultQuality(int kbps) {
  static const int valid[] = {4000, 8000, 12000, 20000, 30000,
                              40000, 50000, 60000, 80000, 100000};
  bool ok = false;
  for (int quality : valid) {
    if (quality == kbps) ok = true;
  }
  if (!ok || defaultQualityKbps_ == kbps) return;
  defaultQualityKbps_ = kbps;
  save();
  emit settingsChanged();
  emit noticeRequested(
      trApp("Qualità predefinita: %1 Mbps").arg(kbps / 1000));
}

void SettingsController::cycleDefaultQuality(int delta) {
  static const int values[] = {4000, 8000, 12000, 20000, 30000,
                               40000, 50000, 60000, 80000, 100000};
  int pos = 2;
  for (int i = 0; i < 10; ++i) {
    if (values[i] == defaultQualityKbps_) pos = i;
  }
  pos = (pos + delta + 10) % 10;
  defaultQualityKbps_ = values[pos];
  save();
  emit settingsChanged();
  emit noticeRequested(
      trApp("Qualità predefinita: %1 Mbps").arg(defaultQualityKbps_ / 1000));
}

void SettingsController::setOsdSeconds(int seconds) {
  if (seconds != 3 && seconds != 5 && seconds != 8) return;
  if (osdSeconds_ == seconds) return;
  osdSeconds_ = seconds;
  save();
  emit osdSecondsChanged();
  emit settingsChanged();
  emit noticeRequested(trApp("OSD: %1 secondi").arg(seconds));
}

void SettingsController::cycleOsdSeconds(int delta) {
  static const int values[] = {3, 5, 8};
  int pos = 1;
  for (int i = 0; i < 3; ++i) {
    if (values[i] == osdSeconds_) pos = i;
  }
  pos = (pos + delta + 3) % 3;
  osdSeconds_ = values[pos];
  save();
  emit osdSecondsChanged();
  emit settingsChanged();
  emit noticeRequested(trApp("OSD: %1 secondi").arg(osdSeconds_));
}

void SettingsController::setAudioDelay(int ms) {
  ms = std::clamp(ms, -2000, 2000);
  if (audioDelayMs_ == ms) return;
  audioDelayMs_ = ms;
  save();
  emit audioDelayChanged();
  emit settingsChanged();
  qInfo("[player] ritardo audio predefinito: %d ms", ms);
}

void SettingsController::previewAudioDelay(int ms) {
  ms = std::clamp(ms, -2000, 2000);
  if (audioDelayMs_ == ms) return;
  audioDelayMs_ = ms;
  emit audioDelayChanged();
}

void SettingsController::commitAudioDelay() { save(); }

void SettingsController::resetSettings() {
  defaultQualityKbps_ = kDefaultQuality;
  osdSeconds_ = kDefaultOsdSeconds;
  save();
  emit osdSecondsChanged();
  emit settingsChanged();
  emit noticeRequested(trApp("Impostazioni ripristinate"));
}

void SettingsController::testConnection() {
  if (client_->server().isEmpty()) {
    emit noticeRequested(trApp("Nessun server configurato"));
    return;
  }
  emit noticeRequested(trApp("Verifico la connessione…"));
  client_->fetchViews([this](const QJsonDocument& doc, const QString& error) {
    if (!error.isEmpty()) {
      emit statusRequested(error);
      return;
    }
    const int count =
        doc.object().value(QStringLiteral("Items")).toArray().size();
    emit noticeRequested(trApp("Connessione OK · %1 librerie").arg(count));
  });
}
