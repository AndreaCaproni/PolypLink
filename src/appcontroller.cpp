#include "appcontroller.h"

#include <QDateTime>
#include <QSettings>
#include <QElapsedTimer>
#include <algorithm>
#include <memory>
#include <QUuid>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>
#include <QtMath>

namespace {

QString formatDuration(qint64 ticks) {
  if (ticks <= 0) return QString();
  const qint64 minutes = ticks / 600000000LL;  // 10.000.000 tick al secondo
  const qint64 h = minutes / 60;
  const qint64 m = minutes % 60;
  if (h > 0) return QStringLiteral("%1h %2m").arg(h).arg(m, 2, 10, QLatin1Char('0'));
  return QStringLiteral("%1m").arg(m);
}

}  // namespace

AppController::AppController(QObject* parent) : QObject(parent) {
  connect(&discovery_, &Discovery::serversChanged, this, &AppController::serversChanged);
  connect(&discovery_, &Discovery::error, this, [this](const QString& m) {
    setStatus(m);
    emit error(m);
  });

  // Token revocato o scaduto: non si resta bloccati su una home vuota.
  connect(&client_, &JellyfinClient::unauthorized, this, [this]() {
    if (state_ == QLatin1String("login")) return;
    qWarning("[api] sessione non piu' valida: torno al login");
    logout();
  });

  // L'OSD resta 5 s, poi sfuma: durante la pausa e con il pannello
  // impostazioni aperto resta fermo.
  QSettings settings;
  defaultQualityKbps_ = settings.value(QStringLiteral("defaultQualityKbps"), 12000).toInt();
  if (defaultQualityKbps_ != 4000 && defaultQualityKbps_ != 8000 &&
      defaultQualityKbps_ != 12000 && defaultQualityKbps_ != 20000) {
    defaultQualityKbps_ = 12000;
  }
  osdSeconds_ = settings.value(QStringLiteral("osdSeconds"), 5).toInt();
  if (osdSeconds_ != 3 && osdSeconds_ != 5 && osdSeconds_ != 8) osdSeconds_ = 5;

  osdTimer_.setSingleShot(true);
  osdTimer_.setInterval(osdSeconds_ * 1000);
  connect(&osdTimer_, &QTimer::timeout, this, [this]() {
    if (playerPaused_ || settingsOpen_ || !playerActive_) return;
    if (!osdVisible_) return;
    osdVisible_ = false;
    emit osdChanged();
  });

  // Il seek parte solo quando il cursore si ferma (o si conferma con A):
  // niente riavvio del flusso a ogni pressione.
  noticeTimer_.setSingleShot(true);
  noticeTimer_.setInterval(3000);
  connect(&noticeTimer_, &QTimer::timeout, this, [this]() {
    if (notice_.isEmpty()) return;
    notice_.clear();
    emit noticeChanged();
  });

  seekTimer_.setSingleShot(true);
  seekTimer_.setInterval(700);
  connect(&seekTimer_, &QTimer::timeout, this, &AppController::commitSeek);

  qcTimer_.setInterval(2000);
  connect(&qcTimer_, &QTimer::timeout, this, [this]() {
    if (qcSecret_.isEmpty()) return;
    if (++qcAttempts_ > 90) {
      qcTimer_.stop();
      qcStatus_ = QStringLiteral("Tempo scaduto. Riprova.");
      emit quickConnectChanged();
      return;
    }
    client_.quickConnectPoll(qcSecret_, [this](const QJsonDocument& doc, const QString& err) {
      if (!err.isEmpty()) return;  // il polling ignora gli errori transitori
      const QJsonObject o = doc.object();
      if (!o.value(QStringLiteral("Authenticated")).toBool()) return;
      qcTimer_.stop();
      qcStatus_ = QStringLiteral("Autorizzato, accesso in corso…");
      emit quickConnectChanged();
      client_.quickConnectAuthenticate(
          qcSecret_, [this](const QJsonDocument&, const QString& err2) {
            if (!err2.isEmpty()) {
              qcStatus_ = err2;
              emit quickConnectChanged();
              return;
            }
            qcSecret_.clear();
            qcCode_.clear();
            afterLogin();
          });
    });
  });

  if (client_.hasToken()) {
    state_ = QStringLiteral("home");
    afterLogin();
    return;
  }

  QTimer::singleShot(0, this, &AppController::applyConfigFile);
}

// Il firmware copia steamlink/config/* in /mnt/config/ a ogni avvio: quel
// percorso e' persistente e leggibile dall'app. Serve perche' la scoperta UDP
// non attraversa Docker (rete bridge) ne' reti con broadcast isolato.
//
// Formato del file (una direttiva per riga, # per i commenti):
//   http://192.168.1.10:8096
//   user=mario
//   pass=segreta
void AppController::applyConfigFile() {
  const QStringList candidates = {
      QStringLiteral("/mnt/config/system/jellyfin-server.txt"),
      QStringLiteral("/home/apps/jellyfin-link/server.txt"),
  };

  QString url;
  QString user;
  QString pass;
  QString used;

  for (const QString& path : candidates) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
      const QString line = raw.trimmed();
      if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
      if (line.startsWith(QLatin1String("user="))) {
        user = line.mid(5).trimmed();
      } else if (line.startsWith(QLatin1String("pass="))) {
        pass = line.mid(5).trimmed();
      } else if (url.isEmpty()) {
        url = line;
      }
    }
    if (!url.isEmpty()) {
      used = path;
      break;
    }
  }

  if (url.isEmpty()) {
    qInfo("[config] nessun file di configurazione, avvio la scoperta automatica");
    discover();
    return;
  }

  qInfo("[config] server da %s: %s (utente: %s)",
        qPrintable(used), qPrintable(url),
        user.isEmpty() ? "quick connect" : qPrintable(user));

  client_.setServer(url, true);
  client_.setServerName(QString());

  if (!user.isEmpty() && !pass.isEmpty()) {
    setState(QStringLiteral("quickconnect"));
    qcStatus_ = QStringLiteral("Accesso in corso…");
    emit quickConnectChanged();
    client_.authenticate(user, pass, [this](const QJsonDocument&, const QString& err) {
      if (!err.isEmpty()) {
        setStatus(err);
        qcStatus_ = QStringLiteral("Accesso fallito: %1").arg(err);
        emit quickConnectChanged();
        setState(QStringLiteral("login"));
        return;
      }
      afterLogin();
    });
    return;
  }

  setState(QStringLiteral("auth"));
}

QString AppController::serverLabel() const {
  if (!client_.serverName().isEmpty()) return client_.serverName();
  return client_.server();
}

QString AppController::userImage() const { return client_.userImageUrl(); }

void AppController::openPreviousEpisode() {
  const QString current = detail_.value(QStringLiteral("id")).toString();
  const QString seriesId = detail_.value(QStringLiteral("seriesId")).toString();
  if (current.isEmpty() || seriesId.isEmpty()) return;
  setLoading(true);
  client_.fetchEpisodes(
      seriesId, QString(),
      [this, current](const QJsonDocument& doc, const QString& err) {
        setLoading(false);
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
          showNotice(QStringLiteral("Nessun episodio precedente"));
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

QVariantList AppController::servers() const { return discovery_.servers(); }

void AppController::discover() {
  setStatus(QStringLiteral("Cerco il server Jellyfin sulla rete…"));
  discovery_.start();
}

void AppController::setManualServer(const QString& url) {
  QString u = url.trimmed();
  if (u.isEmpty()) return;
  if (!u.startsWith(QLatin1String("http://")) && !u.startsWith(QLatin1String("https://"))) {
    u.prepend(QLatin1String("http://"));
  }
  // Se manca la porta, il client ufficiale usa 8096.
  const int schemeEnd = u.indexOf(QLatin1String("://"));
  const QString hostPart = schemeEnd >= 0 ? u.mid(schemeEnd + 3) : u;
  if (!hostPart.contains(QLatin1Char(':')) && !hostPart.contains(QLatin1Char('/'))) {
    while (u.endsWith(QLatin1Char('/'))) u.chop(1);
    u += QLatin1String(":8096");
  }
  client_.setServer(u);
  setState(QStringLiteral("auth"));
}

void AppController::loginWithPassword(const QString& user, const QString& pass) {
  if (user.trimmed().isEmpty()) {
    setStatus(QStringLiteral("Inserisci il nome utente"));
    return;
  }
  setStatus(QStringLiteral("Accesso in corso…"));
  client_.authenticate(user.trimmed(), pass,
                       [this](const QJsonDocument&, const QString& err) {
                         if (!err.isEmpty()) {
                           setStatus(QStringLiteral("Accesso fallito: %1").arg(err));
                           return;
                         }
                         afterLogin();
                       });
}

void AppController::selectServer(int index) {
  const QVariantList list = discovery_.servers();
  if (index < 0 || index >= list.size()) return;
  const QVariantMap s = list.at(index).toMap();
  client_.setServer(s.value(QStringLiteral("address")).toString());
  client_.setServerName(s.value(QStringLiteral("name")).toString());
  setState(QStringLiteral("auth"));
}

void AppController::beginQuickConnect() {
  qcCode_.clear();
  qcStatus_ = QStringLiteral("Preparo il codice…");
  emit quickConnectChanged();
  setState(QStringLiteral("quickconnect"));

  client_.quickConnectInitiate([this](const QJsonDocument& doc, const QString& err) {
    if (!err.isEmpty()) {
      qcStatus_ = err;
      emit quickConnectChanged();
      return;
    }
    const QJsonObject o = doc.object();
    qcCode_ = o.value(QStringLiteral("Code")).toString();
    qcSecret_ = o.value(QStringLiteral("Secret")).toString();
    qcAttempts_ = 0;
    qcStatus_ = QStringLiteral(
        "Apri Jellyfin sul telefono o sul PC: Impostazioni → Quick Connect, "
        "e inserisci questo codice.");
    emit quickConnectChanged();
    if (!qcSecret_.isEmpty()) qcTimer_.start();
  });
}

void AppController::cancelQuickConnect() {
  qcTimer_.stop();
  qcSecret_.clear();
  qcCode_.clear();
  setState(QStringLiteral("login"));
}

void AppController::logout() {
  qcTimer_.stop();
  teardownPlayer();
  client_.logout();
  views_.clear();
  resume_.clear();
  nextUp_.clear();
  latest_.clear();
  libraryItems_.clear();
  detail_.clear();
  userStats_.clear();
  emit viewsChanged();
  emit homeChanged();
  emit libraryChanged();
  emit detailChanged();
  setLoading(false);
  setStatus(QString());
  // Dopo un logout si sceglie il server: quello salvato resta in cima,
  // così si rientra con un tasto o si passa a un altro.
  setState(QStringLiteral("login"));
  discover();
}

void AppController::refreshHome() { afterLogin(); }

void AppController::useSavedServer() {
  if (client_.server().isEmpty()) return;
  setState(QStringLiteral("auth"));
}

void AppController::afterLogin() {
  const quint64 gen = ++homeGen_;
  setState(QStringLiteral("home"));
  setStatus(QString());
  setLoading(true);

  client_.fetchViews([this, gen](const QJsonDocument& doc, const QString& err) {
    if (gen != homeGen_) return;
    if (!err.isEmpty()) {
      setStatus(err);
      setLoading(false);
      return;
    }
    views_.clear();
    const QJsonArray items = doc.object().value(QStringLiteral("Items")).toArray();
    for (const QJsonValue& v : items) {
      const QJsonObject o = v.toObject();
      const QString type = o.value(QStringLiteral("CollectionType")).toString();
      // Musica/playlist non hanno senso in questa prima versione.
      if (type == QLatin1String("music") || type == QLatin1String("playlists")) continue;
      QVariantMap m;
      m.insert(QStringLiteral("id"), o.value(QStringLiteral("Id")).toString());
      m.insert(QStringLiteral("name"), o.value(QStringLiteral("Name")).toString());
      m.insert(QStringLiteral("type"), type);
      const QString tag =
          o.value(QStringLiteral("ImageTags")).toObject().value(QStringLiteral("Primary")).toString();
      m.insert(QStringLiteral("image"),
               client_.imageUrl(o.value(QStringLiteral("Id")).toString(),
                                QStringLiteral("Primary"), 320, tag));
      views_.append(m);
    }
    emit viewsChanged();
    // "Aggiunti di recente": una chiamata per libreria, cosi' ogni card sa
    // da dove arriva (utile per i doppioni tra librerie) e l'ordine resta
    // cronologico.
    loadLatest(gen);
  });

  client_.fetchResume(20, [this, gen](const QJsonDocument& doc, const QString& err) {
    if (gen != homeGen_) return;
    if (err.isEmpty()) {
      resume_ = toList(doc.object().value(QStringLiteral("Items")).toArray());
      emit homeChanged();
    }
  });

  // "Prossimo episodio": il prossimo non visto delle serie gia' iniziate.
  client_.fetchNextUp(20, [this, gen](const QJsonDocument& doc, const QString& err) {
    if (gen != homeGen_) return;
    if (err.isEmpty()) {
      nextUp_ = toList(doc.object().value(QStringLiteral("Items")).toArray());
      emit homeChanged();
    }
  });

}

void AppController::loadLatest(quint64 gen) {
  auto acc = std::make_shared<QVariantList>();
  auto left = std::make_shared<int>(0);

  for (const QVariant& v : views_) {
    const QVariantMap vm = v.toMap();
    const QString vid = vm.value(QStringLiteral("id")).toString();
    const QString vname = vm.value(QStringLiteral("name")).toString();
    if (vid.isEmpty()) continue;
    ++(*left);
    client_.fetchLatestFor(
        vid, 8,
        [this, acc, left, vname, gen](const QJsonDocument& doc, const QString& err) {
          if (gen != homeGen_) return;
          if (err.isEmpty()) {
            for (const QVariant& item : toList(doc.array())) {
              const QVariantMap m = item.toMap();
              const QString type = m.value(QStringLiteral("type")).toString();
              if (type == QLatin1String("Episode") ||
                  type == QLatin1String("Season")) {
                continue;
              }
              QVariantMap tagged = m;
              tagged.insert(QStringLiteral("library"), vname);
              acc->append(tagged);
            }
          }
          if (--(*left) == 0) {
            if (gen != homeGen_) return;
            std::sort(acc->begin(), acc->end(),
                      [](const QVariant& a, const QVariant& b) {
                        return a.toMap().value(QStringLiteral("dateCreated")).toString() >
                               b.toMap().value(QStringLiteral("dateCreated")).toString();
                      });
            latest_ = *acc;
            setLoading(false);
            emit homeChanged();
          }
        });
  }

  if (*left == 0) {
    latest_.clear();
    setLoading(false);
    emit homeChanged();
  }
}

QVariantMap AppController::toMap(const QJsonObject& item) const {
  QVariantMap m;
  const QString id = item.value(QStringLiteral("Id")).toString();
  m.insert(QStringLiteral("id"), id);
  m.insert(QStringLiteral("name"), item.value(QStringLiteral("Name")).toString());
  m.insert(QStringLiteral("type"), item.value(QStringLiteral("Type")).toString());
  m.insert(QStringLiteral("seriesId"), item.value(QStringLiteral("SeriesId")).toString());
  m.insert(QStringLiteral("seasonId"), item.value(QStringLiteral("SeasonId")).toString());
  m.insert(QStringLiteral("indexNumber"), item.value(QStringLiteral("IndexNumber")).toInt());
  m.insert(QStringLiteral("parentIndexNumber"),
           item.value(QStringLiteral("ParentIndexNumber")).toInt());
  m.insert(QStringLiteral("overview"), item.value(QStringLiteral("Overview")).toString());
  {
    QVariantList genres;
    for (const QJsonValue& g : item.value(QStringLiteral("Genres")).toArray()) {
      genres.append(g.toString());
    }
    m.insert(QStringLiteral("genres"), genres);
  }
  m.insert(QStringLiteral("childCount"), item.value(QStringLiteral("ChildCount")).toInt());
  m.insert(QStringLiteral("rating"),
           item.value(QStringLiteral("CommunityRating")).toDouble());
  m.insert(QStringLiteral("communityRating"),
           item.value(QStringLiteral("CommunityRating")).toDouble());
  m.insert(QStringLiteral("officialRating"),
           item.value(QStringLiteral("OfficialRating")).toString());

  // Dettagli tecnici dalla prima sorgente: risoluzione e audio.
  QString resolution;
  QString audio;
  const QJsonArray sources = item.value(QStringLiteral("MediaSources")).toArray();
  if (!sources.isEmpty()) {
    const QJsonArray streams =
        sources.at(0).toObject().value(QStringLiteral("MediaStreams")).toArray();
    for (const QJsonValue& sv : streams) {
      const QJsonObject st = sv.toObject();
      const QString type = st.value(QStringLiteral("Type")).toString();
      if (type == QLatin1String("Video") && resolution.isEmpty()) {
        const int h = st.value(QStringLiteral("Height")).toInt();
        if (h >= 1900) resolution = QStringLiteral("4K");
        else if (h >= 1000) resolution = QStringLiteral("1080p");
        else if (h >= 700) resolution = QStringLiteral("720p");
        else if (h > 0) resolution = QStringLiteral("%1p").arg(h);
      } else if (type == QLatin1String("Audio") && audio.isEmpty()) {
        const QString codec = st.value(QStringLiteral("Codec")).toString().toUpper();
        const int channels = st.value(QStringLiteral("Channels")).toInt();
        QString layout;
        if (channels == 6) layout = QStringLiteral(" 5.1");
        else if (channels == 8) layout = QStringLiteral(" 7.1");
        else if (channels == 2) layout = QStringLiteral(" stereo");
        audio = codec + layout;
      }
    }
  }
  m.insert(QStringLiteral("resolution"), resolution);
  m.insert(QStringLiteral("audio"), audio);

  const int year = item.value(QStringLiteral("ProductionYear")).toInt();
  m.insert(QStringLiteral("year"), year);
  m.insert(QStringLiteral("dateCreated"),
           item.value(QStringLiteral("DateCreated")).toString());
  const qint64 runtime = static_cast<qint64>(item.value(QStringLiteral("RunTimeTicks")).toDouble());
  m.insert(QStringLiteral("runtimeTicks"), runtime);

  QString subtitle;
  if (year > 0) subtitle = QString::number(year);
  const QString duration = formatDuration(runtime);
  m.insert(QStringLiteral("duration"), duration);
  if (!duration.isEmpty()) subtitle += (subtitle.isEmpty() ? QString() : QStringLiteral(" · ")) + duration;

  // Per gli episodi il titolo utile e' "Serie · S1 E3"
  const QString series = item.value(QStringLiteral("SeriesName")).toString();
  if (!series.isEmpty()) {
    const int season = item.value(QStringLiteral("ParentIndexNumber")).toInt();
    const int episode = item.value(QStringLiteral("IndexNumber")).toInt();
    subtitle = series;
    if (season > 0 && episode > 0) {
      subtitle += QStringLiteral(" · S%1 E%2").arg(season).arg(episode);
    }
  }
  m.insert(QStringLiteral("subtitle"), subtitle);

  const QJsonObject tags = item.value(QStringLiteral("ImageTags")).toObject();
  m.insert(QStringLiteral("image"),
           client_.imageUrl(id, QStringLiteral("Primary"), 400,
                            tags.value(QStringLiteral("Primary")).toString()));
  const QJsonArray backdrops = item.value(QStringLiteral("BackdropImageTags")).toArray();
  m.insert(QStringLiteral("backdrop"),
           backdrops.isEmpty()
               ? QString()
               : client_.imageUrl(id, QStringLiteral("Backdrop"), 1280, backdrops.at(0).toString()));

  // --- card orizzontali (Continua a guardare / Prossimo episodio) ---------
  // Gli episodi hanno come Primary lo screenshot 16:9; i film hanno il Thumb.
  const double imgAspect = item.value(QStringLiteral("PrimaryImageAspectRatio")).toDouble();
  const QString thumbTag = tags.value(QStringLiteral("Thumb")).toString();
  const QString primaryTag = tags.value(QStringLiteral("Primary")).toString();
  QString landscape;
  if (!thumbTag.isEmpty()) {
    landscape = client_.imageUrl(id, QStringLiteral("Thumb"), 640, thumbTag);
  } else if (imgAspect >= 1.2 && !primaryTag.isEmpty()) {
    landscape = client_.imageUrl(id, QStringLiteral("Primary"), 640, primaryTag);
  } else if (!backdrops.isEmpty()) {
    landscape = client_.imageUrl(id, QStringLiteral("Backdrop"), 1280,
                                 backdrops.at(0).toString());
  } else {
    landscape = m.value(QStringLiteral("image")).toString();
  }
  m.insert(QStringLiteral("imageLandscape"), landscape);

  QString cardTitle = item.value(QStringLiteral("Name")).toString();
  QString cardSubtitle = year > 0 ? QString::number(year) : subtitle;
  if (item.value(QStringLiteral("Type")).toString() == QLatin1String("Episode")) {
    if (!series.isEmpty()) cardTitle = series;
    const int season = item.value(QStringLiteral("ParentIndexNumber")).toInt();
    const int episode = item.value(QStringLiteral("IndexNumber")).toInt();
    if (season > 0 && episode > 0) {
      cardSubtitle = QStringLiteral("S%1 E%2").arg(season).arg(episode);
      const QString epName = item.value(QStringLiteral("Name")).toString();
      if (!epName.isEmpty()) cardSubtitle += QStringLiteral(" · ") + epName;
    }
  }
  m.insert(QStringLiteral("cardTitle"), cardTitle);
  m.insert(QStringLiteral("cardSubtitle"), cardSubtitle);

  const QJsonObject userData = item.value(QStringLiteral("UserData")).toObject();
  m.insert(QStringLiteral("played"), userData.value(QStringLiteral("Played")).toBool());
  m.insert(QStringLiteral("favorite"),
           userData.value(QStringLiteral("IsFavorite")).toBool());
  m.insert(QStringLiteral("progress"),
           userData.value(QStringLiteral("PlayedPercentage")).toDouble());
  m.insert(QStringLiteral("resumeTicks"),
           static_cast<qint64>(userData.value(QStringLiteral("PlaybackPositionTicks")).toDouble()));
  return m;
}

QVariantList AppController::toList(const QJsonArray& items) const {
  QVariantList out;
  out.reserve(items.size());
  for (const QJsonValue& v : items) out.append(toMap(v.toObject()));
  return out;
}

void AppController::openView(int index) {
  if (index < 0 || index >= views_.size()) return;
  const QVariantMap view = views_.at(index).toMap();
  libraryParentId_ = view.value(QStringLiteral("id")).toString();
  libraryTitle_ = view.value(QStringLiteral("name")).toString();
  libraryIndex_ = index;

  const QString collection = view.value(QStringLiteral("type")).toString();
  QString include = QStringLiteral("Movie,Series");
  if (collection == QLatin1String("movies")) include = QStringLiteral("Movie");
  else if (collection == QLatin1String("tvshows")) include = QStringLiteral("Series");
  else if (collection == QLatin1String("boxsets")) include = QStringLiteral("BoxSet");

  const quint64 gen = ++libraryGen_;
  libraryItems_.clear();
  emit libraryChanged();
  setLoading(true);
  setStatus(QString());

  client_.fetchLibrary(libraryParentId_, include, QStringLiteral("SortName"), 300, 0,
                       [this, gen](const QJsonDocument& doc, const QString& err) {
                         if (gen != libraryGen_) return;
                         setLoading(false);
                         if (!err.isEmpty()) {
                           setStatus(err);
                           return;
                         }
                         libraryItems_ =
                             toList(doc.object().value(QStringLiteral("Items")).toArray());
                         emit libraryChanged();
                       });
}

void AppController::openFavorites() {
  libraryParentId_.clear();
  libraryTitle_ = QStringLiteral("Preferiti");
  const quint64 gen = ++libraryGen_;
  libraryItems_.clear();
  emit libraryChanged();
  setLoading(true);
  setStatus(QString());
  client_.fetchFavorites(200, [this, gen](const QJsonDocument& doc, const QString& err) {
    if (gen != libraryGen_) return;
    setLoading(false);
    if (!err.isEmpty()) {
      setStatus(err);
      return;
    }
    libraryItems_ = toList(doc.object().value(QStringLiteral("Items")).toArray());
    emit libraryChanged();
  });
}

void AppController::openItem(const QString& itemId) {
  const quint64 gen = ++itemGen_;
  // Se e' un altro titolo, non mostrare per 10 s la scheda precedente.
  if (detail_.value(QStringLiteral("id")).toString() != itemId) {
    detail_.clear();
    detail_.insert(QStringLiteral("id"), itemId);
    emit detailChanged();
  }
  setLoading(true);
  client_.fetchItem(itemId, [this, gen](const QJsonDocument& doc, const QString& err) {
    if (gen != itemGen_) return;
    setLoading(false);
    if (!err.isEmpty()) {
      setStatus(err);
      detail_.insert(QStringLiteral("name"), QStringLiteral("Impossibile caricare"));
      detail_.insert(QStringLiteral("overview"), err);
      emit detailChanged();
      return;
    }
    const QJsonObject o = doc.object();
    detail_ = toMap(o);
    detail_.insert(QStringLiteral("communityRating"),
                   o.value(QStringLiteral("CommunityRating")).toDouble());
    detail_.insert(QStringLiteral("officialRating"),
                   o.value(QStringLiteral("OfficialRating")).toString());
    detail_.insert(QStringLiteral("tagline"), o.value(QStringLiteral("Tagline")).toString());
    emit detailChanged();

    // tracce audio e sottotitoli del file
    audioTracks_.clear();
    subtitleTracks_.clear();
    audioIndex_ = -1;
    subtitleIndex_ = -1;
    // parte dalla qualità predefinita scelta nelle impostazioni
    qualityKbps_ = defaultQualityKbps_;
    const QJsonArray sources = o.value(QStringLiteral("MediaSources")).toArray();
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
          const int ch = st.value(QStringLiteral("Channels")).toInt();
          QString label = lang.isEmpty() ? QStringLiteral("Audio %1").arg(index) : lang;
          if (!codec.isEmpty()) label += QStringLiteral(" · ") + codec;
          if (ch == 6) label += QStringLiteral(" 5.1");
          else if (ch == 8) label += QStringLiteral(" 7.1");
          QVariantMap tr;
          tr.insert(QStringLiteral("index"), index);
          tr.insert(QStringLiteral("label"), label);
          audioTracks_.append(tr);
          if (audioIndex_ < 0 || st.value(QStringLiteral("IsDefault")).toBool()) audioIndex_ = index;
        } else if (type == QLatin1String("Subtitle")) {
          QString label = lang.isEmpty() ? QStringLiteral("Sottotitoli %1").arg(index) : lang;
          const QString title = st.value(QStringLiteral("DisplayTitle")).toString();
          if (!title.isEmpty()) label += QStringLiteral(" · ") + title;
          QVariantMap tr;
          tr.insert(QStringLiteral("index"), index);
          tr.insert(QStringLiteral("label"), label);
          subtitleTracks_.append(tr);
        }
      }
    }
    emit tracksChanged();

    const QString type = o.value(QStringLiteral("Type")).toString();
    if (type == QLatin1String("Series")) {
      seriesId_ = o.value(QStringLiteral("Id")).toString();
      seriesTitle_ = o.value(QStringLiteral("Name")).toString();
      seriesPoster_ = detail_.value(QStringLiteral("image")).toString();
      seriesOverview_ = o.value(QStringLiteral("Overview")).toString();
      seasons_.clear();
      emit seasonsChanged();
      const quint64 sgen = ++seasonsGen_;
      client_.fetchSeasons(seriesId_, [this, sgen](const QJsonDocument& seasonsDoc,
                                                  const QString& e) {
        if (sgen != seasonsGen_) return;
        if (!e.isEmpty()) {
          setStatus(e);
          return;
        }
        seasons_ = toList(seasonsDoc.object().value(QStringLiteral("Items")).toArray());
        for (QVariant& v : seasons_) {
          QVariantMap m = v.toMap();
          m.insert(QStringLiteral("name"), m.value(QStringLiteral("name")).toString());
          v = m;
        }
        emit seasonsChanged();
      });
    } else if (type == QLatin1String("Episode")) {
      const QString sid = o.value(QStringLiteral("SeriesId")).toString();
      if (!sid.isEmpty()) seriesId_ = sid;
    }
  });
}

void AppController::openSeason(int index) {
  if (index < 0 || index >= seasons_.size()) return;
  const QVariantMap season = seasons_.at(index).toMap();
  currentSeasonId_ = season.value(QStringLiteral("id")).toString();
  seasonTitle_ = season.value(QStringLiteral("name")).toString();
  seasonInfo_ = season;
  seasonInfo_.insert(QStringLiteral("series"), seriesTitle_);
  seasonInfo_.insert(QStringLiteral("seriesId"), seriesId_);
  if (seasonInfo_.value(QStringLiteral("overview")).toString().isEmpty()) {
    seasonInfo_.insert(QStringLiteral("overview"), seriesOverview_);
  }
  episodes_.clear();
  emit episodesChanged();
  setLoading(true);
  const quint64 gen = ++episodesGen_;
  client_.fetchEpisodes(seriesId_, currentSeasonId_,
                        [this, gen](const QJsonDocument& doc, const QString& err) {
                          if (gen != episodesGen_) return;
                          setLoading(false);
                          if (!err.isEmpty()) {
                            setStatus(err);
                            return;
                          }
                          episodes_ =
                              toList(doc.object().value(QStringLiteral("Items")).toArray());
                          emit episodesChanged();
                        });
}

void AppController::reopenSeries() {
  if (seriesId_.isEmpty()) return;
  openItem(seriesId_);
}

void AppController::patchLists(const QString& itemId, const QString& key,
                               const QVariant& value) {
  auto patch = [&](QVariantList& list) {
    for (int i = 0; i < list.size(); ++i) {
      QVariantMap m = list.at(i).toMap();
      if (m.value(QStringLiteral("id")).toString() == itemId) {
        m.insert(key, value);
        list[i] = m;
      }
    }
  };
  patch(resume_);
  patch(nextUp_);
  patch(latest_);
  patch(libraryItems_);
  patch(episodes_);
  emit homeChanged();
  emit libraryChanged();
  emit episodesChanged();
}

void AppController::togglePlayed() {
  const QString id = detail_.value(QStringLiteral("id")).toString();
  if (id.isEmpty()) return;
  const bool played = detail_.value(QStringLiteral("played")).toBool();
  client_.setPlayed(id, !played, [this, id, played](const QJsonDocument&, const QString& err) {
    if (!err.isEmpty()) {
      setStatus(err);
      return;
    }
    // Come il server: segnare visto/non visto azzera anche la posizione di
    // ripresa, altrimenti resterebbero "Riprendi" e "Riproduci dall'inizio".
    detail_.insert(QStringLiteral("played"), !played);
    detail_.insert(QStringLiteral("progress"), 0.0);
    detail_.insert(QStringLiteral("resumeTicks"), qint64(0));
    emit detailChanged();
    patchLists(id, QStringLiteral("played"), !played);
    patchLists(id, QStringLiteral("progress"), 0.0);
    patchLists(id, QStringLiteral("resumeTicks"), qint64(0));
    // "Continua a guardare" e "Prossimo episodio" vanno ricalcolati dal
    // server: una serie puo' far avanzare l'episodio successivo.
    QTimer::singleShot(400, this, &AppController::refreshHome);
    showNotice(!played ? QStringLiteral("Segnato come visto")
                       : QStringLiteral("Segnato come non visto"));
  });
}

void AppController::toggleFavorite() {
  const QString id = detail_.value(QStringLiteral("id")).toString();
  if (id.isEmpty()) return;
  const bool favorite = detail_.value(QStringLiteral("favorite")).toBool();
  client_.setFavorite(id, !favorite,
                      [this, id, favorite](const QJsonDocument&, const QString& err) {
                        if (!err.isEmpty()) {
                          setStatus(err);
                          return;
                        }
                        detail_.insert(QStringLiteral("favorite"), !favorite);
                        emit detailChanged();
                        patchLists(id, QStringLiteral("favorite"), !favorite);
                        showNotice(!favorite ? QStringLiteral("Aggiunto ai preferiti")
                                             : QStringLiteral("Rimosso dai preferiti"));
                      });
}

void AppController::playItem(const QString& itemId) { startPlayback(itemId); }

void AppController::playSeries(const QString& seriesId) {
  if (seriesId.isEmpty()) return;
  setLoading(true);
  client_.fetchNextUpForSeries(
      seriesId, [this, seriesId](const QJsonDocument& doc, const QString& err) {
        setLoading(false);
        const QJsonArray items = doc.object().value(QStringLiteral("Items")).toArray();
        if (err.isEmpty() && !items.isEmpty()) {
          playEpisode(items.at(0).toObject());
          return;
        }
        // Serie mai iniziata o gia' finita: primo episodio non visto.
        client_.fetchEpisodes(
            seriesId, QString(),
            [this](const QJsonDocument& d2, const QString& e2) {
              if (!e2.isEmpty()) {
                setStatus(e2);
                return;
              }
              const QJsonArray eps = d2.object().value(QStringLiteral("Items")).toArray();
              if (eps.isEmpty()) {
                setStatus(QStringLiteral("La serie non ha episodi"));
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

void AppController::toggleSeasonPlayed(const QString& seasonId) {
  if (seasonId.isEmpty()) return;
  bool played = false;
  for (const QVariant& v : seasons_) {
    const QVariantMap m = v.toMap();
    if (m.value(QStringLiteral("id")).toString() == seasonId) {
      played = m.value(QStringLiteral("played")).toBool();
      break;
    }
  }
  client_.setPlayed(seasonId, !played,
                    [this, seasonId, played](const QJsonDocument&, const QString& err) {
                      if (!err.isEmpty()) {
                        setStatus(err);
                        return;
                      }
                      for (int i = 0; i < seasons_.size(); ++i) {
                        QVariantMap m = seasons_.at(i).toMap();
                        if (m.value(QStringLiteral("id")).toString() == seasonId) {
                          m.insert(QStringLiteral("played"), !played);
                          m.insert(QStringLiteral("progress"), 0.0);
                          seasons_[i] = m;
                          break;
                        }
                      }
                      if (seasonInfo_.value(QStringLiteral("id")).toString() == seasonId) {
                        seasonInfo_.insert(QStringLiteral("played"), !played);
                        seasonInfo_.insert(QStringLiteral("progress"), 0.0);
                      }
                      emit seasonsChanged();
                      emit episodesChanged();
                      showNotice(!played ? QStringLiteral("Stagione segnata come vista")
                                         : QStringLiteral("Stagione segnata come non vista"));
                      QTimer::singleShot(400, this, &AppController::refreshHome);
                    });
}

void AppController::toggleSeasonFavorite(const QString& seasonId) {
  if (seasonId.isEmpty()) return;
  bool favorite = seasonInfo_.value(QStringLiteral("favorite")).toBool();
  client_.setFavorite(seasonId, !favorite,
                      [this, seasonId, favorite](const QJsonDocument&, const QString& err) {
                        if (!err.isEmpty()) {
                          setStatus(err);
                          return;
                        }
                        if (seasonInfo_.value(QStringLiteral("id")).toString() == seasonId) {
                          seasonInfo_.insert(QStringLiteral("favorite"), !favorite);
                        }
                        for (int i = 0; i < seasons_.size(); ++i) {
                          QVariantMap m = seasons_.at(i).toMap();
                          if (m.value(QStringLiteral("id")).toString() == seasonId) {
                            m.insert(QStringLiteral("favorite"), !favorite);
                            seasons_[i] = m;
                            break;
                          }
                        }
                        emit seasonsChanged();
                        emit episodesChanged();
                        showNotice(!favorite ? QStringLiteral("Stagione aggiunta ai preferiti")
                                             : QStringLiteral("Stagione rimossa dai preferiti"));
                      });
}

void AppController::playSeasonFromStart() {
  if (episodes_.isEmpty()) {
    setStatus(QStringLiteral("Gli episodi non sono ancora caricati"));
    return;
  }
  const QVariantMap ep = episodes_.first().toMap();
  const QString id = ep.value(QStringLiteral("id")).toString();
  if (id.isEmpty()) return;
  QString title = ep.value(QStringLiteral("name")).toString();
  if (!seriesTitle_.isEmpty()) title = seriesTitle_ + QStringLiteral(" · ") + title;
  const double dur = ep.value(QStringLiteral("runtimeTicks")).toDouble() / 1e7;
  qualityKbps_ = defaultQualityKbps_;
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
  qualityKbps_ = defaultQualityKbps_;
  qInfo("[player] serie: riproduco %s", qPrintable(title));
  startPlaybackAt(id, ticks, title, dur);
}

void AppController::playFromStart() {
  const QString id = detail_.value(QStringLiteral("id")).toString();
  if (id.isEmpty()) return;
  startPlaybackAt(id, 0);
}

void AppController::startPlayback(const QString& itemId) {
  const qint64 resume = detail_.value(QStringLiteral("resumeTicks")).toLongLong();
  startPlaybackAt(itemId, resume);
}

void AppController::startPlaybackAt(const QString& itemId, qint64 startTicks,
                                    const QString& title, double durationSec) {
  const quint64 gen = ++playbackGen_;
  mediaSourceId_.clear();
  qInfo("[diag] startPlaybackAt gen=%llu ticks=%lld player=%s",
        static_cast<unsigned long long>(gen), static_cast<long long>(startTicks),
        player_ ? "esistente" : "nullo");
  if (player_ != nullptr) {
    QElapsedTimer ot;
    ot.start();
    // Scollega i segnali PRIMA di fermarlo: altrimenti il suo "finished"
    // viene letto come fine riproduzione e chiude il player che stiamo
    // riaprendo (cambio impostazioni / seek).
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
  playerTitle_ = title.isEmpty() ? detail_.value(QStringLiteral("name")).toString()
                                 : title;
  playerDuration_ = durationSec > 0.0
                        ? durationSec
                        : detail_.value(QStringLiteral("runtimeTicks")).toDouble() / 1e7;
  playerPosition_ = startTicks / 1e7;
  playerStatus_.clear();
  playerActive_ = true;
  playerPaused_ = false;
  emit playerChanged();
  emit playerPositionChanged();

  // PlaybackInfo crea la sessione di transcode con tracce e bitrate. La
  // posizione non passa da StartTimeTicks (il server la propaga ai segmenti,
  // che la rifiutano): il player parte dal segmento che copre il tempo.
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
              // L'URL del server porta le scelte *sue* (sottotitoli predefiniti
              // in burn-in, traccia audio, StartTimeTicks). Lo riscriviamo con
              // le nostre: senza questo il server brucia i sottotitoli anche
              // se l'utente li ha spenti.
              QUrl u(url);
              QUrlQuery q(u);
              const QStringList managed{
                  QStringLiteral("StartTimeTicks"),
                  QStringLiteral("SubtitleStreamIndex"),
                  QStringLiteral("SubtitleMethod"),
                  QStringLiteral("AudioStreamIndex"),
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
                // traccia scelta: unica via possibile sul Link, burn-in
                q.addQueryItem(QStringLiteral("SubtitleStreamIndex"),
                               QString::number(subtitleIndex_));
                q.addQueryItem(QStringLiteral("SubtitleMethod"),
                               QStringLiteral("Encode"));
              } else {
                // spenti: -1 esplicito, il server non ne sceglie una da solo
                q.addQueryItem(QStringLiteral("SubtitleStreamIndex"),
                               QStringLiteral("-1"));
              }
              // Il Link decodifica solo H.264/AAC: forziamo il re-encode per
              // avere timestamp e pacing prevedibili (il copy faceva scattare).
              q.addQueryItem(QStringLiteral("EnableAutoStreamCopy"), QStringLiteral("false"));
              q.addQueryItem(QStringLiteral("AllowVideoStreamCopy"), QStringLiteral("false"));
              q.addQueryItem(QStringLiteral("AllowAudioStreamCopy"), QStringLiteral("false"));
              if (!q.hasQueryItem(QStringLiteral("api_key")) &&
                  !q.hasQueryItem(QStringLiteral("ApiKey"))) {
                q.addQueryItem(QStringLiteral("api_key"), client_.token());
              }
              u.setQuery(q);
              url = u.toString(QUrl::FullyEncoded);
            }
            const QString psid = s0.value(QStringLiteral("PlaySessionId")).toString();
            if (!psid.isEmpty()) playSessionId_ = psid;
            // La dashboard Jellyfin mostra la sessione corretta solo se sa
            // quale media source e che e' un transcode.
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
          // Niente player "alla cieca": schermo nero per 8 s e poi chiusura.
          qWarning("[player] avvio annullato: %s", qPrintable(err));
          playerActive_ = false;
          playerPaused_ = false;
          osdVisible_ = false;
          setStatus(err.isEmpty() ? QStringLiteral("Impossibile avviare la riproduzione")
                                  : err);
          emit playerChanged();
          emit osdChanged();
          return;
        }
        // Il server accetta PlayMethod=Transcode solo se il PlaySessionId e'
        // quello del job di transcodifica, cioe' quello nell'URL del flusso.
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
        qInfo("[player] stream: %s", qPrintable(url.left(150)));

        player_ = new Player(this);
        connect(player_, &Player::statsChanged, this, [this](const QVariantMap& stats) {
          playerStats_ = stats;
          emit playerStatsChanged();
        });
        connect(player_, &Player::positionChanged, this, [this](double seconds) {
          playerPosition_ = seconds;
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
        connect(player_, &Player::stalled, this, [this](bool stalled) {
          if (stalled) {
            playerStatus_ = QStringLiteral("Connessione persa… riprovo");
          } else if (playerStatus_.startsWith(QStringLiteral("Connessione persa"))) {
            playerStatus_.clear();
          }
          emit playerChanged();
        });
        connect(player_, &Player::failed, this, [this](const QString& message) {
          playerStatus_ = QStringLiteral("Errore: %1").arg(message);
          playerActive_ = false;
          setStatus(playerStatus_);
          emit playerChanged();
        });
        connect(player_, &Player::finished, this, [this]() {
          if (!playerActive_) return;
          client_.reportPlaybackStopped(playingItemId_, playSessionId_,
                                        static_cast<qint64>(playerPosition_ * 1e7),
                                        mediaSourceId_);
          playerActive_ = false;
          emit playerChanged();
          // la home deve riflettere subito "Continua a guardare" e il prossimo episodio
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

        if (mediaSourceId_.isEmpty()) mediaSourceId_ = itemId;
        client_.reportPlaybackStart(itemId, playSessionId_, startTicks,
                                    mediaSourceId_, audioIndex_, subtitleIndex_);
        player_->begin(cfg);
      });
}

void AppController::teardownPlayer() {
  ++playbackGen_;
  if (player_ == nullptr) {
    qInfo("[diag] teardownPlayer: nessun player");
    return;
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
  osdVisible_ = false;
  seekPending_ = false;
  seekTimer_.stop();
  statsOpen_ = false;
  progressInFlight_ = false;
  emit playerChanged();
  emit osdChanged();
  emit playerUiChanged();
}

void AppController::stopPlayback() {
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
  // -1 = spenti, poi le tracce
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
  static const int qualities[] = {4000, 8000, 12000, 20000};
  int pos = 2;
  for (int i = 0; i < 4; ++i) if (qualities[i] == qualityKbps_) pos = i;
  pos = (pos + delta + 4) % 4;
  qualityKbps_ = qualities[pos];
  emit tracksChanged();
}

void AppController::showOsd() {
  if (!playerActive_ || settingsOpen_) return;
  if (!seekPending_) seekPreview_ = playerPosition_;
  if (!osdVisible_) {
    osdVisible_ = true;
    emit osdChanged();
  }
  osdTimer_.start();
  emit playerUiChanged();
}

void AppController::toggleSettings() {
  settingsOpen_ = !settingsOpen_;
  settingsRow_ = 0;
  if (settingsOpen_) {
    seekPending_ = false;
    seekTimer_.stop();
    seekPreview_ = playerPosition_;
    osdTimer_.stop();
    if (osdVisible_) {
      osdVisible_ = false;
      emit osdChanged();
    }
  } else {
    showOsd();
  }
  emit settingsChanged();
}

void AppController::closeSettings() {
  if (!settingsOpen_) return;
  settingsOpen_ = false;
  emit settingsChanged();
  showOsd();
}

void AppController::moveSettings(int delta) {
  if (!settingsOpen_) return;
  settingsRow_ = std::max(0, std::min(2, settingsRow_ + delta));
  emit settingsChanged();
}

void AppController::changeSetting(int delta) {
  if (!settingsOpen_) return;
  if (settingsRow_ == 0) cycleQuality(delta);
  else if (settingsRow_ == 1) cycleAudio(delta);
  else cycleSubtitle(delta);
  // applica subito: riavvia il flusso alla posizione attuale
  restartPlaybackAtPosition();
}

void AppController::restartPlaybackAtPosition() {
  if (player_ == nullptr) return;
  startPlaybackAt(playingItemId_, static_cast<qint64>(playerPosition_ * 1e7));
}

void AppController::togglePause() {
  if (player_ == nullptr) return;

  if (!playerPaused_) {
    // pausa: fermo l'invio di audio e video, il frame resta congelato
    playerPaused_ = true;
    player_->setPaused(true);
    seekPending_ = false;
    seekTimer_.stop();
    seekPreview_ = playerPosition_;
    showOsd();
    emit playerChanged();
    return;
  }

  // ripresa: scongelo il decoder, il flusso e' rimasto aperto.
  playerPaused_ = false;
  player_->setPaused(false);
  seekPending_ = false;
  seekTimer_.stop();
  seekPreview_ = playerPosition_;
  showOsd();
  emit playerChanged();
}

void AppController::toggleStats() {
  if (!playerActive_) return;
  statsOpen_ = !statsOpen_;
  emit playerUiChanged();
}

void AppController::playerNavigate(int dx, int dy) {
  if (!playerActive_ || settingsOpen_) return;

  const bool wasHidden = !osdVisible_;
  if (wasHidden) {
    osdVisible_ = true;
    playerFocus_ = 0;
    seekPreview_ = playerPosition_;
    seekPending_ = false;
    emit osdChanged();
  }

  // La barra sta SOPRA i pulsanti: giu' porta ai pulsanti, su dalla barra
  // nasconde subito l'OSD (solo video, senza aspettare i 5 s).
  if (!wasHidden) {
    if (dy < 0) {
      if (playerFocus_ == 0) {
        osdVisible_ = false;
        osdTimer_.stop();
        emit osdChanged();
        emit playerUiChanged();
        return;
      }
      playerFocus_ = 0;
    } else if (dy > 0 && playerFocus_ == 0) {
      playerFocus_ = 2;   // play/pausa, al centro della riga
    }
  }

  if (dx != 0) {
    if (playerFocus_ == 0) {
      // Accelerazione: scatti consecutivi nella stessa direzione allargano
      // il passo. Con lo stick, che ripete, si attraversano anche i film
      // lunghi senza premere decine di volte.
      const qint64 now = QDateTime::currentMSecsSinceEpoch();
      if (dx != seekDirection_ || now - lastSeekMoveMs_ > 1500) {
        seekSteps_ = 0;
        seekDirection_ = dx;
      } else {
        ++seekSteps_;
      }
      lastSeekMoveMs_ = now;

      double step;
      // tetto a 5 minuti: oltre si salta troppo e si perde il punto
      if (seekSteps_ < 3) step = dx > 0 ? 30.0 : -10.0;
      else if (seekSteps_ < 6) step = dx > 0 ? 60.0 : -60.0;
      else step = dx > 0 ? 300.0 : -300.0;

      const double maxPos = playerDuration_ > 1.0 ? playerDuration_ - 1.0 : 0.0;
      seekPreview_ = qBound(0.0, seekPreview_ + step, maxPos);
      if (qAbs(seekPreview_ - playerPosition_) < 0.4) {
        seekPending_ = false;
        seekTimer_.stop();
      } else {
        seekPending_ = true;
        seekTimer_.start();
      }
    } else {
      playerFocus_ = qBound(1, playerFocus_ + dx, 7);
    }
  }

  osdTimer_.start();
  emit playerUiChanged();
}

void AppController::playerConfirm() {
  if (!playerActive_ || settingsOpen_) return;
  if (!osdVisible_) playerNavigate(0, 0);

  const double maxPos = playerDuration_ > 1.0 ? playerDuration_ - 1.0 : 0.0;
  switch (playerFocus_) {
    case 0:
      if (seekPending_) commitSeek();
      else togglePause();
      break;
    case 1:  // -10 s: sposta il cursore, il seek parte a fine corsa
      seekPreview_ = qBound(0.0, playerPosition_ - 10.0, maxPos);
      seekPending_ = true;
      seekTimer_.start();
      break;
    case 2:  // play / pausa
      togglePause();
      break;
    case 3:  // +30 s
      seekPreview_ = qBound(0.0, playerPosition_ + 30.0, maxPos);
      seekPending_ = true;
      seekTimer_.start();
      break;
    case 4:  // audio
    case 5:  // sottotitoli
    case 6: {  // qualita'
      seekPending_ = false;
      seekTimer_.stop();
      seekPreview_ = playerPosition_;
      settingsOpen_ = true;
      settingsRow_ = (playerFocus_ == 4) ? 1 : (playerFocus_ == 5 ? 2 : 0);
      osdTimer_.stop();
      if (osdVisible_) {
        osdVisible_ = false;
        emit osdChanged();
      }
      emit settingsChanged();
      break;
    }
    case 7:
      toggleStats();
      break;
    default:
      break;
  }
  emit playerUiChanged();
}

void AppController::commitSeek() {
  seekTimer_.stop();
  seekSteps_ = 0;
  seekDirection_ = 0;
  if (!seekPending_) return;
  seekPending_ = false;
  const qint64 ticks = static_cast<qint64>(seekPreview_ * 1e7);
  qInfo("[player] seek confermato a %.0f s", seekPreview_);
  startPlaybackAt(playingItemId_, ticks);
}

void AppController::playerBack() {
  if (!playerActive_) return;
  qInfo("[diag] playerBack: osdVisible=%d focus=%d", osdVisible_ ? 1 : 0,
        playerFocus_);
  if (osdVisible_) {
    seekPending_ = false;
    seekTimer_.stop();
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

void AppController::saveDefaults() {
  QSettings s;
  s.setValue(QStringLiteral("defaultQualityKbps"), defaultQualityKbps_);
  s.setValue(QStringLiteral("osdSeconds"), osdSeconds_);
}

void AppController::cycleDefaultQuality(int delta) {
  static const int values[] = {4000, 8000, 12000, 20000};
  int pos = 2;
  for (int i = 0; i < 4; ++i) if (values[i] == defaultQualityKbps_) pos = i;
  pos = (pos + delta + 4) % 4;
  defaultQualityKbps_ = values[pos];
  saveDefaults();
  emit tracksChanged();
  showNotice(QStringLiteral("Qualità predefinita: %1 Mbps").arg(defaultQualityKbps_ / 1000));
}

void AppController::cycleOsdSeconds(int delta) {
  static const int values[] = {3, 5, 8};
  int pos = 1;
  for (int i = 0; i < 3; ++i) if (values[i] == osdSeconds_) pos = i;
  pos = (pos + delta + 3) % 3;
  osdSeconds_ = values[pos];
  osdTimer_.setInterval(osdSeconds_ * 1000);
  saveDefaults();
  emit tracksChanged();
  showNotice(QStringLiteral("OSD: %1 secondi").arg(osdSeconds_));
}

void AppController::resetSettings() {
  defaultQualityKbps_ = 12000;
  osdSeconds_ = 5;
  osdTimer_.setInterval(5000);
  saveDefaults();
  emit tracksChanged();
  showNotice(QStringLiteral("Impostazioni ripristinate"));
}

void AppController::testConnection() {
  if (client_.server().isEmpty()) {
    showNotice(QStringLiteral("Nessun server configurato"));
    return;
  }
  showNotice(QStringLiteral("Verifico la connessione…"));
  client_.fetchViews([this](const QJsonDocument& doc, const QString& err) {
    if (!err.isEmpty()) {
      setStatus(err);
      return;
    }
    const int n = doc.object().value(QStringLiteral("Items")).toArray().size();
    showNotice(QStringLiteral("Connessione OK · %1 librerie").arg(n));
  });
}

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

void AppController::setLoading(bool v) {
  if (loading_ == v) return;
  loading_ = v;
  emit loadingChanged();
}
