#include "playbackcontroller.h"

#include "jellyfinclient.h"
#include "player.h"
#include "redact.h"
#include "settingscontroller.h"

#include "translation.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

PlaybackController::PlaybackController(JellyfinClient* client,
                                       SettingsController* settings,
                                       QObject* parent)
    : QObject(parent), client_(client), settings_(settings) {
  // The player reports a stall after 5 s; the banner appears after 10 s.
  stallWatch_.setInterval(1000);
  connect(&stallWatch_, &QTimer::timeout, this, [this]() {
    if (!active_) {
      stallWatch_.stop();
      return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (paused_) {
      lastProgressMs_ = now;
      return;
    }
    if (now - lastProgressMs_ > 10000 && !stalled_) {
      stalled_ = true;
      status_ = TranslateApp("Connessione persa… riprovo");
      emit playerChanged();
    }
  });
}

bool PlaybackController::stopPlayerThread() {
  ++playbackGen_;
  stallWatch_.stop();
  if (player_ == nullptr) {
    qInfo("[diag] teardownPlayer: nessun player");
    return true;
  }
  QElapsedTimer timer;
  timer.start();
  // Disconnect first: the player's "finished" must not be read as the end of
  // a playback that we are reopening.
  QObject::disconnect(player_, nullptr, this, nullptr);
  player_->requestStop();
  const bool joined = player_->wait(Player::kStopTimeoutMs);
  qInfo("[diag] teardownPlayer: wait %lld ms -> %s",
        static_cast<long long>(timer.elapsed()), joined ? "ok" : "TIMEOUT");
  if (!joined) {
    qWarning("[player] il thread non termina: mantengo il player e blocco nuovi avvii");
    return false;
  }
  delete player_;
  player_ = nullptr;
  return true;
}

bool PlaybackController::teardownPlayer() {
  const bool joined = stopPlayerThread();
  active_ = false;
  paused_ = false;
  stalled_ = false;
  progressInFlight_ = false;
  emit playerChanged();
  return joined;
}

void PlaybackController::startPlaybackAt(const QString& itemId, qint64 startTicks,
                                         const QString& title, double durationSec,
                                         int audioIndex, int subtitleIndex,
                                         int qualityKbps) {
  qInfo("[diag] startPlaybackAt ticks=%lld player=%s",
        static_cast<long long>(startTicks), player_ ? "esistente" : "nullo");
  if (!stopPlayerThread()) {
    status_ = TranslateApp("Player precedente ancora attivo");
    active_ = false;
    emit playerChanged();
    emit failed(status_);
    return;
  }
  const quint64 gen = ++playbackGen_;
  mediaSourceId_.clear();

  playSessionId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
  playingItemId_ = itemId;
  title_ = title;
  duration_ = durationSec;
  position_ = startTicks / 1e7;
  status_.clear();
  active_ = true;
  paused_ = false;
  stalled_ = false;
  lastProgressPos_ = position_;
  lastProgressMs_ = QDateTime::currentMSecsSinceEpoch();
  stallWatch_.start();

  emit playerChanged();
  emit positionChanged(position_);

  // PlaybackInfo creates the transcode session with tracks and bitrate. The
  // position does not go through StartTimeTicks (the server propagates it to the
  // segments, which reject it): the player starts from the segment that covers it.
  client_->fetchPlaybackInfo(
      itemId, startTicks, audioIndex, subtitleIndex, qualityKbps,
      [this, itemId, startTicks, audioIndex, subtitleIndex, qualityKbps, gen](
          const QJsonDocument& doc, const QString& error) {
        if (gen != playbackGen_ || !active_) return;

        QString url;
        if (error.isEmpty()) {
          const QJsonArray sources =
              doc.object().value(QStringLiteral("MediaSources")).toArray();
          if (!sources.isEmpty()) {
            const QJsonObject source = sources.at(0).toObject();
            url = source.value(QStringLiteral("TranscodingUrl")).toString();
            if (url.isEmpty()) {
              url = source.value(QStringLiteral("DirectStreamUrl")).toString();
            }
            if (!url.isEmpty() && !url.startsWith(QLatin1String("http"))) {
              url = client_->server() + url;
            }
            if (!url.isEmpty()) {
              // The server URL carries *its own* choices (default subtitles
              // burned in, audio track, StartTimeTicks). We rewrite it with
              // ours: without this the server burns in the subtitles even
              // if the user turned them off.
              QUrl parsed(url);
              QUrlQuery query(parsed);
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
                while (query.hasQueryItem(key)) query.removeAllQueryItems(key);
              }
              if (audioIndex >= 0) {
                query.addQueryItem(QStringLiteral("AudioStreamIndex"),
                                   QString::number(audioIndex));
              }
              if (subtitleIndex >= 0) {
                // selected track: the only possible way on the Link, burn-in
                query.addQueryItem(QStringLiteral("SubtitleStreamIndex"),
                                   QString::number(subtitleIndex));
                query.addQueryItem(QStringLiteral("SubtitleMethod"),
                                   QStringLiteral("Encode"));
              } else {
                // off: explicit -1, the server does not pick one on its own
                query.addQueryItem(QStringLiteral("SubtitleStreamIndex"),
                                   QStringLiteral("-1"));
              }
              // The Link decoder goes up to 1920x1080: without this limit
              // Jellyfin can send 2560x1072 from 4K sources and SLVideo
              // rejects every frame with "stream macroblock error".
              query.addQueryItem(QStringLiteral("MaxWidth"), QStringLiteral("1920"));
              query.addQueryItem(QStringLiteral("MaxHeight"), QStringLiteral("1080"));
              // The Link only decodes H.264/AAC: we force re-encode to
              // get predictable timestamps and pacing (copy made it stutter).
              query.addQueryItem(QStringLiteral("EnableAutoStreamCopy"),
                                 QStringLiteral("false"));
              query.addQueryItem(QStringLiteral("AllowVideoStreamCopy"),
                                 QStringLiteral("false"));
              query.addQueryItem(QStringLiteral("AllowAudioStreamCopy"),
                                 QStringLiteral("false"));
              // The token travels in the player's Authorization header, not
              // in the query string (which ends up in server/proxy logs).
              query.removeAllQueryItems(QStringLiteral("api_key"));
              query.removeAllQueryItems(QStringLiteral("ApiKey"));
              parsed.setQuery(query);
              url = parsed.toString(QUrl::FullyEncoded);
            }
            const QString psid =
                source.value(QStringLiteral("PlaySessionId")).toString();
            if (!psid.isEmpty()) playSessionId_ = psid;
            // The Jellyfin dashboard shows the correct session only if it knows
            // which media source it is and that it is a transcode.
            const QString msid = source.value(QStringLiteral("Id")).toString();
            mediaSourceId_ = msid.isEmpty() ? itemId : msid;
          }
        }
        if (url.isEmpty() && error.isEmpty()) {
          qWarning("[player] PlaybackInfo senza URL, uso il fallback");
          url = client_->buildHlsUrl(itemId, playSessionId_, itemId, qualityKbps,
                                     startTicks, audioIndex, subtitleIndex);
        }
        if (url.isEmpty()) {
          // No "blind" player: black screen for 8 s and then it closes.
          qWarning("[player] avvio annullato: %s",
                   qPrintable(RedactSecrets(error)));
          active_ = false;
          paused_ = false;
          stalled_ = false;
          stallWatch_.stop();
          status_ = error.isEmpty()
                        ? TranslateApp("Impossibile avviare la riproduzione")
                        : RedactSecrets(error);
          emit playerChanged();
          emit failed(status_);
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
        qInfo("[player] stream: %s",
              qPrintable(RedactSecrets(url).left(150)));

        player_ = new Player(this);
        connect(player_, &Player::statsChanged, this, [this](const QVariantMap& stats) {
          stats_ = stats;
          emit statsChanged();
        });
        connect(player_, &Player::positionChanged, this,
                [this, audioIndex, subtitleIndex](double seconds) {
          position_ = seconds;
          if (qAbs(seconds - lastProgressPos_) > 0.05) {
            lastProgressPos_ = seconds;
            lastProgressMs_ = QDateTime::currentMSecsSinceEpoch();
            if (stalled_) {
              stalled_ = false;
              status_.clear();
              emit playerChanged();
            }
          }
          emit positionChanged(seconds);
          const qint64 now = QDateTime::currentMSecsSinceEpoch();
          if (now - lastReportMs_ > 10000 && !progressInFlight_) {
            lastReportMs_ = now;
            progressInFlight_ = true;
            client_->reportPlaybackProgress(
                playingItemId_, playSessionId_,
                static_cast<qint64>(seconds * 1e7), paused_, mediaSourceId_,
                audioIndex, subtitleIndex, [this](const QJsonDocument&, const QString&) {
                  progressInFlight_ = false;
                });
          }
        });
        // The player reports the stall after 5 s, but only the watchdog
        // shows the banner at 10 s. Here we use the signal only to hide it
        // as soon as the data comes back.
        connect(player_, &Player::stalled, this, [this](bool stalled) {
          if (!stalled && stalled_) {
            stalled_ = false;
            status_.clear();
            lastProgressMs_ = QDateTime::currentMSecsSinceEpoch();
            emit playerChanged();
          }
        });
        connect(player_, &Player::failed, this, [this](const QString& message) {
          status_ = TranslateApp("Errore: %1").arg(message);
          active_ = false;
          stalled_ = false;
          stallWatch_.stop();
          emit playerChanged();
          emit failed(status_);
        });
        connect(player_, &Player::finished, this, [this]() {
          if (!active_) return;
          client_->reportPlaybackStopped(
              playingItemId_, playSessionId_,
              static_cast<qint64>(position_ * 1e7), mediaSourceId_);
          active_ = false;
          stalled_ = false;
          stallWatch_.stop();
          emit playerChanged();
          emit finished();
        });

        Player::Config cfg;
        cfg.masterUrl = url;
        cfg.itemId = itemId;
        cfg.playSessionId = playSessionId_;
        cfg.title = title_;
        cfg.authHeader = client_->authHeader();
        cfg.startTicks = startTicks;
        cfg.durationTicks = static_cast<qint64>(duration_ * 1e7);
        cfg.maxBitrateKbps = qualityKbps;
        cfg.audioDelayMs = settings_->audioDelayMs();

        if (mediaSourceId_.isEmpty()) mediaSourceId_ = itemId;
        client_->reportPlaybackStart(itemId, playSessionId_, startTicks,
                                     mediaSourceId_, audioIndex, subtitleIndex);
        player_->begin(cfg);
      });
}

bool PlaybackController::stopPlayback() {
  if (player_ == nullptr) return false;
  const qint64 position = static_cast<qint64>(position_ * 1e7);
  client_->reportPlaybackStopped(playingItemId_, playSessionId_, position,
                                 mediaSourceId_);
  teardownPlayer();
  emit stopped();
  return true;
}

bool PlaybackController::shutdown() { return teardownPlayer(); }

void PlaybackController::togglePause() {
  if (!active_) return;
  paused_ = !paused_;
  if (player_ != nullptr) player_->setPaused(paused_);
  emit playerChanged();
}
