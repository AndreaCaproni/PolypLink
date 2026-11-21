#include "jellyfinclient.h"

#include "redact.h"
#include "serverurl.h"

#include "version.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

namespace {

constexpr const char* kClientName = "PolypLink";
constexpr const char* kDeviceName = "Steam Link";

// Un server che non risponde non deve bloccare la UI per sempre: Qt 5.14 non
// ha un transfer timeout, quindi lo mettiamo noi per ogni richiesta.
void armTimeout(QNetworkReply* reply, int ms = 10000) {
  auto* timer = new QTimer(reply);
  timer->setSingleShot(true);
  QObject::connect(timer, &QTimer::timeout, reply, [reply]() {
    reply->setProperty("timedOut", true);
    reply->abort();
  });
  timer->start(ms);
}

// Il token e' un segreto: il file di configurazione non deve essere leggibile
// da altri utenti/processi del dispositivo. Sul Link non c'e' un keyring, quindi
// la difesa pratica e' 0600 sul file e 0700 sulla directory dell'app.
void SecureSettingsFile(QSettings& settings) {
  settings.sync();
  const QString path = settings.fileName();
  if (path.isEmpty()) return;

  const QFileInfo info(path);
  if (info.dir().exists()) {
    QFile::setPermissions(info.dir().absolutePath(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                              QFileDevice::ExeOwner);
  }
  if (info.exists()) {
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  }
}

}  // namespace

JellyfinClient::JellyfinClient(QObject* parent) : QObject(parent) {
  clientName_ = QString::fromLatin1(kClientName);
  clientVersion_ = QString::fromLatin1(APP_VERSION);

  QSettings s;
  deviceId_ = s.value(QStringLiteral("deviceId")).toString();
  if (deviceId_.isEmpty()) {
    deviceId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.setValue(QStringLiteral("deviceId"), deviceId_);
  }
  SecureSettingsFile(s);
  load();
}

void JellyfinClient::load() {
  QSettings s;
  server_ = NormalizeServerUrl(s.value(QStringLiteral("server")).toString());
  serverName_ = s.value(QStringLiteral("serverName")).toString();
  token_ = s.value(QStringLiteral("token")).toString();
  userId_ = s.value(QStringLiteral("userId")).toString();
  userName_ = s.value(QStringLiteral("userName")).toString();
  SecureSettingsFile(s);
  if (server_.startsWith(QLatin1String("http://"))) {
    qWarning("[security] connessione HTTP non cifrata verso %s",
             qPrintable(RedactSecrets(server_)));
  }
}

void JellyfinClient::save() const {
  QSettings s;
  s.setValue(QStringLiteral("server"), server_);
  s.setValue(QStringLiteral("serverName"), serverName_);
  s.setValue(QStringLiteral("token"), token_);
  s.setValue(QStringLiteral("userId"), userId_);
  s.setValue(QStringLiteral("userName"), userName_);
  SecureSettingsFile(s);
}

void JellyfinClient::logout() {
  // Revoca la sessione lato server prima di cancellare il token locale.
  if (!token_.isEmpty()) {
    sendPost(QStringLiteral("/Sessions/Logout"), QJsonObject(), true, nullptr);
  }
  token_.clear();
  userId_.clear();
  userName_.clear();
  save();
}

void JellyfinClient::setServer(const QString& baseUrl, bool store) {
  const QString next = NormalizeServerUrl(baseUrl);
  if (!server_.isEmpty() && !next.isEmpty() && next != server_ &&
      !token_.isEmpty()) {
    // Un token vale solo per il server che lo ha emesso.
    logout();
  }
  server_ = next;
  if (server_.startsWith(QLatin1String("http://"))) {
    qWarning("[security] connessione HTTP non cifrata verso %s",
             qPrintable(RedactSecrets(server_)));
  }
  if (store) save();
}

QString JellyfinClient::authHeader() const {
  QString auth = QStringLiteral(
                     "MediaBrowser Client=\"%1\", Device=\"%2\", DeviceId=\"%3\", "
                     "Version=\"%4\"")
                     .arg(clientName_, QString::fromLatin1(kDeviceName), deviceId_,
                          clientVersion_);
  if (!token_.isEmpty()) auth += QStringLiteral(", Token=\"%1\"").arg(token_);
  return auth;
}

QNetworkRequest JellyfinClient::makeRequestUrl(const QUrl& url, bool withToken) const {
  QNetworkRequest req(url);
  QString auth = authHeader();
  if (!withToken && !token_.isEmpty()) {
    // il token non va usato per le chiamate di login: lo togliamo
    auth = auth.left(auth.indexOf(QStringLiteral(", Token=")));
  }
  req.setRawHeader("Authorization", auth.toUtf8());
  req.setRawHeader("Accept", "application/json");
  // La lingua dell'interfaccia viene comunicata anche al server: localizza i
  // messaggi generati dall'API (non i metadati delle schede, che dipendono
  // dalla lingua impostata sulle librerie Jellyfin).
  {
    QSettings settings;
    const QString lang = settings.value(QStringLiteral("language"),
                                       QStringLiteral("it")).toString();
    const QByteArray acceptLanguage =
        lang == QLatin1String("en")
            ? QByteArrayLiteral("en-US,en;q=0.9")
            : (lang + QStringLiteral(",en;q=0.8")).toUtf8();
    req.setRawHeader("Accept-Language", acceptLanguage);
  }
  // I redirect sono accettati solo per le chiamate senza token. Su una
  // richiesta autenticata Qt copierebbe l'header Authorization anche verso
  // un altro host.
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, !withToken);
  return req;
}

QNetworkRequest JellyfinClient::makeRequest(const QString& pathAndQuery, bool withToken) const {
  return makeRequestUrl(QUrl(server_ + pathAndQuery), withToken);
}

void JellyfinClient::sendGet(const QString& pathAndQuery, bool withToken, JsonCallback cb) {
  if (server_.isEmpty()) {
    if (cb) cb(QJsonDocument(), tr("server non configurato"));
    return;
  }
  auto* reply = net_.get(makeRequest(pathAndQuery, withToken));
  armTimeout(reply);
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, cb, pathAndQuery, withToken]() {
    reply->deleteLater();
    const QByteArray raw = reply->readAll();
    if (reply->property("timedOut").toBool()) {
      qWarning("[api] timeout GET %s", qPrintable(RedactSecrets(pathAndQuery).left(160)));
      if (cb) cb(QJsonDocument(), tr("il server non risponde (timeout)"));
      return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (status == 401 && withToken) emit unauthorized();
    if (reply->error() != QNetworkReply::NoError || status >= 400) {
      qWarning("[api] HTTP %d GET %s", status, qPrintable(RedactSecrets(pathAndQuery).left(160)));
      if (cb) cb(doc, parseError(doc, raw, status));
      return;
    }
    if (cb) cb(doc, QString());
  });
}

void JellyfinClient::sendPost(const QString& pathAndQuery, const QJsonObject& body,
                              bool withToken, JsonCallback cb) {
  if (server_.isEmpty()) {
    if (cb) cb(QJsonDocument(), tr("server non configurato"));
    return;
  }
  QNetworkRequest req = makeRequest(pathAndQuery, withToken);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  auto* reply = net_.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
  armTimeout(reply);
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, cb, pathAndQuery, withToken]() {
    reply->deleteLater();
    const QByteArray raw = reply->readAll();
    if (reply->property("timedOut").toBool()) {
      qWarning("[api] timeout POST %s", qPrintable(RedactSecrets(pathAndQuery).left(160)));
      if (cb) cb(QJsonDocument(), tr("il server non risponde (timeout)"));
      return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (status == 401 && withToken) emit unauthorized();
    if (reply->error() != QNetworkReply::NoError || status >= 400) {
      qWarning("[api] HTTP %d POST %s", status, qPrintable(RedactSecrets(pathAndQuery).left(160)));
      if (cb) cb(doc, parseError(doc, raw, status));
      return;
    }
    if (cb) cb(doc, QString());
  });
}

void JellyfinClient::sendDelete(const QString& pathAndQuery, bool withToken,
                                JsonCallback cb) {
  if (server_.isEmpty()) {
    if (cb) cb(QJsonDocument(), tr("server non configurato"));
    return;
  }
  auto* reply = net_.deleteResource(makeRequest(pathAndQuery, withToken));
  armTimeout(reply);
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, cb, pathAndQuery, withToken]() {
    reply->deleteLater();
    const QByteArray raw = reply->readAll();
    if (reply->property("timedOut").toBool()) {
      qWarning("[api] timeout DELETE %s", qPrintable(RedactSecrets(pathAndQuery).left(160)));
      if (cb) cb(QJsonDocument(), tr("il server non risponde (timeout)"));
      return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (status == 401 && withToken) emit unauthorized();
    if (reply->error() != QNetworkReply::NoError || status >= 400) {
      qWarning("[api] HTTP %d DELETE %s", status, qPrintable(RedactSecrets(pathAndQuery).left(160)));
      if (cb) cb(doc, parseError(doc, raw, status));
      return;
    }
    if (cb) cb(doc, QString());
  });
}

QString JellyfinClient::parseError(const QJsonDocument& doc, const QByteArray& raw,
                                   int status) {
  // Messaggi utili invece di "errore HTTP 0". 401 e 403 hanno gia' il loro
  // percorso (logout/login), qui servono per la UI.
  if (status == 0) return tr("Server non raggiungibile");
  if (status == 401) return tr("Sessione scaduta");
  if (status == 403) return tr("Accesso negato");
  if (status == 404) return tr("Risorsa non trovata sul server");
  if (status >= 500) return tr("Errore del server (%1)").arg(status);

  const QJsonObject o = doc.object();
  for (const char* key : {"Message", "message", "error", "Error"}) {
    const QString v = o.value(QLatin1String(key)).toString();
    if (!v.isEmpty()) return v;
  }
  const QString text = QString::fromUtf8(raw).trimmed();
  if (!text.isEmpty() && text.size() < 200) return text;
  return tr("errore HTTP %1").arg(status);
}

void JellyfinClient::authenticate(const QString& username, const QString& password,
                                  JsonCallback cb) {
  QJsonObject body;
  body.insert(QStringLiteral("Username"), username);
  body.insert(QStringLiteral("Pw"), password);
  sendPost(QStringLiteral("/Users/AuthenticateByName"), body, false,
           [this, cb](const QJsonDocument& doc, const QString& err) {
             if (!err.isEmpty()) {
               if (cb) cb(doc, err);
               return;
             }
             const QJsonObject o = doc.object();
             token_ = o.value(QStringLiteral("AccessToken")).toString();
             const QJsonObject user = o.value(QStringLiteral("User")).toObject();
             userId_ = user.value(QStringLiteral("Id")).toString();
             userName_ = user.value(QStringLiteral("Name")).toString();
             save();
             if (cb) cb(doc, QString());
           });
}

void JellyfinClient::quickConnectInitiate(JsonCallback cb) {
  sendPost(QStringLiteral("/QuickConnect/Initiate"), QJsonObject(), false, cb);
}

void JellyfinClient::quickConnectPoll(const QString& secret, JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("secret"), secret);
  sendGet(QStringLiteral("/QuickConnect/Connect?") + q.toString(QUrl::FullyEncoded), false,
          cb);
}

void JellyfinClient::quickConnectAuthenticate(const QString& secret, JsonCallback cb) {
  QJsonObject body;
  body.insert(QStringLiteral("Secret"), secret);
  sendPost(QStringLiteral("/Users/AuthenticateWithQuickConnect"), body, false,
           [this, cb](const QJsonDocument& doc, const QString& err) {
             if (!err.isEmpty()) {
               if (cb) cb(doc, err);
               return;
             }
             const QJsonObject o = doc.object();
             token_ = o.value(QStringLiteral("AccessToken")).toString();
             const QJsonObject user = o.value(QStringLiteral("User")).toObject();
             userId_ = user.value(QStringLiteral("Id")).toString();
             userName_ = user.value(QStringLiteral("Name")).toString();
             save();
             if (cb) cb(doc, QString());
           });
}

void JellyfinClient::fetchViews(JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  sendGet(QStringLiteral("/UserViews?") + q.toString(QUrl::FullyEncoded), true, cb);
}

void JellyfinClient::fetchResume(int limit, JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
  q.addQueryItem(QStringLiteral("mediaTypes"), QStringLiteral("Video"));
  q.addQueryItem(QStringLiteral("fields"),
                 QStringLiteral("PrimaryImageAspectRatio,Overview,ProductionYear,"
                                "RunTimeTicks,SeriesPrimaryImageTag"));
  q.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Thumb"));
  sendGet(QStringLiteral("/UserItems/Resume?") + q.toString(QUrl::FullyEncoded), true, cb);
}

void JellyfinClient::fetchNextUp(int limit, JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
  q.addQueryItem(QStringLiteral("fields"),
                 QStringLiteral("PrimaryImageAspectRatio,Overview,ProductionYear,"
                                "RunTimeTicks,SeriesPrimaryImageTag"));
  q.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Thumb"));
  sendGet(QStringLiteral("/Shows/NextUp?") + q.toString(QUrl::FullyEncoded), true, cb);
}

void JellyfinClient::fetchNextUpForSeries(const QString& seriesId, JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  q.addQueryItem(QStringLiteral("seriesId"), seriesId);
  q.addQueryItem(QStringLiteral("limit"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("fields"), QStringLiteral("RunTimeTicks"));
  sendGet(QStringLiteral("/Shows/NextUp?") + q.toString(QUrl::FullyEncoded), true, cb);
}

void JellyfinClient::fetchFavorites(int limit, JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  q.addQueryItem(QStringLiteral("Filters"), QStringLiteral("IsFavorite"));
  q.addQueryItem(QStringLiteral("Recursive"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("IncludeItemTypes"), QStringLiteral("Movie,Series"));
  q.addQueryItem(QStringLiteral("SortBy"), QStringLiteral("SortName"));
  q.addQueryItem(QStringLiteral("Limit"), QString::number(limit));
  q.addQueryItem(QStringLiteral("fields"),
                 QStringLiteral("PrimaryImageAspectRatio,Overview,ProductionYear,"
                                "RunTimeTicks,SeriesPrimaryImageTag"));
  q.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Thumb"));
  sendGet(QStringLiteral("/Users/") + userId_ + QStringLiteral("/Items?") +
              q.toString(QUrl::FullyEncoded),
          true, cb);
}

void JellyfinClient::fetchLatestFor(const QString& parentId, int limit,
                                    JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  q.addQueryItem(QStringLiteral("parentId"), parentId);
  q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
  q.addQueryItem(QStringLiteral("fields"),
                 QStringLiteral("PrimaryImageAspectRatio,Overview,ProductionYear,"
                                "RunTimeTicks,DateCreated"));
  q.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Thumb"));
  sendGet(QStringLiteral("/Items/Latest?") + q.toString(QUrl::FullyEncoded), true, cb);
}

void JellyfinClient::fetchLibrary(const QString& parentId, const QString& includeTypes,
                                  const QString& sortBy, int limit, int startIndex,
                                  JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  q.addQueryItem(QStringLiteral("parentId"), parentId);
  q.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("includeItemTypes"), includeTypes);
  q.addQueryItem(QStringLiteral("sortBy"), sortBy);
  q.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("Ascending"));
  q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
  q.addQueryItem(QStringLiteral("startIndex"), QString::number(startIndex));
  q.addQueryItem(QStringLiteral("fields"),
                 QStringLiteral("PrimaryImageAspectRatio,Overview,ProductionYear,"
                                "RunTimeTicks"));
  q.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary,Thumb"));
  sendGet(QStringLiteral("/Users/%1/Items?").arg(userId_) + q.toString(QUrl::FullyEncoded),
          true, cb);
}

void JellyfinClient::fetchItem(const QString& itemId, JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  q.addQueryItem(QStringLiteral("fields"), QStringLiteral("MediaSources"));
  sendGet(QStringLiteral("/Users/%1/Items/%2?").arg(userId_, itemId) +
              q.toString(QUrl::FullyEncoded),
          true, cb);
}

void JellyfinClient::fetchSeasons(const QString& seriesId, JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  q.addQueryItem(QStringLiteral("fields"),
                 QStringLiteral("ItemCounts,PrimaryImageAspectRatio,Overview,Genres"));
  sendGet(QStringLiteral("/Shows/%1/Seasons?").arg(seriesId) +
              q.toString(QUrl::FullyEncoded),
          true, cb);
}

void JellyfinClient::fetchEpisodes(const QString& seriesId, const QString& seasonId,
                                   JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  if (!seasonId.isEmpty()) q.addQueryItem(QStringLiteral("seasonId"), seasonId);
  q.addQueryItem(QStringLiteral("fields"),
                 QStringLiteral("Overview,PrimaryImageAspectRatio,RunTimeTicks,"
                                "ProductionYear"));
  q.addQueryItem(QStringLiteral("imageTypeLimit"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("enableImageTypes"), QStringLiteral("Primary"));
  sendGet(QStringLiteral("/Shows/%1/Episodes?").arg(seriesId) +
              q.toString(QUrl::FullyEncoded),
          true, cb);
}

void JellyfinClient::setPlayed(const QString& itemId, bool played, JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  const QString path = QStringLiteral("/UserPlayedItems/%1?").arg(itemId) +
                       q.toString(QUrl::FullyEncoded);
  if (played) {
    sendPost(path, QJsonObject(), true, cb);
  } else {
    sendDelete(path, true, cb);
  }
}

void JellyfinClient::setFavorite(const QString& itemId, bool favorite, JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  const QString path = QStringLiteral("/UserFavoriteItems/%1?").arg(itemId) +
                       q.toString(QUrl::FullyEncoded);
  if (favorite) {
    sendPost(path, QJsonObject(), true, cb);
  } else {
    sendDelete(path, true, cb);
  }
}

void JellyfinClient::fetchPlaybackInfo(const QString& itemId, qint64 startTicks,
                                       int audioIndex, int subtitleIndex, int qualityKbps,
                                       JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);

  // Profilo che obbliga la transcodifica in H.264/AAC dentro HLS: il decoder
  // del Link non gestisce altro, e senza profilo Jellyfin sceglierebbe il copy.
  QJsonObject transcode;
  transcode.insert(QStringLiteral("Container"), QStringLiteral("ts"));
  transcode.insert(QStringLiteral("Type"), QStringLiteral("Video"));
  transcode.insert(QStringLiteral("VideoCodec"), QStringLiteral("h264"));
  transcode.insert(QStringLiteral("AudioCodec"), QStringLiteral("aac"));
  transcode.insert(QStringLiteral("Protocol"), QStringLiteral("hls"));
  transcode.insert(QStringLiteral("Context"), QStringLiteral("Streaming"));
  transcode.insert(QStringLiteral("MaxAudioChannels"), QStringLiteral("2"));
  transcode.insert(QStringLiteral("MinSegments"), 1);
  transcode.insert(QStringLiteral("BreakOnNonKeyFrames"), true);

  QJsonArray subtitleProfiles;
  for (const char* fmt : {"srt", "subrip", "ass", "ssa", "pgssub", "dvdsub", "vtt"}) {
    QJsonObject sp;
    sp.insert(QStringLiteral("Format"), QString::fromLatin1(fmt));
    // "External" = mai burn-in automatico: il master non allega sottotitoli e
    // il Link non li decodifica, quindi restano spenti. Quando l'utente sceglie
    // una traccia la chiediamo esplicitamente con SubtitleMethod=Encode.
    sp.insert(QStringLiteral("Method"), QStringLiteral("External"));
    subtitleProfiles.append(sp);
  }

  // Il decoder del Link non supera 1920x1080: dichiararlo anche nel profilo,
  // cosi' Jellyfin non sceglie 2560x1072 per le sorgenti 4K.
  QJsonArray videoConditions;
  {
    QJsonObject w;
    w.insert(QStringLiteral("Condition"), QStringLiteral("LessThanEqual"));
    w.insert(QStringLiteral("Property"), QStringLiteral("Width"));
    w.insert(QStringLiteral("Value"), QStringLiteral("1920"));
    w.insert(QStringLiteral("IsRequired"), false);
    videoConditions.append(w);
    QJsonObject h;
    h.insert(QStringLiteral("Condition"), QStringLiteral("LessThanEqual"));
    h.insert(QStringLiteral("Property"), QStringLiteral("Height"));
    h.insert(QStringLiteral("Value"), QStringLiteral("1080"));
    h.insert(QStringLiteral("IsRequired"), false);
    videoConditions.append(h);
  }
  QJsonObject codecProfile;
  codecProfile.insert(QStringLiteral("Type"), QStringLiteral("Video"));
  codecProfile.insert(QStringLiteral("Codec"), QStringLiteral("h264"));
  codecProfile.insert(QStringLiteral("Conditions"), videoConditions);

  QJsonObject profile;
  profile.insert(QStringLiteral("Name"), QStringLiteral("polyplink"));
  profile.insert(QStringLiteral("MaxStreamingBitrate"), qualityKbps * 1000);
  profile.insert(QStringLiteral("DirectPlayProfiles"), QJsonArray());
  profile.insert(QStringLiteral("TranscodingProfiles"), QJsonArray{transcode});
  profile.insert(QStringLiteral("CodecProfiles"), QJsonArray{codecProfile});
  profile.insert(QStringLiteral("SubtitleProfiles"), subtitleProfiles);

  QJsonObject body;
  body.insert(QStringLiteral("UserId"), userId_);
  body.insert(QStringLiteral("StartTimeTicks"), static_cast<double>(startTicks));
  body.insert(QStringLiteral("MaxStreamingBitrate"), qualityKbps * 1000);
  body.insert(QStringLiteral("EnableDirectPlay"), false);
  body.insert(QStringLiteral("EnableDirectStream"), false);
  body.insert(QStringLiteral("EnableTranscoding"), true);
  body.insert(QStringLiteral("DeviceProfile"), profile);
  if (audioIndex >= 0) body.insert(QStringLiteral("AudioStreamIndex"), audioIndex);
  // -1 va inviato esplicitamente: omettendolo il server sceglie la traccia
  // predefinita e la brucia nel video.
  body.insert(QStringLiteral("SubtitleStreamIndex"), subtitleIndex);

  sendPost(QStringLiteral("/Items/%1/PlaybackInfo?").arg(itemId) +
               q.toString(QUrl::FullyEncoded),
           body, true, cb);
}

void JellyfinClient::reportPlaybackStart(const QString& itemId,
                                         const QString& playSessionId,
                                         qint64 positionTicks,
                                         const QString& mediaSourceId,
                                         int audioIndex, int subtitleIndex) {
  QJsonObject o;
  o.insert(QStringLiteral("ItemId"), itemId);
  o.insert(QStringLiteral("PlaySessionId"), playSessionId);
  o.insert(QStringLiteral("MediaSourceId"), mediaSourceId);
  o.insert(QStringLiteral("PlayMethod"), QStringLiteral("Transcode"));
  o.insert(QStringLiteral("PositionTicks"), static_cast<double>(positionTicks));
  o.insert(QStringLiteral("CanSeek"), true);
  o.insert(QStringLiteral("IsPaused"), false);
  o.insert(QStringLiteral("IsMuted"), false);
  if (audioIndex >= 0) o.insert(QStringLiteral("AudioStreamIndex"), audioIndex);
  if (subtitleIndex >= 0) o.insert(QStringLiteral("SubtitleStreamIndex"), subtitleIndex);
  sendPost(QStringLiteral("/Sessions/Playing"), o, true, nullptr);
}

void JellyfinClient::reportPlaybackProgress(const QString& itemId,
                                            const QString& playSessionId,
                                            qint64 positionTicks, bool paused,
                                            const QString& mediaSourceId,
                                            int audioIndex, int subtitleIndex,
                                            JsonCallback cb) {
  QJsonObject o;
  o.insert(QStringLiteral("ItemId"), itemId);
  o.insert(QStringLiteral("PlaySessionId"), playSessionId);
  o.insert(QStringLiteral("MediaSourceId"), mediaSourceId);
  o.insert(QStringLiteral("PlayMethod"), QStringLiteral("Transcode"));
  o.insert(QStringLiteral("PositionTicks"), static_cast<double>(positionTicks));
  o.insert(QStringLiteral("CanSeek"), true);
  o.insert(QStringLiteral("IsPaused"), paused);
  o.insert(QStringLiteral("IsMuted"), false);
  if (audioIndex >= 0) o.insert(QStringLiteral("AudioStreamIndex"), audioIndex);
  if (subtitleIndex >= 0) o.insert(QStringLiteral("SubtitleStreamIndex"), subtitleIndex);
  sendPost(QStringLiteral("/Sessions/Playing/Progress"), o, true, cb);
}

void JellyfinClient::reportPlaybackStopped(const QString& itemId,
                                           const QString& playSessionId,
                                           qint64 positionTicks,
                                           const QString& mediaSourceId) {
  QJsonObject o;
  o.insert(QStringLiteral("ItemId"), itemId);
  o.insert(QStringLiteral("PlaySessionId"), playSessionId);
  o.insert(QStringLiteral("MediaSourceId"), mediaSourceId);
  o.insert(QStringLiteral("PositionTicks"), static_cast<double>(positionTicks));
  sendPost(QStringLiteral("/Sessions/Playing/Stopped"), o, true, nullptr);
}

QString JellyfinClient::userImageUrl() const {
  if (server_.isEmpty() || userId_.isEmpty()) return QString();
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("maxWidth"), QStringLiteral("160"));
  q.addQueryItem(QStringLiteral("quality"), QStringLiteral("90"));
  return QStringLiteral("%1/Users/%2/Images/Primary?%3")
      .arg(server_, userId_, q.toString(QUrl::FullyEncoded));
}

void JellyfinClient::fetchItemCount(const QString& includeTypes,
                                    const QString& filters, JsonCallback cb) {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("userId"), userId_);
  q.addQueryItem(QStringLiteral("recursive"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("includeItemTypes"), includeTypes);
  if (!filters.isEmpty()) q.addQueryItem(QStringLiteral("filters"), filters);
  q.addQueryItem(QStringLiteral("limit"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("enableTotalRecordCount"), QStringLiteral("true"));
  sendGet(QStringLiteral("/Users/%1/Items?").arg(userId_) +
              q.toString(QUrl::FullyEncoded),
          true, cb);
}

QString JellyfinClient::imageUrl(const QString& itemId, const QString& type,
                                 int maxWidth, const QString& tag) const {
  if (server_.isEmpty() || itemId.isEmpty()) return QString();
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("maxWidth"), QString::number(maxWidth));
  q.addQueryItem(QStringLiteral("quality"), QStringLiteral("88"));
  if (!tag.isEmpty()) q.addQueryItem(QStringLiteral("tag"), tag);
  return QStringLiteral("%1/Items/%2/Images/%3?%4")
      .arg(server_, itemId, type, q.toString(QUrl::FullyEncoded));
}

QString JellyfinClient::buildHlsUrl(const QString& itemId, const QString& playSessionId,
                                    const QString& mediaSourceId, int maxBitrate,
                                    qint64 startTicks, int audioIndex,
                                    int subtitleIndex) const {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("MediaSourceId"), mediaSourceId);
  q.addQueryItem(QStringLiteral("PlaySessionId"), playSessionId);
  q.addQueryItem(QStringLiteral("VideoCodec"), QStringLiteral("h264"));
  q.addQueryItem(QStringLiteral("AudioCodec"), QStringLiteral("aac"));
  q.addQueryItem(QStringLiteral("VideoBitrate"),
                 QString::number(static_cast<qint64>(maxBitrate) * 1000));
  q.addQueryItem(QStringLiteral("AudioBitrate"), QStringLiteral("192000"));
  // Uscita a risoluzione nativa fino a 1080p: serve a capire quali altezze
  // il decoder del Link gestisce davvero.
  q.addQueryItem(QStringLiteral("MaxWidth"), QStringLiteral("1920"));
  q.addQueryItem(QStringLiteral("MaxHeight"), QStringLiteral("1080"));
  q.addQueryItem(QStringLiteral("MaxAudioChannels"), QStringLiteral("2"));
  q.addQueryItem(QStringLiteral("TranscodingContainer"), QStringLiteral("ts"));
  q.addQueryItem(QStringLiteral("SegmentContainer"), QStringLiteral("ts"));
  // Obbliga la transcodifica: il decoder del Link vuole un flusso H.264 con
  // timestamp prevedibili; il copy del file originale faceva scattare.
  q.addQueryItem(QStringLiteral("EnableAutoStreamCopy"), QStringLiteral("false"));
  q.addQueryItem(QStringLiteral("AllowVideoStreamCopy"), QStringLiteral("false"));
  q.addQueryItem(QStringLiteral("AllowAudioStreamCopy"), QStringLiteral("false"));
  if (audioIndex >= 0) q.addQueryItem(QStringLiteral("AudioStreamIndex"), QString::number(audioIndex));
  if (subtitleIndex >= 0) {
    q.addQueryItem(QStringLiteral("SubtitleStreamIndex"), QString::number(subtitleIndex));
    q.addQueryItem(QStringLiteral("SubtitleMethod"), QStringLiteral("Encode"));
  }
  q.addQueryItem(QStringLiteral("DeviceId"), deviceId_);
  if (startTicks > 0) q.addQueryItem(QStringLiteral("StartTimeTicks"), QString::number(startTicks));
  return QStringLiteral("%1/Videos/%2/master.m3u8?%3")
      .arg(server_, itemId, q.toString(QUrl::FullyEncoded));
}
