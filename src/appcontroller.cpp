#include "appcontroller.h"

#include "redact.h"
#include "quality.h"
#include "serverurl.h"
#include "translation.h"

#include <algorithm>
#include <memory>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>

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

  playback_ = std::make_unique<PlaybackController>(&client_, settings_.get());
  connect(playback_.get(), &PlaybackController::playerChanged, this,
          &AppController::playerChanged);
  connect(playback_.get(), &PlaybackController::positionChanged, this,
          [this](double seconds) {
            if (!seekPending_) seekPreview_ = seconds;
            emit playerPositionChanged();
            emit playerUiChanged();
          });
  connect(playback_.get(), &PlaybackController::statsChanged, this,
          &AppController::playerStatsChanged);
  connect(playback_.get(), &PlaybackController::failed, this,
          [this](const QString& message) {
            setStatus(message);
            nextEpisode_.clear();
            nextEpisodeDismissed_ = false;
            emit nextEpisodeChanged();
            osdVisible_ = false;
            emit osdChanged();
          });
  connect(playback_.get(), &PlaybackController::finished, this, [this]() {
    nextEpisode_.clear();
    nextEpisodeDismissed_ = false;
    emit nextEpisodeChanged();
    QTimer::singleShot(800, this, &AppController::refreshHome);
  });

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
    if (playback_->paused() || !playback_->active()) return;
    if (!osdVisible_) return;
    // With focus on the seek bar the OSD stays still: the user is
    // choosing a point and must not lose the cursor.
    if (playerFocus_ == ActionSeek) return;
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
  if (!playback_->active() || nextEpisode_.isEmpty() || nextEpisodeDismissed_) return false;
  if (playback_->duration() <= 1.0 || playback_->paused()) return false;
  const double remaining = playback_->duration() - playback_->position();
  return remaining > 0.0 && remaining <= 60.0;
}

void AppController::loadNextEpisode(const QString& seriesId,
                                    const QString& currentId) {
  if (seriesId.isEmpty() || currentId.isEmpty()) return;
  const quint64 gen = playback_->generation();
  client_.fetchEpisodes(
      seriesId, QString(),
      [this, gen, currentId](const QJsonDocument& doc, const QString& err) {
        if (gen != playback_->generation() || !playback_->active() || !err.isEmpty()) return;
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
  if (!playback_->active()) return;
  startPlaybackAt(playback_->playingItemId(), static_cast<qint64>(playback_->position() * 1e7));
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
  if (!playback_->active()) return;
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
    addItem(TranslateMain("Spenti"), -1, subtitleIndex_ < 0);
    for (const QVariant& v : subtitleTracks_) {
      const QVariantMap track = v.toMap();
      const int index = track.value(QStringLiteral("index")).toInt();
      addItem(track.value(QStringLiteral("label")).toString(), index,
              index == subtitleIndex_);
    }
  } else if (mode == QLatin1String("quality")) {
    for (int q : kQualityKbps) {
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
  playback_->shutdown();
  nextEpisode_.clear();
  nextEpisodeDismissed_ = false;
  playerMenuOpen_ = false;
  osdVisible_ = false;
  seekPending_ = false;
  statsOpen_ = false;
  emit nextEpisodeChanged();
  emit osdChanged();
  emit playerUiChanged();
  emit playerMenuChanged();
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

void AppController::backToServerList() { auth_->showServers(); }

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
  const bool newItem =
      (!playback_->active() || itemId != playback_->playingItemId());
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
  const QString resolvedTitle =
      title.isEmpty()
          ? library_->detail().value(QStringLiteral("name")).toString()
          : title;
  const double resolvedDuration =
      durationSec > 0.0
          ? durationSec
          : library_->detail().value(QStringLiteral("runtimeTicks")).toDouble() / 1e7;
  playback_->startPlaybackAt(itemId, startTicks, resolvedTitle, resolvedDuration,
                             audioIndex_, subtitleIndex_, qualityKbps_);
  if (newItem && !nextSeriesId.isEmpty()) {
    loadNextEpisode(nextSeriesId, itemId);
  }
}

void AppController::shutdown() {
  if (playback_->shutdown()) {
    Player::releaseAudioContext();
  } else {
    qWarning("[shutdown] player thread still running: display release skipped");
  }
}

void AppController::stopPlayback() {
  nextEpisode_.clear();
  nextEpisodeDismissed_ = false;
  emit nextEpisodeChanged();
  const bool hadPlayer = playback_->stopPlayback();
  if (!hadPlayer) return;
  playerMenuOpen_ = false;
  osdVisible_ = false;
  seekPending_ = false;
  statsOpen_ = false;
  emit playerChanged();
  emit osdChanged();
  emit playerUiChanged();
  emit playerMenuChanged();
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
  qualityKbps_ = ShiftQuality(qualityKbps_, delta);
  emit tracksChanged();
}

void AppController::showOsd() {
  if (!playback_->active()) return;
  if (!seekPending_) seekPreview_ = playback_->position();
  if (!osdVisible_) {
    // When entering the OSD the focus starts on play/pause, not on the slider.
    osdVisible_ = true;
    playerFocus_ = ActionToggle;
    emit osdChanged();
  }
  osdTimer_.start();
  emit playerUiChanged();
}

void AppController::restartPlaybackAtPosition() {
  if (!playback_->hasPlayer()) return;
  startPlaybackAt(playback_->playingItemId(), static_cast<qint64>(playback_->position() * 1e7));
}

void AppController::togglePause() {
  if (!playback_->active()) return;
  playback_->togglePause();
  seekPending_ = false;
  seekPreview_ = playback_->position();
  showOsd();
}

void AppController::toggleStats() {
  if (!playback_->active()) return;
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
  if (!playback_->active()) return;

  const bool wasHidden = !osdVisible_;
  if (wasHidden) {
    osdVisible_ = true;
    playerFocus_ = ActionToggle;
    seekPreview_ = playback_->position();
    seekPending_ = false;
    emit osdChanged();
  }

  // The bar is ABOVE the buttons: down leads to the buttons, up from the bar
  // hides the OSD immediately (video only, without waiting 5 s).
  if (!wasHidden) {
    if (dy < 0) {
      if (playerFocus_ == ActionSeek) {
        seekPending_ = false;
        seekPreview_ = playback_->position();
        osdVisible_ = false;
        osdTimer_.stop();
        emit osdChanged();
        emit playerUiChanged();
        return;
      }
      playerFocus_ = ActionSeek;
    } else if (dy > 0 && playerFocus_ == ActionSeek) {
      // Leaving the slider without OK cancels the pending seek.
      seekPending_ = false;
      seekPreview_ = playback_->position();
      playerFocus_ = ActionToggle;   // play/pause, in the middle of the row
    }
  }

  if (dx != 0) {
    if (playerFocus_ == ActionSeek) {
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

      const double maxPos = playback_->duration() > 1.0 ? playback_->duration() - 1.0 : 0.0;
      seekPreview_ = qBound(0.0, seekPreview_ + step, maxPos);
      if (qAbs(seekPreview_ - playback_->position()) < 0.4) {
        seekPending_ = false;
      } else {
        seekPending_ = true;
      }
    } else {
      playerFocus_ = qBound(static_cast<int>(ActionBack10), playerFocus_ + dx,
                            static_cast<int>(ActionStats));
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
  if (!playback_->active()) return;
  if (!osdVisible_) {
    // First the two "urgent" cards: connection lost and next episode.
    if (playback_->stalled()) {
      retryPlayback();
      return;
    }
    if (nextEpisodeVisible()) {
      playNextEpisode();
      return;
    }
  }
  if (!osdVisible_) playerNavigate(0, 0);

  const double maxPos = playback_->duration() > 1.0 ? playback_->duration() - 1.0 : 0.0;
  switch (playerFocus_) {
    case ActionSeek:
      if (seekPending_) commitSeek();
      else togglePause();
      break;
    case ActionBack10:
      seekPreview_ = qBound(0.0, playback_->position() - 10.0, maxPos);
      seekPending_ = true;
      commitSeek();
      break;
    case ActionToggle:
      togglePause();
      break;
    case ActionForward30:
      seekPreview_ = qBound(0.0, playback_->position() + 30.0, maxPos);
      seekPending_ = true;
      commitSeek();
      break;
    case ActionAudio:
      openPlayerMenu(QStringLiteral("audio"));
      break;
    case ActionSubtitles:
      openPlayerMenu(QStringLiteral("subtitles"));
      break;
    case ActionQuality:
      openPlayerMenu(QStringLiteral("quality"));
      break;
    case ActionAudioDelay:
      openPlayerMenu(QStringLiteral("avdelay"));
      break;
    case ActionStats:
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
  startPlaybackAt(playback_->playingItemId(), ticks);
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
  if (!playback_->active()) return;
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
    seekPreview_ = playback_->position();
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
