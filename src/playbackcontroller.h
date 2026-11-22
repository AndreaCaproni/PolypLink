#ifndef PLAYBACK_CONTROLLER_H
#define PLAYBACK_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

class JellyfinClient;
class SettingsController;
class Player;

// Owns the player thread, the Jellyfin playback session, progress reporting and
// the stall watchdog. The OSD/menu remain in AppController; this class exposes
// state and transport actions.
class PlaybackController : public QObject {
  Q_OBJECT

 public:
  PlaybackController(JellyfinClient* client, SettingsController* settings,
                     QObject* parent = nullptr);

  bool hasPlayer() const { return player_ != nullptr; }
  bool active() const { return active_; }
  bool paused() const { return paused_; }
  bool stalled() const { return stalled_; }
  double position() const { return position_; }
  double duration() const { return duration_; }
  QString title() const { return title_; }
  QString status() const { return status_; }
  QVariantMap stats() const { return stats_; }
  QString playingItemId() const { return playingItemId_; }
  quint64 generation() const { return playbackGen_; }

  void startPlaybackAt(const QString& itemId, qint64 startTicks,
                       const QString& title, double durationSec,
                       int audioIndex, int subtitleIndex, int qualityKbps);
  void restartAt(qint64 startTicks, int audioIndex, int subtitleIndex,
                 int qualityKbps);
  bool stopPlayback();
  bool shutdown();
  void togglePause();

 signals:
  void playerChanged();
  void positionChanged(double seconds);
  void statsChanged();
  void failed(const QString& message);
  void finished();
  void stopped();

 private:
  bool stopPlayerThread();
  bool teardownPlayer();

  JellyfinClient* client_;
  SettingsController* settings_;
  Player* player_ = nullptr;

  bool active_ = false;
  bool paused_ = false;
  bool stalled_ = false;
  double position_ = 0.0;
  double duration_ = 0.0;
  QString title_;
  QString status_;
  QVariantMap stats_;
  QString playingItemId_;
  QString mediaSourceId_;
  QString playSessionId_;

  QTimer stallWatch_;
  qint64 lastProgressMs_ = 0;
  double lastProgressPos_ = -1.0;
  qint64 lastReportMs_ = 0;
  bool progressInFlight_ = false;
  quint64 playbackGen_ = 0;
};

#endif  // PLAYBACK_CONTROLLER_H
