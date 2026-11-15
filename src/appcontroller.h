#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "discovery.h"
#include "jellyfinclient.h"
#include "player.h"

// Ponte tra l'API Jellyfin e la UI QML. Espone liste gia' pronte per le viste
// (QVariantList di QVariantMap) per evitare modelli C++ da mantenere.
class AppController : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
  Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
  Q_PROPERTY(QString serverLabel READ serverLabel NOTIFY stateChanged)
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
  Q_PROPERTY(bool playerLoading READ playerLoading NOTIFY playerChanged)
  Q_PROPERTY(bool playerStalled READ playerStalled NOTIFY playerChanged)
  Q_PROPERTY(double playerPosition READ playerPosition NOTIFY playerPositionChanged)
  Q_PROPERTY(double playerDuration READ playerDuration NOTIFY playerChanged)
  Q_PROPERTY(QString playerTitle READ playerTitle NOTIFY playerChanged)
  Q_PROPERTY(QString playerStatus READ playerStatus NOTIFY playerChanged)
  Q_PROPERTY(QVariantMap playerStats READ playerStats NOTIFY playerStatsChanged)
  Q_PROPERTY(QVariantMap nextEpisode READ nextEpisode NOTIFY nextEpisodeChanged)
  Q_PROPERTY(bool nextEpisodeVisible READ nextEpisodeVisible NOTIFY playerUiChanged)
  // OSD del player: visibile per 5 s dopo l'ultimo comando, poi sfuma.
  Q_PROPERTY(bool osdVisible READ osdVisible NOTIFY osdChanged)
  // Navigazione dell'OSD: 0 = barra di seek, 1 = -10 s, 2 = play/pausa,
  // 3 = +30 s, 4 = audio, 5 = sottotitoli, 6 = qualita', 7 = statistiche.
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
  Q_PROPERTY(bool settingsOpen READ settingsOpen NOTIFY settingsChanged)
  Q_PROPERTY(int settingsRow READ settingsRow NOTIFY settingsChanged)

  Q_PROPERTY(QVariantList seasons READ seasons NOTIFY seasonsChanged)
  Q_PROPERTY(QString seriesTitle READ seriesTitle NOTIFY seasonsChanged)
  Q_PROPERTY(QString seriesPoster READ seriesPoster NOTIFY seasonsChanged)
  Q_PROPERTY(QVariantList episodes READ episodes NOTIFY episodesChanged)
  Q_PROPERTY(QString seasonTitle READ seasonTitle NOTIFY episodesChanged)
  Q_PROPERTY(QVariantMap seasonInfo READ seasonInfo NOTIFY episodesChanged)

 public:
  explicit AppController(QObject* parent = nullptr);

  QString state() const { return state_; }
  QString statusMessage() const { return status_; }
  QString notice() const { return notice_; }
  QString serverLabel() const;
  QString userName() const { return client_.userName(); }
  QString userImage() const;
  QVariantMap userStats() const { return userStats_; }

  QVariantList servers() const;
  bool searching() const { return discovery_.searching(); }

  QString quickConnectCode() const { return qcCode_; }
  QString quickConnectStatus() const { return qcStatus_; }

  QVariantList views() const { return views_; }
  QVariantList resume() const { return resume_; }
  QVariantList nextUp() const { return nextUp_; }
  QVariantList latest() const { return latest_; }
  bool loading() const { return loading_; }

  QString libraryTitle() const { return libraryTitle_; }
  QVariantList libraryItems() const { return libraryItems_; }
  int libraryIndex() const { return libraryIndex_; }

  QVariantMap detail() const { return detail_; }
  bool playerActive() const { return playerActive_; }
  bool playerPaused() const { return playerPaused_; }
  bool playerLoading() const { return playerLoading_; }
  bool playerStalled() const { return playerStalled_; }
  double playerPosition() const { return playerPosition_; }
  double playerDuration() const { return playerDuration_; }
  QString playerTitle() const { return playerTitle_; }
  QString playerStatus() const { return playerStatus_; }
  QVariantMap playerStats() const { return playerStats_; }
  QVariantMap nextEpisode() const { return nextEpisode_; }
  bool nextEpisodeVisible() const;
  QVariantList audioTracks() const { return audioTracks_; }
  QVariantList subtitleTracks() const { return subtitleTracks_; }
  int audioIndex() const { return audioIndex_; }
  int subtitleIndex() const { return subtitleIndex_; }
  int qualityKbps() const { return qualityKbps_; }
  int qualityDefaultKbps() const { return defaultQualityKbps_; }
  int osdSeconds() const { return osdSeconds_; }
  bool settingsOpen() const { return settingsOpen_; }
  bool osdVisible() const { return osdVisible_; }
  int playerFocus() const { return playerFocus_; }
  double seekPreview() const { return seekPreview_; }
  bool seekPending() const { return seekPending_; }
  bool statsOpen() const { return statsOpen_; }
  int settingsRow() const { return settingsRow_; }
  QVariantList seasons() const { return seasons_; }
  QString seriesTitle() const { return seriesTitle_; }
  QString seriesPoster() const { return seriesPoster_; }
  QVariantList episodes() const { return episodes_; }
  QString seasonTitle() const { return seasonTitle_; }
  QVariantMap seasonInfo() const { return seasonInfo_; }

  Q_INVOKABLE void discover();
  Q_INVOKABLE void selectServer(int index);
  Q_INVOKABLE void setManualServer(const QString& url);
  Q_INVOKABLE void loginWithPassword(const QString& user, const QString& pass);
  Q_INVOKABLE void beginQuickConnect();
  Q_INVOKABLE void cancelQuickConnect();
  Q_INVOKABLE void logout();
  Q_INVOKABLE void useSavedServer();
  Q_INVOKABLE void refreshHome();

  Q_INVOKABLE void openView(int index);
  Q_INVOKABLE void openFavorites();
  Q_INVOKABLE void cycleDefaultQuality(int delta);
  Q_INVOKABLE void cycleOsdSeconds(int delta);
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
  Q_INVOKABLE void toggleSettings();
  // OSD navigabile (tasti instradati da main.cpp)
  Q_INVOKABLE void playerNavigate(int dx, int dy);
  Q_INVOKABLE void playerConfirm();
  Q_INVOKABLE void playerBack();
  Q_INVOKABLE void toggleStats();
  Q_INVOKABLE void retryPlayback();
  Q_INVOKABLE void playNextEpisode();
  Q_INVOKABLE void dismissNextEpisode();
  Q_INVOKABLE void moveSettings(int delta);
  Q_INVOKABLE void changeSetting(int delta);
  Q_INVOKABLE void closeSettings();
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
  void settingsChanged();
  void osdChanged();
  void playerUiChanged();
  void nextEpisodeChanged();

  void error(const QString& message);

 private:
  void showOsd();
  void showNotice(const QString& message);
  void saveDefaults();
  void playEpisode(const QJsonObject& episode);
  void teardownPlayer();
  void loadNextEpisode(const QString& seriesId, const QString& currentId);
  void loadLatest(quint64 gen);
  void commitSeek();
  void setState(const QString& s);
  void setStatus(const QString& s);
  void setLoading(bool v);
  QVariantMap toMap(const QJsonObject& item) const;
  QVariantList toList(const QJsonArray& items) const;
  void afterLogin();
  void applyConfigFile();

  JellyfinClient client_;
  Discovery discovery_;

  QString state_ = QStringLiteral("login");
  QString status_;
  QString notice_;
  QVariantMap userStats_;
  QTimer noticeTimer_;
  QString qcCode_;
  QString qcStatus_;

  QVariantList views_;
  QVariantList resume_;
  QVariantList nextUp_;
  QVariantList latest_;
  bool loading_ = false;

  QString libraryTitle_;
  QVariantList libraryItems_;
  QString libraryParentId_;
  int libraryIndex_ = -1;

  QVariantMap detail_;
  QVariantList seasons_;
  QString seriesTitle_;
  QString seriesPoster_;
  QVariantList episodes_;
  QString seasonTitle_;
  QVariantMap seasonInfo_;
  QString seriesOverview_;
  QString seriesId_;
  QString currentSeasonId_;

  void patchLists(const QString& itemId, const QString& key, const QVariant& value);
  void startPlaybackAt(const QString& itemId, qint64 startTicks,
                       const QString& title = QString(), double durationSec = -1.0);

  Player* player_ = nullptr;
  bool playerActive_ = false;
  bool playerPaused_ = false;
  bool playerLoading_ = false;
  bool playerStalled_ = false;
  QVariantMap nextEpisode_;
  bool nextEpisodeDismissed_ = false;
  QString pendingSeriesId_;
  bool osdVisible_ = false;
  QTimer osdTimer_;
  int playerFocus_ = 0;
  // scatti consecutivi sulla barra: il passo cresce (30 s -> 1 -> 5 -> 10 min)
  int seekDirection_ = 0;
  int seekSteps_ = 0;
  qint64 lastSeekMoveMs_ = 0;
  double seekPreview_ = 0;
  bool seekPending_ = false;
  bool statsOpen_ = false;
  bool progressInFlight_ = false;
  // Ogni avvio invalida le callback di PlaybackInfo ancora in volo.
  quint64 playbackGen_ = 0;
  // Ogni richiesta asincrona vecchia viene ignorata quando ne parte una nuova.
  quint64 itemGen_ = 0;
  quint64 libraryGen_ = 0;
  quint64 homeGen_ = 0;
  quint64 seasonsGen_ = 0;
  quint64 episodesGen_ = 0;
  QTimer seekTimer_;
  double playerPosition_ = 0.0;
  double playerDuration_ = 0.0;
  QString playerTitle_;
  QString playerStatus_;
  QString mediaSourceId_;
  QString playSessionId_;
  QVariantMap playerStats_;
  QVariantList audioTracks_;
  QVariantList subtitleTracks_;
  int audioIndex_ = -1;
  int subtitleIndex_ = -1;
  int qualityKbps_ = 12000;
  int defaultQualityKbps_ = 12000;
  int osdSeconds_ = 5;
  bool settingsOpen_ = false;
  int settingsRow_ = 0;
  QString playingItemId_;
  qint64 lastReportMs_ = 0;

  QTimer qcTimer_;
  int qcAttempts_ = 0;
  QString qcSecret_;
};

#endif  // APP_CONTROLLER_H
