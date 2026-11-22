#ifndef SETTINGS_CONTROLLER_H
#define SETTINGS_CONTROLLER_H

#include <QObject>
#include <QString>

class JellyfinClient;

// Persistent settings and connection test: default quality, OSD duration and
// the default audio delay. Playback reads these values; the settings UI edits
// them through AppController.
class SettingsController : public QObject {
  Q_OBJECT

 public:
  explicit SettingsController(JellyfinClient* client, QObject* parent = nullptr);

  int defaultQualityKbps() const { return defaultQualityKbps_; }
  int osdSeconds() const { return osdSeconds_; }
  int audioDelayMs() const { return audioDelayMs_; }

  void setDefaultQuality(int kbps);
  void cycleDefaultQuality(int delta);
  void setOsdSeconds(int seconds);
  void cycleOsdSeconds(int delta);
  void setAudioDelay(int ms);
  // Player-menu editing: preview now, persist when the menu is confirmed.
  void previewAudioDelay(int ms);
  void commitAudioDelay();
  void resetSettings();
  void testConnection();

 signals:
  void settingsChanged();
  void osdSecondsChanged();
  void audioDelayChanged();
  void noticeRequested(const QString& message);
  void statusRequested(const QString& status);

 private:
  void save();

  JellyfinClient* client_;
  int defaultQualityKbps_ = 12000;
  int osdSeconds_ = 5;
  int audioDelayMs_ = 0;
};

#endif  // SETTINGS_CONTROLLER_H
