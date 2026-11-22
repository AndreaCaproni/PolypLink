#include "appcontroller.h"

#include "redact.h"
#include "serverurl.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <algorithm>
#include <memory>
#include <QUuid>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>
#include <QtMath>

namespace {

QString uiText(const char* text) {
  return QCoreApplication::translate("main", text);
}

}  // namespace

AppController::AppController(QObject* parent) : QObject(parent) {
  auth_ = std::make_unique<AuthController>(&client_);
  connect(auth_.get(), &AuthController::stateRequested, this,
          &AppController::setState);
  connect(auth_.get(), &AuthController::statusRequested, this,
          &AppController::setStatus);
  connect(auth_.get(), &AuthController::errorRaised, this, &AppController::error);
  connect(auth_.get(), &AuthController::authenticated, this,
          &AppController::afterLogin);
  connect(auth_.get(), &AuthController::loggedOut, this,
          &AppController::handleLoggedOut);
  connect(auth_.get(), &AuthController::serversChanged, this,
          &AppController::serversChanged);
  connect(auth_.get(), &AuthController::quickConnectChanged, this,
          &AppController::quickConnectChanged);

  library_ = std::make_unique<LibraryController>(&client_);
  connect(library_.get(), &LibraryController::viewsChanged, this,
          &AppController::viewsChanged);
  connect(library_.get(), &LibraryController::homeChanged, this,
          &AppController::homeChanged);
  connect(library_.get(), &LibraryController::libraryChanged, this,
          &AppController::libraryChanged);
  connect(library_.get(), &LibraryController::detailChanged, this,
          &AppController::detailChanged);
  connect(library_.get(), &LibraryController::seasonsChanged, this,
          &AppController::seasonsChanged);
  connect(library_.get(), &LibraryController::episodesChanged, this,
          &AppController::episodesChanged);
  connect(library_.get(), &LibraryController::loadingChanged, this,
          &AppController::loadingChanged);
  connect(library_.get(), &LibraryController::statusRequested, this,
          &AppController::setStatus);
  connect(library_.get(), &LibraryController::noticeRequested, this,
          &AppController::showNotice);
  connect(library_.get(), &LibraryController::homeRefreshRequested, this,
          &AppController::refreshHome);
  connect(library_.get(), &LibraryController::itemLoaded, this,
          &AppController::updateTracksFromItem);

  settings_ = std::make_unique<SettingsController>(&client_);
  connect(settings_.get(), &SettingsController::settingsChanged, this,
          &AppController::tracksChanged);
  connect(settings_.get(), &SettingsController::audioDelayChanged, this,
          &AppController::playerMenuChanged);
  connect(settings_.get(), &SettingsController::osdSecondsChanged, this, [this]() {
    osdTimer_.setInterval(settings_->osdSeconds() * 1000);
  });
  connect(settings_.get(), &SettingsController::noticeRequested, this,
          &AppController::showNotice);
  connect(settings_.get(), &SettingsController::statusRequested, this,
          &AppController::setStatus);

  // Token revoked or expired: don't stay stuck on an empty home screen.
  connect(&client_, &JellyfinClient::unauthorized, this, [this]() {
    if (state_ == QLatin1String("login")) return;
    qWarning("[api] sessione non piu' valida: torno al login");
    logout();
  });

  // The OSD stays for 5 s, then fades out: it stays still during pause and
  // while the settings panel is open.
  osdTimer_.setSingleShot(true);
  osdTimer_.setInterval(settings_->osdSeconds() * 1000);
  connect(&osdTimer_, &QTimer::timeout, this, [this]() {
    if (playerPaused_ || !playerActive_) return;
    if (!osdVisible_) return;
    // With focus on the seek bar the OSD stays still: the user is
    // choosing a point and must not lose the cursor.
    if (playerFocus_ == 0) return;
    osdVisible_ = false;
    emit osdChanged();
  });

  // Seeking only starts when confirmed with A: moving the cursor does
  // not interrupt the video and there is no automatic jump on inactivity.
  noticeTimer_.setSingleShot(true);
  noticeTimer_.setInterval(3000);
  connect(&noticeTimer_, &QTimer::timeout, this, [this]() {
    if (notice_.isEmpty()) return;
    notice_.clear();
    emit noticeChanged();
  });

  // Network down: the player can stay stuck in a fetch and never emit
  // "stalled". This watchdog watches the position and shows the banner after
  // 5 seconds without progress.
  stallWatch_.setInterval(1000);
  connect(&stallWatch_, &QTimer::timeout, this, [this]() {
    if (!playerActive_) {
      stallWatch_.stop();
      return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (playerPaused_) {
      lastProgressMs_ = now;
      return;
    }
    if (now - lastProgressMs_ > 10000 && !playerStalled_) {
      playerStalled_ = true;
      playerStatus_ = tr("Connessione persa… riprovo");
      emit playerChanged();
    }
  });

  if (client_.hasToken()) {
    state_ = QStringLiteral("home");
    afterLogin();
    return;
  }

  QTimer::singleShot(0, auth_.get(), &AuthController::applyConfigFile);
}

QString AppController::serverLabel() const {
  if (!client_.serverName().isEmpty()) return client_.serverName();
  return client_.server();
}

bool AppController::insecureHttp() const {
  return IsInsecureHttpUrl(client_.server());
}

QString AppController::userImage() const { return client_.userImageUrl(); }

bool AppController::nextEpisodeVisible() const {
  if (!playerActive_ || nextEpisode_.isEmpty() || nextEpisodeDismissed_) return false;
  if (playerDuration_ <= 1.0 || playerPaused_) return false;
  const double remaining = playerDuration_ - playerPosition_;
  return remaining > 0.0 && remaining <= 60.0;
}

void AppController::loadNextEpisode(const QString& seriesId,
                                    const QString& currentId) {
  if (seriesId.isEmpty() || currentId.isEmpty()) return;
  const quint64 gen = playbackGen_;
  client_.fetchEpisodes(
      seriesId, QString(),
      [this, gen, currentId](const QJsonDocument& doc, const QString& err) {
        if (gen != playbackGen_ || !playerActive_ || !err.isEmpty()) return;
        QVector<QJsonObject> eps;
        for (const QJsonValue& v : doc.object().value(QStringLiteral("Items")).toArray())
          eps.append(v.toObject());
        std::sort(eps.begin(), eps.end(),
                  [](const QJsonObject& a, const QJsonObject& b) {
                    const int sa = a.value(QStringLiteral("ParentIndexNumber")).toInt();
                    const int sb = b.value(QStringLiteral("ParentIndexNumber")).toInt();
                    if (sa != sb) return sa < sb;
                    return a.value(QStringLiteral("IndexNumber")).toInt() <
                           b.value(QStringLiteral("IndexNumber")).toInt();
                  });
        int pos = -1;
        for (int i = 0; i < eps.size(); ++i) {
          if (eps.at(i).value(QStringLiteral("Id")).toString() == currentId) {
            pos = i;
            break;
          }
        }
        if (pos < 0 || pos + 1 >= eps.size()) {
          qInfo("[player] nessun prossimo episodio");
          return;
        }
        nextEpisode_ = library_->toMap(eps.at(pos + 1));
        nextEpisodeDismissed_ = false;
        qInfo("[player] prossimo episodio: %s",
              qPrintable(nextEpisode_.value(QStringLiteral("name")).toString()));
        emit nextEpisodeChanged();
        emit playerUiChanged();
      });
}

void AppController::retryPlayback() {
  if (!playerActive_) return;
  startPlaybackAt(playingItemId_, static_cast<qint64>(playerPosition_ * 1e7));
}

void AppController::playNextEpisode() {
  if (nextEpisode_.isEmpty()) return;
  const QString id = nextEpisode_.value(QStringLiteral("id")).toString();
  if (id.isEmpty()) return;
  pendingSeriesId_ = nextEpisode_.value(QStringLiteral("seriesId")).toString();
  nextEpisodeDismissed_ = false;
  startPlaybackAt(id, 0);
}

void AppController::dismissNextEpisode() {
  if (nextEpisode_.isEmpty()) return;
  nextEpisodeDismissed_ = true;
  emit playerUiChanged();
}

void AppController::openPlayerMenu(const QString& mode) {
  if (!playerActive_) return;
  playerMenuItems_.clear();
  playerMenuMode_ = mode;

  // The audio delay is not a list: it is a popup with two arrows and 100 ms
  // steps. The value is applied only on confirmation.
  if (mode == QLatin1String("avdelay")) {
    audioDelayBeforeMenu_ = settings_->audioDelayMs();
    playerMenuOpen_ = true;
    osdTimer_.stop();
    if (!osdVisible_) {
      osdVisible_ = true;
      emit osdChanged();
    }
    emit playerMenuChanged();
    emit playerUiChanged();
    return;
  }

  int selected = -1;

  auto addItem = [this, &selected](const QString& label, int value, bool isSelected) {
    QVariantMap item;
    item.insert(QStringLiteral("label"), label);
    item.insert(QStringLiteral("value"), value);
    item.insert(QStringLiteral("selected"), isSelected);
    if (isSelected) selected = playerMenuItems_.size();
    playerMenuItems_.append(item);
  };

  if (mode == QLatin1String("audio")) {
    if (audioTracks_.isEmpty()) return;
    for (const QVariant& v : audioTracks_) {
      const QVariantMap track = v.toMap();
      const int index = track.value(QStringLiteral("index")).toInt();
      addItem(track.value(QStringLiteral("label")).toString(), index,
              index == audioIndex_);
    }
  } else if (mode == QLatin1String("subtitles")) {
    addItem(uiText("Spenti"), -1, subtitleIndex_ < 0);
    for (const QVariant& v : subtitleTracks_) {
      const QVariantMap track = v.toMap();
      const int index = track.value(QStringLiteral("index")).toInt();
      addItem(track.value(QStringLiteral("label")).toString(), index,
              index == subtitleIndex_);
    }
  } else if (mode == QLatin1String("quality")) {
    static const int qualities[] = {4000, 8000, 12000, 20000, 30000, 40000, 50000, 60000, 80000, 100000};
    for (int q : qualities) {
      addItem(QString::number(q / 1000) + QStringLiteral(" Mbps"), q,
              q == qualityKbps_);
    }
  }

  if (playerMenuItems_.isEmpty()) return;
  playerMenuIndex_ = selected >= 0 ? selected : 0;
  playerMenuOpen_ = true;
  osdTimer_.stop();
  if (!osdVisible_) {
    osdVisible_ = true;
    emit osdChanged();
  }
  emit playerMenuChanged();
  emit playerUiChanged();
}

void AppController::playerMenuMove(int delta) {
  if (!playerMenuOpen_ || playerMenuItems_.isEmpty()) return;
  playerMenuIndex_ = std::max(
      0, std::min(playerMenuItems_.size() - 1, playerMenuIndex_ + delta));
  emit playerMenuChanged();
}

void AppController::playerMenuConfirm() {
  if (!playerMenuOpen_ || playerMenuItems_.isEmpty()) return;
  const QVariantMap item = playerMenuItems_.at(playerMenuIndex_).toMap();
  const int value = item.value(QStringLiteral("value")).toInt();
  int current = -2;
  if (playerMenuMode_ == QLatin1String("audio")) {
    current = audioIndex_;
  } else if (playerMenuMode_ == QLatin1String("subtitles")) {
    current = subtitleIndex_;
  } else if (playerMenuMode_ == QLatin1String("quality")) {
    current = qualityKbps_;
  }
  if (value == current) {
    playerMenuClose();
    return;
  }
  if (playerMenuMode_ == QLatin1String("audio")) {
    audioIndex_ = value;
  } else if (playerMenuMode_ == QLatin1String("subtitles")) {
    subtitleIndex_ = value;
  } else if (playerMenuMode_ == QLatin1String("quality")) {
    qualityKbps_ = value;
  }
  emit tracksChanged();
  playerMenuClose();
  restartPlaybackAtPosition();
}

void AppController::playerMenuClose() {
  if (!playerMenuOpen_) return;
  playerMenuOpen_ = false;
  emit playerMenuChanged();
  emit playerUiChanged();
  showOsd();
}

void AppController::openPreviousEpisode() {
  const QString current = library_->detail().value(QStringLiteral("id")).toString();
  const QString seriesId = library_->detail().value(QStringLiteral("seriesId")).toString();
  if (current.isEmpty() || seriesId.isEmpty()) return;
  library_->setLoading(true);
  client_.fetchEpisodes(
      seriesId, QString(),
      [this, current](const QJsonDocument& doc, const QString& err) {
        library_->setLoading(false);
        if (!err.isEmpty()) {
          setStatus(err);
          return;
        }
        QVector<QJsonObject> eps;
        for (const QJsonValue& v : doc.object().value(QStringLiteral("Items")).toArray()) {
          eps.append(v.toObject());
        }
        std::sort(eps.begin(), eps.end(), [](const QJsonObject& a, const QJsonObject& b) {
          const int sa = a.value(QStringLiteral("ParentIndexNumber")).toInt();
          const int sb = b.value(QStringLiteral("ParentIndexNumber")).toInt();
          if (sa != sb) return sa < sb;
          return a.value(QStringLiteral("IndexNumber")).toInt() <
                 b.value(QStringLiteral("IndexNumber")).toInt();
        });
        int pos = -1;
        for (int i = 0; i < eps.size(); ++i) {
          if (eps.at(i).value(QStringLiteral("Id")).toString() == current) {
            pos = i;
            break;
          }
        }
        if (pos <= 0) {
          showNotice(tr("Nessun episodio precedente"));
          return;
        }
        openItem(eps.at(pos - 1).value(QStringLiteral("Id")).toString());
      });
}

void AppController::refreshUserStats() {
  if (client_.userId().isEmpty()) return;
  userStats_.clear();
  emit userStatsChanged();
  struct Def {
    const char* types;
    const char* filters;
    const char* key;
  };
  static const Def defs[] = {
      {"Movie", "", "movies"},
      {"Series", "", "series"},
      {"Episode", "", "episodes"},
      {"Movie,Series", "IsFavorite", "favorites"},
  };
  for (const Def& d : defs) {
    const QString key = QString::fromLatin1(d.key);
    client_.fetchItemCount(
        QString::fromLatin1(d.types), QString::fromLatin1(d.filters),
        [this, key](const QJsonDocument& doc, const QString& err) {
          if (!err.isEmpty()) return;
          userStats_.insert(key,
                            doc.object().value(QStringLiteral("TotalRecordCount")).toInt());
          emit userStatsChanged();
        });
  }
}

QVariantList AppController::servers() const { return auth_->servers(); }

bool AppController::searching() const { return auth_->searching(); }

QString AppController::quickConnectCode() const {
  return auth_->quickConnectCode();
}

QString AppController::quickConnectStatus() const {
  return auth_->quickConnectStatus();
}

void AppController::discover() { auth_->discover(); }

void AppController::setManualServer(const QString& url) {
  auth_->setManualServer(url);
}

void AppController::loginWithPassword(const QString& user, const QString& pass) {
  auth_->loginWithPassword(user, pass);
}

void AppController::selectServer(int index) { auth_->selectServer(index); }

void AppController::beginQuickConnect() { auth_->beginQuickConnect(); }

void AppController::cancelQuickConnect() { auth_->cancelQuickConnect(); }

void AppController::logout() {
  teardownPlayer();
  auth_->logout();
}

void AppController::handleLoggedOut() {
  library_->clear();
  userStats_.clear();
  setStatus(QString());
  // After a logout you choose the server: the saved one stays at the top,
  // so you get back in with one key or switch to another.
  setState(QStringLiteral("login"));
  discover();
}

void AppController::refreshHome() { afterLogin(); }

void AppController::useSavedServer() { auth_->useSavedServer(); }

void AppController::afterLogin() {
  setState(QStringLiteral("home"));
  setStatus(QString());
  library_->loadHome();
}

void AppController::openView(int index) { library_->openView(index); }

void AppController::openFavorites() { library_->openFavorites(); }

void AppController::openItem(const QString& itemId) { library_->openItem(itemId); }

void AppController::openSeason(int index) { library_->openSeason(index); }

void AppController::reopenSeries() { library_->reopenSeries(); }

void AppController::togglePlayed() { library_->togglePlayed(); }

void AppController::toggleFavorite() { library_->toggleFavorite(); }

void AppController::toggleSeasonPlayed(const QString& seasonId) {
  library_->toggleSeasonPlayed(seasonId);
}

void AppController::toggleSeasonFavorite(const QString& seasonId) {
  library_->toggleSeasonFavorite(seasonId);
}

void AppController::updateTracksFromItem(const QJsonObject& item) {
  audioTracks_.clear();
  subtitleTracks_.clear();
  audioIndex_ = -1;
  subtitleIndex_ = -1;
  // starts from the default quality chosen in the settings
  qualityKbps_ = settings_->defaultQualityKbps();
  const QJsonArray sources = item.value(QStringLiteral("MediaSources")).toArray();
  if (!sources.isEmpty()) {
    const QJsonArray streams =
        sources.at(0).toObject().value(QStringLiteral("MediaStreams")).toArray();
    for (const QJsonValue& sv : streams) {
      const QJsonObject st = sv.toObject();
      const QString type = st.value(QStringLiteral("Type")).toString();
      const int index = st.value(QStringLiteral("Index")).toInt();
      const QString lang = st.value(QStringLiteral("Language")).toString();
      const QString codec = st.value(QStringLiteral("Codec")).toString().toUpper();
      if (type == QLatin1String("Audio")) {
        const int channels = st.value(QStringLiteral("Channels")).toInt();
        QString label = lang.isEmpty() ? QStringLiteral("Audio %1").arg(index) : lang;
        if (!codec.isEmpty()) label += QStringLiteral(" · ") + codec;
        if (channels == 6) label += QStringLiteral(" 5.1");
        else if (channels == 8) label += QStringLiteral(" 7.1");
        QVariantMap track;
        track.insert(QStringLiteral("index"), index);
        track.insert(QStringLiteral("label"), label);
        audioTracks_.append(track);
        if (audioIndex_ < 0 || st.value(QStringLiteral("IsDefault")).toBool()) {
          audioIndex_ = index;
        }
      } else if (type == QLatin1String("Subtitle")) {
        QString label = lang.isEmpty() ? tr("Sottotitoli %1").arg(index) : lang;
        const QString title = st.value(QStringLiteral("DisplayTitle")).toString();
        if (!title.isEmpty()) label += QStringLiteral(" · ") + title;
        QVariantMap track;
        track.insert(QStringLiteral("index"), index);
        track.insert(QStringLiteral("label"), label);
        subtitleTracks_.append(track);
      }
    }
  }
  emit tracksChanged();
}

void AppController::playItem(const QString& itemId) { startPlayback(itemId); }

void AppController::playSeries(const QString& seriesId) {
  if (seriesId.isEmpty()) return;
  library_->setLoading(true);
  client_.fetchNextUpForSeries(
      seriesId, [this, seriesId](const QJsonDocument& doc, const QString& err) {
        library_->setLoading(false);
        const QJsonArray items = doc.object().value(QStringLiteral("Items")).toArray();
        if (err.isEmpty() && !items.isEmpty()) {
          playEpisode(items.at(0).toObject());
          return;
        }
        // Series never started or already finished: first unwatched episode.
        client_.fetchEpisodes(
            seriesId, QString(),
            [this](const QJsonDocument& d2, const QString& e2) {
              if (!e2.isEmpty()) {
                setStatus(e2);
                return;
              }
              const QJsonArray eps = d2.object().value(QStringLiteral("Items")).toArray();
              if (eps.isEmpty()) {
                setStatus(tr("La serie non ha episodi"));
                return;
              }
              QJsonObject pick = eps.at(0).toObject();
              for (const QJsonValue& v : eps) {
                const QJsonObject o = v.toObject();
                if (!o.value(QStringLiteral("UserData"))
                         .toObject()
                         .value(QStringLiteral("Played"))
                         .toBool()) {
                  pick = o;
                  break;
                }
              }
              playEpisode(pick);
            });
      });
}

void AppController::playSeasonFromStart() {
  if (library_->episodes().isEmpty()) {
    setStatus(tr("Gli episodi non sono ancora caricati"));
    return;
  }
  const QVariantMap ep = library_->episodes().first().toMap();
  const QString id = ep.value(QStringLiteral("id")).toString();
  if (id.isEmpty()) return;
  QString title = ep.value(QStringLiteral("name")).toString();
  if (!library_->seriesTitle().isEmpty()) title = library_->seriesTitle() + QStringLiteral(" · ") + title;
  const double dur = ep.value(QStringLiteral("runtimeTicks")).toDouble() / 1e7;
  qualityKbps_ = settings_->defaultQualityKbps();
  pendingSeriesId_ = library_->seasonInfo().value(QStringLiteral("seriesId")).toString();
  startPlaybackAt(id, 0, title, dur);
}

void AppController::playEpisode(const QJsonObject& episode) {
  const QString id = episode.value(QStringLiteral("Id")).toString();
  if (id.isEmpty()) return;
  const QString name = episode.value(QStringLiteral("Name")).toString();
  const QString series = episode.value(QStringLiteral("SeriesName")).toString();
  const int season = episode.value(QStringLiteral("ParentIndexNumber")).toInt();
  const int number = episode.value(QStringLiteral("IndexNumber")).toInt();
  QString title = name;
  if (!series.isEmpty()) {
    title = series + QStringLiteral(" · ") + name;
  } else if (season > 0 && number > 0) {
    title = QStringLiteral("S%1 E%2 · %3").arg(season).arg(number).arg(name);
  }
  const qint64 ticks = static_cast<qint64>(
      episode.value(QStringLiteral("UserData"))
          .toObject()
          .value(QStringLiteral("PlaybackPositionTicks"))
          .toDouble());
  const double dur = episode.value(QStringLiteral("RunTimeTicks")).toDouble() / 1e7;
  qualityKbps_ = settings_->defaultQualityKbps();
  pendingSeriesId_ = episode.value(QStringLiteral("SeriesId")).toString();
  qInfo("[player] serie: riproduco %s", qPrintable(title));
  startPlaybackAt(id, ticks, title, dur);
}

void AppController::playFromStart() {
  const QString id = library_->detail().value(QStringLiteral("id")).toString();
  if (id.isEmpty()) return;
  startPlaybackAt(id, 0);
}

void AppController::startPlayback(const QString& itemId) {
  const qint64 resume = library_->detail().value(QStringLiteral("resumeTicks")).toLongLong();
  startPlaybackAt(itemId, resume);
}

void AppController::startPlaybackAt(const QString& itemId, qint64 startTicks,
                                    const QString& title, double durationSec) {
  const bool newItem = (!playerActive_ || itemId != playingItemId_);
  const quint64 gen = ++playbackGen_;
  mediaSourceId_.clear();
  qInfo("[diag] startPlaybackAt gen=%llu ticks=%lld player=%s",
        static_cast<unsigned long long>(gen), static_cast<long long>(startTicks),
        player_ ? "esistente" : "nullo");
  if (player_ != nullptr) {
    QElapsedTimer ot;
    ot.start();
    // Disconnect the signals BEFORE stopping it: otherwise its "finished"
    // is read as end of playback and closes the player we are
    // reopening (settings change / seek).
    QObject::disconnect(player_, nullptr, this, nullptr);
    player_->requestStop();
    const bool joined = player_->wait(8000);
    qInfo("[diag] stop player precedente: %lld ms -> %s",
          static_cast<long long>(ot.elapsed()), joined ? "ok" : "TIMEOUT");
    if (joined) delete player_;
    else qWarning("[player] il thread non termina: non lo cancello");
    player_ = nullptr;
  }

  playSessionId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
  playingItemId_ = itemId;
  playerTitle_ = title.isEmpty() ? library_->detail().value(QStringLiteral("name")).toString()
                                 : title;
  playerDuration_ = durationSec > 0.0
                        ? durationSec
                        : library_->detail().value(QStringLiteral("runtimeTicks")).toDouble() / 1e7;
  playerPosition_ = startTicks / 1e7;
  playerStatus_.clear();
  playerActive_ = true;
  playerPaused_ = false;
  playerStalled_ = false;
  lastProgressPos_ = playerPosition_;
  lastProgressMs_ = QDateTime::currentMSecsSinceEpoch();
  stallWatch_.start();

  QString nextSeriesId = pendingSeriesId_;
  pendingSeriesId_.clear();
  if (nextSeriesId.isEmpty() && newItem &&
      itemId == library_->detail().value(QStringLiteral("id")).toString() &&
      library_->detail().value(QStringLiteral("type")) == QLatin1String("Episode")) {
    nextSeriesId = library_->detail().value(QStringLiteral("seriesId")).toString();
  }
  if (newItem) {
    nextEpisode_.clear();
    nextEpisodeDismissed_ = false;
    emit nextEpisodeChanged();
  }
  if (newItem && !nextSeriesId.isEmpty()) loadNextEpisode(nextSeriesId, itemId);

  emit playerChanged();
  emit playerPositionChanged();

  // PlaybackInfo creates the transcode session with tracks and bitrate. The
  // position does not go through StartTimeTicks (the server propagates it to the
  // segments, which reject it): the player starts from the segment that covers it.
  client_.fetchPlaybackInfo(
      itemId, startTicks, audioIndex_, subtitleIndex_, qualityKbps_,
      [this, itemId, startTicks, gen](const QJsonDocument& doc, const QString& err) {
        if (gen != playbackGen_ || !playerActive_) return;

        QString url;
        if (err.isEmpty()) {
          const QJsonArray sources =
              doc.object().value(QStringLiteral("MediaSources")).toArray();
          if (!sources.isEmpty()) {
            const QJsonObject s0 = sources.at(0).toObject();
            url = s0.value(QStringLiteral("TranscodingUrl")).toString();
            if (url.isEmpty()) url = s0.value(QStringLiteral("DirectStreamUrl")).toString();
            if (!url.isEmpty() && !url.startsWith(QLatin1String("http"))) {
              url = client_.server() + url;
            }
            if (!url.isEmpty()) {
              // The server URL carries *its own* choices (default subtitles
              // burned in, audio track, StartTimeTicks). We rewrite it with
              // ours: without this the server burns in the subtitles even
              // if the user turned them off.
              QUrl u(url);
              QUrlQuery q(u);
              const QStringList managed{
                  QStringLiteral("StartTimeTicks"),
                  QStringLiteral("SubtitleStreamIndex"),
                  QStringLiteral("SubtitleMethod"),
                  QStringLiteral("AudioStreamIndex"),
                  QStringLiteral("MaxWidth"),
                  QStringLiteral("MaxHeight"),
                  QStringLiteral("EnableAutoStreamCopy"),
                  QStringLiteral("AllowVideoStreamCopy"),
                  QStringLiteral("AllowAudioStreamCopy")};
              for (const QString& key : managed) {
                while (q.hasQueryItem(key)) q.removeAllQueryItems(key);
              }
              if (audioIndex_ >= 0) {
                q.addQueryItem(QStringLiteral("AudioStreamIndex"),
                               QString::number(audioIndex_));
              }
              if (subtitleIndex_ >= 0) {
                // selected track: the only possible way on the Link, burn-in
                q.addQueryItem(QStringLiteral("SubtitleStreamIndex"),
                               QString::number(subtitleIndex_));
                q.addQueryItem(QStringLiteral("SubtitleMethod"),
                               QStringLiteral("Encode"));
              } else {
                // off: explicit -1, the server does not pick one on its own
                q.addQueryItem(QStringLiteral("SubtitleStreamIndex"),
                               QStringLiteral("-1"));
              }
              // The Link decoder goes up to 1920x1080: without this limit
              // Jellyfin can send 2560x1072 from 4K sources and SLVideo
              // rejects every frame with "stream macroblock error".
              q.addQueryItem(QStringLiteral("MaxWidth"), QStringLiteral("1920"));
              q.addQueryItem(QStringLiteral("MaxHeight"), QStringLiteral("1080"));
              // The Link only decodes H.264/AAC: we force re-encode to
              // get predictable timestamps and pacing (copy made it stutter).
              q.addQueryItem(QStringLiteral("EnableAutoStreamCopy"), QStringLiteral("false"));
              q.addQueryItem(QStringLiteral("AllowVideoStreamCopy"), QStringLiteral("false"));
              q.addQueryItem(QStringLiteral("AllowAudioStreamCopy"), QStringLiteral("false"));
              // The token travels in the player's Authorization header, not
              // in the query string (which ends up in server/proxy logs).
              q.removeAllQueryItems(QStringLiteral("api_key"));
              q.removeAllQueryItems(QStringLiteral("ApiKey"));
              u.setQuery(q);
              url = u.toString(QUrl::FullyEncoded);
            }
            const QString psid = s0.value(QStringLiteral("PlaySessionId")).toString();
            if (!psid.isEmpty()) playSessionId_ = psid;
            // The Jellyfin dashboard shows the correct session only if it knows
            // which media source it is and that it is a transcode.
            const QString msid = s0.value(QStringLiteral("Id")).toString();
            mediaSourceId_ = msid.isEmpty() ? itemId : msid;
          }
        }
        if (url.isEmpty() && err.isEmpty()) {
          qWarning("[player] PlaybackInfo senza URL, uso il fallback");
          url = client_.buildHlsUrl(itemId, playSessionId_, itemId, qualityKbps_,
                                    startTicks, audioIndex_, subtitleIndex_);
        }
        if (url.isEmpty()) {
          // No "blind" player: black screen for 8 s and then it closes.
          qWarning("[player] avvio annullato: %s",
                   qPrintable(RedactSecrets(err)));
          playerActive_ = false;
          playerPaused_ = false;
                  playerStalled_ = false;
          osdVisible_ = false;
          nextEpisode_.clear();
          nextEpisodeDismissed_ = false;
          setStatus(err.isEmpty() ? tr("Impossibile avviare la riproduzione")
                                  : RedactSecrets(err));
          emit nextEpisodeChanged();
          emit playerChanged();
          emit osdChanged();
          return;
        }
        // The server accepts PlayMethod=Transcode only if the PlaySessionId is
        // the one of the transcode job, that is, the one in the stream URL.
        {
          const QUrlQuery streamQuery{QUrl(url)};
          const QString urlPsid =
              streamQuery.queryItemValue(QStringLiteral("PlaySessionId"));
          if (!urlPsid.isEmpty()) playSessionId_ = urlPsid;
          const QString urlMsid =
              streamQuery.queryItemValue(QStringLiteral("MediaSourceId"));
          if (!urlMsid.isEmpty()) mediaSourceId_ = urlMsid;
        }
        qInfo("[player] sessione: psid=%s msid=%s", qPrintable(playSessionId_),
              qPrintable(mediaSourceId_));
        qInfo("[player] stream: %s", qPrintable(RedactSecrets(url).left(150)));

        player_ = new Player(this);
        connect(player_, &Player::statsChanged, this, [this](const QVariantMap& stats) {
          playerStats_ = stats;
          emit playerStatsChanged();
        });
        connect(player_, &Player::positionChanged, this, [this](double seconds) {
          playerPosition_ = seconds;
          if (qAbs(seconds - lastProgressPos_) > 0.05) {
            lastProgressPos_ = seconds;
            lastProgressMs_ = QDateTime::currentMSecsSinceEpoch();
            if (playerStalled_) {
              playerStalled_ = false;
              playerStatus_.clear();
              emit playerChanged();
            }
          }
          if (!seekPending_) seekPreview_ = seconds;
          emit playerPositionChanged();
          emit playerUiChanged();
          const qint64 now = QDateTime::currentMSecsSinceEpoch();
          if (now - lastReportMs_ > 10000 && !progressInFlight_) {
            lastReportMs_ = now;
            progressInFlight_ = true;
            client_.reportPlaybackProgress(
                playingItemId_, playSessionId_, static_cast<qint64>(seconds * 1e7),
                playerPaused_, mediaSourceId_, audioIndex_, subtitleIndex_,
                [this](const QJsonDocument&, const QString&) {
                  progressInFlight_ = false;
                });
          }
        });
        // The player reports the stall after 5 s, but only the watchdog
        // shows the banner at 10 s. Here we use the signal only to hide it
        // as soon as the data comes back.
        connect(player_, &Player::stalled, this, [this](bool stalled) {
          if (!stalled && playerStalled_) {
            playerStalled_ = false;
            playerStatus_.clear();
            lastProgressMs_ = QDateTime::currentMSecsSinceEpoch();
            emit playerChanged();
          }
        });
        connect(player_, &Player::failed, this, [this](const QString& message) {
          playerStatus_ = tr("Errore: %1").arg(message);
          playerActive_ = false;
          playerStalled_ = false;
          stallWatch_.stop();
          nextEpisode_.clear();
          nextEpisodeDismissed_ = false;
          setStatus(playerStatus_);
          emit nextEpisodeChanged();
          emit playerChanged();
        });
        connect(player_, &Player::finished, this, [this]() {
          if (!playerActive_) return;
          client_.reportPlaybackStopped(playingItemId_, playSessionId_,
                                        static_cast<qint64>(playerPosition_ * 1e7),
                                        mediaSourceId_);
          playerActive_ = false;
          playerStalled_ = false;
          stallWatch_.stop();
          nextEpisode_.clear();
          nextEpisodeDismissed_ = false;
          emit nextEpisodeChanged();
          emit playerChanged();
          // the home must immediately reflect "Continue watching" and the next episode
          QTimer::singleShot(800, this, &AppController::refreshHome);
        });

        Player::Config cfg;
        cfg.masterUrl = url;
        cfg.itemId = itemId;
        cfg.playSessionId = playSessionId_;
        cfg.title = playerTitle_;
        cfg.authHeader = client_.authHeader();
        cfg.startTicks = startTicks;
        cfg.durationTicks = static_cast<qint64>(playerDuration_ * 1e7);
        cfg.maxBitrateKbps = qualityKbps_;
        cfg.audioDelayMs = settings_->audioDelayMs();

        if (mediaSourceId_.isEmpty()) mediaSourceId_ = itemId;
        client_.reportPlaybackStart(itemId, playSessionId_, startTicks,
                                    mediaSourceId_, audioIndex_, subtitleIndex_);
        player_->begin(cfg);
      });
}

void AppController::shutdown() {
  if (teardownPlayer()) {
    Player::releaseDisplay();
  } else {
    qWarning("[shutdown] player thread still running: display release skipped");
  }
}

bool AppController::teardownPlayer() {
  ++playbackGen_;
  stallWatch_.stop();
  playerStalled_ = false;
  if (player_ == nullptr) {
    qInfo("[diag] teardownPlayer: nessun player");
    return true;
  }
  QElapsedTimer t;
  t.start();
  player_->requestStop();
  const bool joined = player_->wait(8000);
  qInfo("[diag] teardownPlayer: wait %lld ms -> %s",
        static_cast<long long>(t.elapsed()), joined ? "ok" : "TIMEOUT");
  if (joined) {
    delete player_;
  } else {
    qWarning("[player] il thread non termina: non lo cancello (evito crash)");
  }
  player_ = nullptr;
  playerActive_ = false;
  playerPaused_ = false;
  playerMenuOpen_ = false;
  osdVisible_ = false;
  seekPending_ = false;
  statsOpen_ = false;
  progressInFlight_ = false;
  emit playerChanged();
  emit osdChanged();
  emit playerUiChanged();
  emit playerMenuChanged();
  return joined;
}

void AppController::stopPlayback() {
  nextEpisode_.clear();
  nextEpisodeDismissed_ = false;
  emit nextEpisodeChanged();
  if (player_ == nullptr) return;
  const qint64 position = static_cast<qint64>(playerPosition_ * 1e7);
  teardownPlayer();
  client_.reportPlaybackStopped(playingItemId_, playSessionId_, position,
                                mediaSourceId_);
  QTimer::singleShot(800, this, &AppController::refreshHome);
}

void AppController::cycleAudio(int delta) {
  if (audioTracks_.isEmpty()) return;
  int pos = 0;
  for (int i = 0; i < audioTracks_.size(); ++i) {
    if (audioTracks_.at(i).toMap().value(QStringLiteral("index")).toInt() == audioIndex_) pos = i;
  }
  pos = (pos + delta + audioTracks_.size()) % audioTracks_.size();
  audioIndex_ = audioTracks_.at(pos).toMap().value(QStringLiteral("index")).toInt();
  emit tracksChanged();
}

void AppController::cycleSubtitle(int delta) {
  // -1 = off, then the tracks
  const int count = subtitleTracks_.size() + 1;
  int pos = 0;
  if (subtitleIndex_ >= 0) {
    for (int i = 0; i < subtitleTracks_.size(); ++i) {
      if (subtitleTracks_.at(i).toMap().value(QStringLiteral("index")).toInt() == subtitleIndex_) pos = i + 1;
    }
  }
  pos = (pos + delta + count) % count;
  subtitleIndex_ = (pos == 0) ? -1
                              : subtitleTracks_.at(pos - 1).toMap().value(QStringLiteral("index")).toInt();
  emit tracksChanged();
}

void AppController::cycleQuality(int delta) {
  static const int qualities[] = {4000, 8000, 12000, 20000, 30000, 40000, 50000, 60000, 80000, 100000};
  int pos = 2;
  for (int i = 0; i < 10; ++i) if (qualities[i] == qualityKbps_) pos = i;
  pos = (pos + delta + 10) % 10;
  qualityKbps_ = qualities[pos];
  emit tracksChanged();
}

void AppController::showOsd() {
  if (!playerActive_) return;
  if (!seekPending_) seekPreview_ = playerPosition_;
  if (!osdVisible_) {
    // When entering the OSD the focus starts on play/pause, not on the slider.
    osdVisible_ = true;
    playerFocus_ = 2;
    emit osdChanged();
  }
  osdTimer_.start();
  emit playerUiChanged();
}

void AppController::restartPlaybackAtPosition() {
  if (player_ == nullptr) return;
  startPlaybackAt(playingItemId_, static_cast<qint64>(playerPosition_ * 1e7));
}

void AppController::togglePause() {
  if (player_ == nullptr) return;

  if (!playerPaused_) {
    // pause: stop sending audio and video, the frame stays frozen
    playerPaused_ = true;
    player_->setPaused(true);
    seekPending_ = false;
    seekPreview_ = playerPosition_;
    showOsd();
    emit playerChanged();
    return;
  }

  // resume: unfreeze the decoder, the stream has stayed open.
  playerPaused_ = false;
  player_->setPaused(false);
  seekPending_ = false;
  seekPreview_ = playerPosition_;
  showOsd();
  emit playerChanged();
}

void AppController::toggleStats() {
  if (!playerActive_) return;
  statsOpen_ = !statsOpen_;
  emit playerUiChanged();
}

void AppController::playerNavigate(int dx, int dy, bool autoRepeat) {
  if (playerMenuOpen_) {
    if (playerMenuMode_ == QLatin1String("avdelay")) {
      if (dx != 0) {
        settings_->previewAudioDelay(settings_->audioDelayMs() + dx * 50);
        emit playerMenuChanged();
      }
      return;
    }
    if (dy != 0) playerMenuMove(dy);
    else if (dx != 0) playerMenuClose();
    return;
  }
  if (!playerActive_) return;

  const bool wasHidden = !osdVisible_;
  if (wasHidden) {
    osdVisible_ = true;
    playerFocus_ = 2;
    seekPreview_ = playerPosition_;
    seekPending_ = false;
    emit osdChanged();
  }

  // The bar is ABOVE the buttons: down leads to the buttons, up from the bar
  // hides the OSD immediately (video only, without waiting 5 s).
  if (!wasHidden) {
    if (dy < 0) {
      if (playerFocus_ == 0) {
        seekPending_ = false;
        seekPreview_ = playerPosition_;
        osdVisible_ = false;
        osdTimer_.stop();
        emit osdChanged();
        emit playerUiChanged();
        return;
      }
      playerFocus_ = 0;
    } else if (dy > 0 && playerFocus_ == 0) {
      // Leaving the slider without OK cancels the pending seek.
      seekPending_ = false;
      seekPreview_ = playerPosition_;
      playerFocus_ = 2;   // play/pause, in the middle of the row
    }
  }

  if (dx != 0) {
    if (playerFocus_ == 0) {
      // Acceleration: consecutive presses in the same direction widen
      // the step. With the stick, which repeats, you can move through even
      // long movies without pressing dozens of times.
      // Acceleration only kicks in with stick auto-repeat.
      // A single press (D-pad or stick just moved) always takes the base step.
      if (dx != seekDirection_) {
        seekSteps_ = 0;
        seekDirection_ = dx;
      } else if (autoRepeat) {
        ++seekSteps_;
      } else {
        seekSteps_ = 0;
      }

      double step;
      // cap at 3 minutes: beyond that you jump too far and lose your place
      if (seekSteps_ < 3) step = dx > 0 ? 30.0 : -10.0;
      else if (seekSteps_ < 6) step = dx > 0 ? 60.0 : -60.0;
      else step = dx > 0 ? 180.0 : -180.0;

      const double maxPos = playerDuration_ > 1.0 ? playerDuration_ - 1.0 : 0.0;
      seekPreview_ = qBound(0.0, seekPreview_ + step, maxPos);
      if (qAbs(seekPreview_ - playerPosition_) < 0.4) {
        seekPending_ = false;
      } else {
        seekPending_ = true;
      }
    } else {
      playerFocus_ = qBound(1, playerFocus_ + dx, 8);
    }
  }

  osdTimer_.start();
  emit playerUiChanged();
}

void AppController::playerConfirm() {
  if (playerMenuOpen_) {
    if (playerMenuMode_ == QLatin1String("avdelay")) {
      const bool changed = settings_->audioDelayMs() != audioDelayBeforeMenu_;
      settings_->commitAudioDelay();
      playerMenuClose();
      if (changed) restartPlaybackAtPosition();
      return;
    }
    playerMenuConfirm();
    return;
  }
  if (!playerActive_) return;
  if (!osdVisible_) {
    // First the two "urgent" cards: connection lost and next episode.
    if (playerStalled_) {
      retryPlayback();
      return;
    }
    if (nextEpisodeVisible()) {
      playNextEpisode();
      return;
    }
  }
  if (!osdVisible_) playerNavigate(0, 0);

  const double maxPos = playerDuration_ > 1.0 ? playerDuration_ - 1.0 : 0.0;
  switch (playerFocus_) {
    case 0:
      if (seekPending_) commitSeek();
      else togglePause();
      break;
    case 1:  // -10 s
      seekPreview_ = qBound(0.0, playerPosition_ - 10.0, maxPos);
      seekPending_ = true;
      commitSeek();
      break;
    case 2:  // play / pause
      togglePause();
      break;
    case 3:  // +30 s
      seekPreview_ = qBound(0.0, playerPosition_ + 30.0, maxPos);
      seekPending_ = true;
      commitSeek();
      break;
    case 4:
      openPlayerMenu(QStringLiteral("audio"));
      break;
    case 5:
      openPlayerMenu(QStringLiteral("subtitles"));
      break;
    case 6:
      openPlayerMenu(QStringLiteral("quality"));
      break;
    case 7:
      openPlayerMenu(QStringLiteral("avdelay"));
      break;
    case 8:
      toggleStats();
      break;
    default:
      break;
  }
  emit playerUiChanged();
}

void AppController::commitSeek() {
  seekSteps_ = 0;
  seekDirection_ = 0;
  if (!seekPending_) return;
  seekPending_ = false;
  const qint64 ticks = static_cast<qint64>(seekPreview_ * 1e7);
  qInfo("[player] seek confermato a %.0f s", seekPreview_);
  startPlaybackAt(playingItemId_, ticks);
}

void AppController::playerBack() {
  if (playerMenuOpen_) {
    if (playerMenuMode_ == QLatin1String("avdelay")) {
      // B also closes by confirming: the set value must not go back to 0
      // when the popup is reopened.
      const bool changed = settings_->audioDelayMs() != audioDelayBeforeMenu_;
      settings_->commitAudioDelay();
      playerMenuClose();
      if (changed) restartPlaybackAtPosition();
      return;
    }
    playerMenuClose();
    return;
  }
  if (!playerActive_) return;
  qInfo("[diag] playerBack: osdVisible=%d focus=%d", osdVisible_ ? 1 : 0,
        playerFocus_);
  if (!osdVisible_ && nextEpisodeVisible()) {
    dismissNextEpisode();
    return;
  }
  if (osdVisible_) {
    seekPending_ = false;
    seekSteps_ = 0;
    seekDirection_ = 0;
    seekPreview_ = playerPosition_;
    osdVisible_ = false;
    osdTimer_.stop();
    emit osdChanged();
    emit playerUiChanged();
    return;
  }
  stopPlayback();
}

void AppController::setState(const QString& s) {
  if (state_ == s) return;
  state_ = s;
  emit stateChanged();
}

void AppController::setDefaultQuality(int kbps) {
  settings_->setDefaultQuality(kbps);
}

void AppController::setOsdSeconds(int seconds) {
  settings_->setOsdSeconds(seconds);
}

void AppController::setAudioDelay(int ms) { settings_->setAudioDelay(ms); }

void AppController::cycleDefaultQuality(int delta) {
  settings_->cycleDefaultQuality(delta);
}

void AppController::cycleOsdSeconds(int delta) {
  settings_->cycleOsdSeconds(delta);
}

void AppController::resetSettings() { settings_->resetSettings(); }

void AppController::testConnection() { settings_->testConnection(); }

void AppController::showNotice(const QString& message) {
  if (message.isEmpty()) return;
  notice_ = message;
  emit noticeChanged();
  noticeTimer_.start();
}

void AppController::setStatus(const QString& s) {
  if (status_ == s) return;
  status_ = s;
  emit statusChanged();
}


