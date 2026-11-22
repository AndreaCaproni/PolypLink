#ifndef JELLYFIN_CLIENT_H
#define JELLYFIN_CLIENT_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>

// HTTP client for the Jellyfin API. Deliberately minimal: it covers only the
// calls needed by a living-room client (authentication, views, items, images,
// progress). It replicates the parameters used by jellyfin-web, not its code.
class JellyfinClient : public QObject {
  Q_OBJECT

 public:
  using JsonCallback = std::function<void(const QJsonDocument&, const QString&)>;

  explicit JellyfinClient(QObject* parent = nullptr);

  void setServer(const QString& baseUrl, bool store = true);
  QString server() const { return server_; }
  QString serverName() const { return serverName_; }
  void setServerName(const QString& name) { serverName_ = name; save(); }

  bool hasToken() const { return !token_.isEmpty() && !userId_.isEmpty(); }
  QString userId() const { return userId_; }
  QString userName() const { return userName_; }
  QString deviceId() const { return deviceId_; }
  QString token() const { return token_; }

  void authenticate(const QString& username, const QString& password,
                    JsonCallback cb);
  void quickConnectInitiate(JsonCallback cb);
  void quickConnectPoll(const QString& secret, JsonCallback cb);
  void quickConnectAuthenticate(const QString& secret, JsonCallback cb);

  void fetchViews(JsonCallback cb);
  void fetchResume(int limit, JsonCallback cb);
  void fetchNextUp(int limit, JsonCallback cb);
  void fetchNextUpForSeries(const QString& seriesId, JsonCallback cb);
  void fetchFavorites(int limit, JsonCallback cb);
  void fetchLatestFor(const QString& parentId, int limit, JsonCallback cb);
  void fetchLibrary(const QString& parentId, const QString& includeTypes,
                    const QString& sortBy, int limit, int startIndex,
                    JsonCallback cb);
  void fetchItem(const QString& itemId, JsonCallback cb);
  void fetchSeasons(const QString& seriesId, JsonCallback cb);
  void fetchEpisodes(const QString& seriesId, const QString& seasonId, JsonCallback cb);
  void setPlayed(const QString& itemId, bool played, JsonCallback cb);
  void setFavorite(const QString& itemId, bool favorite, JsonCallback cb);
  void fetchPlaybackInfo(const QString& itemId, qint64 startTicks, int audioIndex,
                         int subtitleIndex, int qualityKbps, JsonCallback cb);

  // Registers the playback session on the server (history, "continue
  // watching"). Fire-and-forget calls: errors do not block the player.
  void reportPlaybackStart(const QString& itemId, const QString& playSessionId,
                           qint64 positionTicks, const QString& mediaSourceId,
                           int audioIndex = -1, int subtitleIndex = -1);
  void reportPlaybackProgress(const QString& itemId, const QString& playSessionId,
                              qint64 positionTicks, bool paused,
                              const QString& mediaSourceId, int audioIndex = -1,
                              int subtitleIndex = -1, JsonCallback cb = nullptr);
  void reportPlaybackStopped(const QString& itemId, const QString& playSessionId,
                             qint64 positionTicks, const QString& mediaSourceId);

  // Full authorization header: also needed by the player, which downloads
  // playlists and segments with its own requests.
  QString authHeader() const;
  QString userImageUrl() const;
  void fetchItemCount(const QString& includeTypes, const QString& filters, JsonCallback cb);
  QString imageUrl(const QString& itemId, const QString& type = QStringLiteral("Primary"),
                   int maxWidth = 400, const QString& tag = QString()) const;

  QString buildHlsUrl(const QString& itemId, const QString& playSessionId,
                      const QString& mediaSourceId, int maxBitrate,
                      qint64 startTicks = 0, int audioIndex = -1,
                      int subtitleIndex = -1) const;

  // Saves/loads the session (token, user, server) to disk.
  void load();
  void save() const;
  void logout();

 signals:
  // The token is no longer valid (401): the app must return to login.
  void unauthorized();

 private:
  QNetworkRequest makeRequest(const QString& pathAndQuery, bool withToken) const;
  QNetworkRequest makeRequestUrl(const QUrl& url, bool withToken) const;
  void sendGet(const QString& pathAndQuery, bool withToken, JsonCallback cb);
  void sendPost(const QString& pathAndQuery, const QJsonObject& body, bool withToken,
                JsonCallback cb);
  void sendDelete(const QString& pathAndQuery, bool withToken, JsonCallback cb);
  static QString parseError(const QJsonDocument& doc, const QByteArray& raw, int status);

  QNetworkAccessManager net_;
  QString server_;
  QString serverName_;
  QString token_;
  QString userId_;
  QString userName_;
  QString deviceId_;
  QString clientName_;
  QString clientVersion_;
};

#endif  // JELLYFIN_CLIENT_H
