#include "settingscontroller.h"

#include "jellyfinclient.h"
#include "quality.h"

#include "translation.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>

namespace {

constexpr int kDefaultQuality = 12000;
constexpr int kDefaultOsdSeconds = 5;
constexpr int kOsdSeconds[] = {3, 5, 8};
constexpr int kOsdCount =
    static_cast<int>(sizeof(kOsdSeconds) / sizeof(kOsdSeconds[0]));

bool IsValidOsdSeconds(int seconds) {
  for (int value : kOsdSeconds) {
    if (value == seconds) return true;
  }
  return false;
}

int ShiftOsdSeconds(int current, int delta) {
  int index = 1;
  for (int i = 0; i < kOsdCount; ++i) {
    if (kOsdSeconds[i] == current) index = i;
  }
  index = (index + delta + kOsdCount) % kOsdCount;
  return kOsdSeconds[index];
}

}  // namespace

SettingsController::SettingsController(JellyfinClient* client, QObject* parent)
    : QObject(parent), client_(client) {
  QSettings settings;
  defaultQualityKbps_ =
      settings.value(QStringLiteral("defaultQualityKbps"), kDefaultQuality).toInt();
  if (!IsValidQuality(defaultQualityKbps_)) {
    defaultQualityKbps_ = kDefaultQuality;
  }

  osdSeconds_ = settings.value(QStringLiteral("osdSeconds"), kDefaultOsdSeconds).toInt();
  if (!IsValidOsdSeconds(osdSeconds_)) osdSeconds_ = kDefaultOsdSeconds;

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
  if (!IsValidQuality(kbps) || defaultQualityKbps_ == kbps) return;
  defaultQualityKbps_ = kbps;
  save();
  emit settingsChanged();
  emit noticeRequested(
      TranslateApp("Qualità predefinita: %1 Mbps").arg(kbps / 1000));
}

void SettingsController::cycleDefaultQuality(int delta) {
  defaultQualityKbps_ = ShiftQuality(defaultQualityKbps_, delta);
  save();
  emit settingsChanged();
  emit noticeRequested(
      TranslateApp("Qualità predefinita: %1 Mbps").arg(defaultQualityKbps_ / 1000));
}

void SettingsController::setOsdSeconds(int seconds) {
  if (!IsValidOsdSeconds(seconds)) return;
  if (osdSeconds_ == seconds) return;
  osdSeconds_ = seconds;
  save();
  emit osdSecondsChanged();
  emit settingsChanged();
  emit noticeRequested(TranslateApp("OSD: %1 secondi").arg(seconds));
}

void SettingsController::cycleOsdSeconds(int delta) {
  osdSeconds_ = ShiftOsdSeconds(osdSeconds_, delta);
  save();
  emit osdSecondsChanged();
  emit settingsChanged();
  emit noticeRequested(TranslateApp("OSD: %1 secondi").arg(osdSeconds_));
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
  emit noticeRequested(TranslateApp("Impostazioni ripristinate"));
}

void SettingsController::testConnection() {
  if (client_->server().isEmpty()) {
    emit noticeRequested(TranslateApp("Nessun server configurato"));
    return;
  }
  emit noticeRequested(TranslateApp("Verifico la connessione…"));
  client_->fetchViews([this](const QJsonDocument& doc, const QString& error) {
    if (!error.isEmpty()) {
      emit statusRequested(error);
      return;
    }
    const int count =
        doc.object().value(QStringLiteral("Items")).toArray().size();
    emit noticeRequested(TranslateApp("Connessione OK · %1 librerie").arg(count));
  });
}
