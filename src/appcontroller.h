#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

#include "authcontroller.h"
#include "jellyfinclient.h"
#include "librarycontroller.h"
#include "playbackcontroller.h"
#include "player.h"
#include "settingscontroller.h"

// Bridge between the Jellyfin API and the QML UI. It exposes lists ready for the views
// (QVariantList of QVariantMap) to avoid maintaining C++ models.
class AppController : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
  Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
  Q_PROPERTY(QString serverLabel READ serverLabel NOTIFY stateChanged)
  Q_PROPERTY(bool insecureHttp READ insecureHttp NOTIFY stateChanged)
  Q_PROPERTY(QString userName READ userName NOTIFY stateChanged)
  Q_PROPERTY(QString userImage READ userImage NOTIFY stateChanged)
  Q_PROPERTY(QVariantMap userStats READ userStats NOTIFY userStatsChanged)

  Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
  Q_PROPERTY(bool searching READ searching NOTIFY serversChanged)

  Q_PROPERTY(QString quickConnectCode READ quickConnectCode NOTIFY quickConnectChanged)
  Q_PROPERTY(QString quickConnectStatus READ quickConnectStatus NOTIFY quickConnectChanged)

  Q_PROPERTY(QVariantList views READ views NOTIFY viewsChanged)
  Q_PROPERTY(QVariantList resume READ resume NOTIFY homeChanged)
  Q_PROPERTY(QVariantList nextUp READ nextUp NOTIFY homeChanged)
  Q_PROPERTY(QVariantList latest READ latest NOTIFY homeChanged)
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

  Q_PROPERTY(QString libraryTitle READ libraryTitle NOTIFY libraryChanged)
  Q_PROPERTY(QVariantList libraryItems READ libraryItems NOTIFY libraryChanged)
  Q_PROPERTY(int libraryIndex READ libraryIndex NOTIFY libraryChanged)

  Q_PROPERTY(QVariantMap detail READ detail NOTIFY detailChanged)

  Q_PROPERTY(bool playerActive READ playerActive NOTIFY playerChanged)
  Q_PROPERTY(bool playerPaused READ playerPaused NOTIFY playerChanged)
  Q_PROPERTY(bool playerStalled READ playerStalled NOTIFY playerChanged)
  Q_PROPERTY(double playerPosition READ playerPosition NOTIFY playerPositionChanged)
  Q_PROPERTY(double playerDuration READ playerDuration NOTIFY playerChanged)
  Q_PROPERTY(QString playerTitle READ playerTitle NOTIFY playerChanged)
  Q_PROPERTY(QString playerStatus READ playerStatus NOTIFY playerChanged)
  Q_PROPERTY(QVariantMap playerStats READ playerStats NOTIFY playerStatsChanged)
  Q_PROPERTY(QVariantMap nextEpisode READ nextEpisode NOTIFY nextEpisodeChanged)
  Q_PROPERTY(bool nextEpisodeVisible READ nextEpisodeVisible NOTIFY playerUiChanged)
  Q_PROPERTY(bool playerMenuOpen READ playerMenuOpen NOTIFY playerMenuChanged)
  Q_PROPERTY(QString playerMenuMode READ playerMenuMode NOTIFY playerMenuChanged)
  Q_PROPERTY(QVariantList playerMenuItems READ playerMenuItems NOTIFY playerMenuChanged)
  Q_PROPERTY(int playerMenuIndex READ playerMenuIndex NOTIFY playerMenuChanged)
  Q_PROPERTY(int audioDelayMs READ audioDelayMs NOTIFY playerMenuChanged)
  // Player OSD: visible for 5 s after the last command, then it fades out.
  Q_PROPERTY(bool osdVisible READ osdVisible NOTIFY osdChanged)
  // OSD navigation: 0 = seek bar, 1 = -10 s, 2 = play/pause,
  // 3 = +30 s, 4 = audio, 5 = subtitles, 6 = quality,
  // 7 = audio delay, 8 = stats.
  Q_PROPERTY(int playerFocus READ playerFocus NOTIFY playerUiChanged)
  Q_PROPERTY(double seekPreview READ seekPreview NOTIFY playerUiChanged)
  Q_PROPERTY(bool seekPending READ seekPending NOTIFY playerUiChanged)
  Q_PROPERTY(bool statsOpen READ statsOpen NOTIFY playerUiChanged)

  Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY tracksChanged)
  Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY tracksChanged)
  Q_PROPERTY(int audioIndex READ audioIndex NOTIFY tracksChanged)
  Q_PROPERTY(int subtitleIndex READ subtitleIndex NOTIFY tracksChanged)
  Q_PROPERTY(int qualityKbps READ qualityKbps NOTIFY tracksChanged)
  Q_PROPERTY(int qualityDefaultKbps READ qualityDefaultKbps NOTIFY tracksChanged)
  Q_PROPERTY(int osdSeconds READ osdSeconds NOTIFY tracksChanged)

  Q_PROPERTY(QVariantList seasons READ seasons NOTIFY seasonsChanged)
  Q_PROPERTY(QString seriesTitle READ seriesTitle NOTIFY seasonsChanged)
  Q_PROPERTY(QString seriesPoster READ seriesPoster NOTIFY seasonsChanged)
  Q_PROPERTY(QVariantList episodes READ episodes NOTIFY episodesChanged)
  Q_PROPERTY(QString seasonTitle READ seasonTitle NOTIFY episodesChanged)
  Q_PROPERTY(QVariantMap seasonInfo READ seasonInfo NOTIFY episodesChanged)

 public:
  // Player OSD focus. Keep the order in sync with PlayerPage.qml.
  enum PlayerAction {
    ActionSeek = 0,
    ActionBack10,
    ActionToggle,
    ActionForward30,
    ActionAudio,
    ActionSubtitles,
    ActionQuality,
    ActionAudioDelay,
    ActionStats,
  };

  explicit AppController(QObject* parent = nullptr);

  // Stop playback and release the process-wide display contexts on exit.
  void shutdown();

  QString state() const { return state_; }
  QString statusMessage() const { return status_; }
  QString notice() const { return notice_; }
  QString serverLabel() const;
  bool insecureHttp() const;
  QString userName() const { return client_.userName(); }
  QString userImage() const;
  QString authHeader() const { return client_.authHeader(); }
  QString serverUrl() const { return client_.server(); }
  QVariantMap userStats() const { return userStats_; }

  QVariantList servers() const;
  bool searching() const;

  QString quickConnectCode() const;
  QString quickConnectStatus() const;

  QVariantList views() const { return library_->views(); }
  QVariantList resume() const { return library_->resume(); }
  QVariantList nextUp() const { return library_->nextUp(); }
  QVariantList latest() const { return library_->latest(); }
  bool loading() const { return library_->loading(); }

  QString libraryTitle() const { return library_->libraryTitle(); }
  QVariantList libraryItems() const { return library_->libraryItems(); }
  int libraryIndex() const { return library_->libraryIndex(); }

  QVariantMap detail() const { return library_->detail(); }
  bool playerActive() const { return playback_->active(); }
  bool playerPaused() const { return playback_->paused(); }
  bool playerStalled() const { return playback_->stalled(); }
  double playerPosition() const { return playback_->position(); }
  double playerDuration() const { return playback_->duration(); }
  QString playerTitle() const { return playback_->title(); }
  QString playerStatus() const { return playback_->status(); }
  QVariantMap playerStats() const { return playback_->stats(); }
  QVariantMap nextEpisode() const { return nextEpisode_; }
  bool nextEpisodeVisible() const;
  bool playerMenuOpen() const { return playerMenuOpen_; }
  QString playerMenuMode() const { return playerMenuMode_; }
  QVariantList playerMenuItems() const { return playerMenuItems_; }
  int playerMenuIndex() const { return playerMenuIndex_; }
  int audioDelayMs() const { return settings_->audioDelayMs(); }
  QVariantList audioTracks() const { return audioTracks_; }
  QVariantList subtitleTracks() const { return subtitleTracks_; }
  int audioIndex() const { return audioIndex_; }
  int subtitleIndex() const { return subtitleIndex_; }
  int qualityKbps() const { return qualityKbps_; }
  int qualityDefaultKbps() const { return settings_->defaultQualityKbps(); }
  int osdSeconds() const { return settings_->osdSeconds(); }
  bool osdVisible() const { return osdVisible_; }
  int playerFocus() const { return playerFocus_; }
  double seekPreview() const { return seekPreview_; }
  bool seekPending() const { return seekPending_; }
  bool statsOpen() const { return statsOpen_; }
  QVariantList seasons() const { return library_->seasons(); }
  QString seriesTitle() const { return library_->seriesTitle(); }
  QString seriesPoster() const { return library_->seriesPoster(); }
  QVariantList episodes() const { return library_->episodes(); }
  QString seasonTitle() const { return library_->seasonTitle(); }
  QVariantMap seasonInfo() const { return library_->seasonInfo(); }

  Q_INVOKABLE void discover();
  Q_INVOKABLE void selectServer(int index);
  Q_INVOKABLE void setManualServer(const QString& url);
  Q_INVOKABLE void loginWithPassword(const QString& user, const QString& pass);
  Q_INVOKABLE void beginQuickConnect();
  Q_INVOKABLE void cancelQuickConnect();
  Q_INVOKABLE void logout();
  Q_INVOKABLE void useSavedServer();
  Q_INVOKABLE void backToServerList();
  Q_INVOKABLE void refreshHome();

  Q_INVOKABLE void openView(int index);
  Q_INVOKABLE void openFavorites();
  Q_INVOKABLE void cycleDefaultQuality(int delta);
  Q_INVOKABLE void cycleOsdSeconds(int delta);
  Q_INVOKABLE void setDefaultQuality(int kbps);
  Q_INVOKABLE void setOsdSeconds(int seconds);
  Q_INVOKABLE void setAudioDelay(int ms);
  Q_INVOKABLE void testConnection();
  Q_INVOKABLE void resetSettings();
  Q_INVOKABLE void refreshUserStats();
  Q_INVOKABLE void openPreviousEpisode();
  Q_INVOKABLE void openItem(const QString& itemId);
  Q_INVOKABLE void playItem(const QString& itemId);
  Q_INVOKABLE void playFromStart();
  Q_INVOKABLE void playSeries(const QString& seriesId);
  Q_INVOKABLE void toggleSeasonPlayed(const QString& seasonId);
  Q_INVOKABLE void toggleSeasonFavorite(const QString& seasonId);
  Q_INVOKABLE void playSeasonFromStart();
  Q_INVOKABLE void startPlayback(const QString& itemId);
  Q_INVOKABLE void stopPlayback();
  Q_INVOKABLE void togglePause();
  Q_INVOKABLE void cycleAudio(int delta);
  Q_INVOKABLE void cycleSubtitle(int delta);
  Q_INVOKABLE void cycleQuality(int delta);
  Q_INVOKABLE void restartPlaybackAtPosition();
  // Navigable OSD (keys routed from main.cpp)
  Q_INVOKABLE void playerNavigate(int dx, int dy, bool autoRepeat = false);
  Q_INVOKABLE void playerConfirm();
  Q_INVOKABLE void playerSkip(int seconds);
  Q_INVOKABLE void playerBack();
  // Input mapping lives outside QML so it is one testable implementation.
  Q_INVOKABLE int normalizeKey(int key, int modifiers) const;
  Q_INVOKABLE bool handlePlayerKey(int key, bool autoRepeat, int modifiers);
  Q_INVOKABLE void toggleStats();
  Q_INVOKABLE void retryPlayback();
  Q_INVOKABLE void playNextEpisode();
  Q_INVOKABLE void dismissNextEpisode();
  Q_INVOKABLE void playerMenuMove(int delta);
  Q_INVOKABLE void playerMenuConfirm();
  Q_INVOKABLE void playerMenuClose();
  Q_INVOKABLE void openSeason(int index);
  Q_INVOKABLE void reopenSeries();
  Q_INVOKABLE void togglePlayed();
  Q_INVOKABLE void toggleFavorite();

 signals:
  void stateChanged();
  void statusChanged();
  void noticeChanged();
  void userStatsChanged();
  void serversChanged();
  void quickConnectChanged();
  void viewsChanged();
  void homeChanged();
  void loadingChanged();
  void libraryChanged();
  void detailChanged();
  void seasonsChanged();
  void episodesChanged();
  void playerChanged();
  void playerPositionChanged();
  void playerStatsChanged();
  void tracksChanged();
  void osdChanged();
  void playerUiChanged();
  void nextEpisodeChanged();
  void playerMenuChanged();

  void error(const QString& message);

 private:
  void showOsd();
  void showNotice(const QString& message);
  void openPlayerMenu(const QString& mode);
  void playEpisode(const QJsonObject& episode);
  void loadNextEpisode(const QString& seriesId, const QString& currentId);
  void commitSeek();
  double maxSeekPosition() const;
  bool isSeekBarFocused() const;
  void updateTracksFromItem(const QJsonObject& item);
  void setState(const QString& s);
  void setStatus(const QString& s);
  void afterLogin();
  void handleLoggedOut();

  JellyfinClient client_;
  std::unique_ptr<AuthController> auth_;
  std::unique_ptr<LibraryController> library_;
  std::unique_ptr<SettingsController> settings_;
  std::unique_ptr<PlaybackController> playback_;

  QString state_ = QStringLiteral("login");
  QString status_;
  QString notice_;
  QVariantMap userStats_;
  QTimer noticeTimer_;

  void startPlaybackAt(const QString& itemId, qint64 startTicks,
                       const QString& title = QString(), double durationSec = -1.0);

  QVariantMap nextEpisode_;
  bool nextEpisodeDismissed_ = false;
  QString pendingSeriesId_;
  bool playerMenuOpen_ = false;
  QString playerMenuMode_;
  QVariantList playerMenuItems_;
  int playerMenuIndex_ = 0;
  bool osdVisible_ = false;
  QTimer osdTimer_;
  int playerFocus_ = 0;
  // consecutive presses on the bar: the step grows (30 s -> 1 -> 5 -> 10 min)
  int seekDirection_ = 0;
  int seekSteps_ = 0;
  double seekPreview_ = 0;
  bool seekPending_ = false;
  bool statsOpen_ = false;
  QVariantList audioTracks_;
  QVariantList subtitleTracks_;
  int audioIndex_ = -1;
  int subtitleIndex_ = -1;
  int qualityKbps_ = 12000;
  int audioDelayBeforeMenu_ = 0;
};

#endif  // APP_CONTROLLER_H
