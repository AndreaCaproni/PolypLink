#include "librarycontroller.h"

#include "jellyfinclient.h"

#include "translation.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <memory>

namespace {

constexpr int kHomeRefreshDelayMs = 400;

QString formatDuration(qint64 ticks) {
  if (ticks <= 0) return QString();
  const qint64 minutes = ticks / 600000000LL;  // 10,000,000 ticks per second
  const qint64 h = minutes / 60;
  const qint64 m = minutes % 60;
  if (h > 0) {
    return QStringLiteral("%1h %2m").arg(h).arg(m, 2, 10, QLatin1Char('0'));
  }
  return QStringLiteral("%1m").arg(m);
}

}  // namespace

LibraryController::LibraryController(JellyfinClient* client, QObject* parent)
    : QObject(parent), client_(client) {}

void LibraryController::setLoading(bool value) {
  if (loading_ == value) return;
  loading_ = value;
  emit loadingChanged();
}

void LibraryController::clear() {
  // Invalidate callbacks still in flight before clearing the lists.
  ++homeGen_;
  ++libraryGen_;
  ++itemGen_;
  ++seasonsGen_;
  ++episodesGen_;
  views_.clear();
  resume_.clear();
  nextUp_.clear();
  latest_.clear();
  libraryItems_.clear();
  detail_.clear();
  seasons_.clear();
  episodes_.clear();
  seriesId_.clear();
  currentSeasonId_.clear();
  setLoading(false);
  emit viewsChanged();
  emit homeChanged();
  emit libraryChanged();
  emit detailChanged();
  emit seasonsChanged();
  emit episodesChanged();
}

void LibraryController::loadHome() {
  const quint64 gen = ++homeGen_;
  setLoading(true);

  client_->fetchViews([this, gen](const QJsonDocument& doc, const QString& error) {
    if (gen != homeGen_) return;
    if (!error.isEmpty()) {
      emit statusRequested(error);
      setLoading(false);
      return;
    }
    views_.clear();
    const QJsonArray items = doc.object().value(QStringLiteral("Items")).toArray();
    for (const QJsonValue& value : items) {
      const QJsonObject o = value.toObject();
      const QString type = o.value(QStringLiteral("CollectionType")).toString();
      // Music/playlists make no sense in this first version.
      if (type == QLatin1String("music") || type == QLatin1String("playlists")) {
        continue;
      }
      constexpr int kMaxLibraries = 32;
      if (views_.size() >= kMaxLibraries) break;
      QVariantMap item;
      item.insert(QStringLiteral("id"), o.value(QStringLiteral("Id")).toString());
      item.insert(QStringLiteral("name"), o.value(QStringLiteral("Name")).toString());
      item.insert(QStringLiteral("type"), type);
      const QString tag = o.value(QStringLiteral("ImageTags"))
                              .toObject()
                              .value(QStringLiteral("Primary"))
                              .toString();
      item.insert(QStringLiteral("image"),
                  client_->imageUrl(o.value(QStringLiteral("Id")).toString(),
                                    QStringLiteral("Primary"), 320, tag));
      views_.append(item);
    }
    emit viewsChanged();
    // "Recently added": one call per library, so each card knows where it comes
    // from (useful for duplicates across libraries) and the order stays
    // chronological.
    loadLatest(gen);
  });

  client_->fetchResume(20, [this, gen](const QJsonDocument& doc, const QString& error) {
    if (gen != homeGen_) return;
    if (error.isEmpty()) {
      resume_ = toList(doc.object().value(QStringLiteral("Items")).toArray());
      emit homeChanged();
    }
  });

  // "Next up": the next unwatched episode of already started series.
  client_->fetchNextUp(20, [this, gen](const QJsonDocument& doc, const QString& error) {
    if (gen != homeGen_) return;
    if (error.isEmpty()) {
      nextUp_ = toList(doc.object().value(QStringLiteral("Items")).toArray());
      emit homeChanged();
    }
  });
}

void LibraryController::loadLatest(quint64 gen) {
  auto accumulated = std::make_shared<QVariantList>();
  auto remaining = std::make_shared<int>(0);

  for (const QVariant& value : views_) {
    const QVariantMap view = value.toMap();
    const QString viewId = view.value(QStringLiteral("id")).toString();
    const QString viewName = view.value(QStringLiteral("name")).toString();
    if (viewId.isEmpty()) continue;
    ++(*remaining);
    client_->fetchLatestFor(
        viewId, 8,
        [this, accumulated, remaining, viewName, gen](const QJsonDocument& doc,
                                                       const QString& error) {
          if (gen != homeGen_) return;
          if (error.isEmpty()) {
            for (const QVariant& item : toList(doc.array())) {
              const QVariantMap map = item.toMap();
              const QString type = map.value(QStringLiteral("type")).toString();
              if (type == QLatin1String("Episode") ||
                  type == QLatin1String("Season")) {
                continue;
              }
              QVariantMap tagged = map;
              tagged.insert(QStringLiteral("library"), viewName);
              accumulated->append(tagged);
            }
          }
          if (--(*remaining) == 0) {
            if (gen != homeGen_) return;
            std::sort(accumulated->begin(), accumulated->end(),
                      [](const QVariant& a, const QVariant& b) {
                        return a.toMap().value(QStringLiteral("dateCreated")).toString() >
                               b.toMap().value(QStringLiteral("dateCreated")).toString();
                      });
            latest_ = *accumulated;
            setLoading(false);
            emit homeChanged();
          }
        });
  }

  if (*remaining == 0) {
    latest_.clear();
    setLoading(false);
    emit homeChanged();
  }
}

void LibraryController::openView(int index) {
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
  emit statusRequested(QString());

  client_->fetchLibrary(
      libraryParentId_, include, QStringLiteral("SortName"), 300, 0,
      [this, gen](const QJsonDocument& doc, const QString& error) {
        if (gen != libraryGen_) return;
        setLoading(false);
        if (!error.isEmpty()) {
          emit statusRequested(error);
          return;
        }
        libraryItems_ = toList(doc.object().value(QStringLiteral("Items")).toArray());
        emit libraryChanged();
      });
}

void LibraryController::openFavorites() {
  libraryParentId_.clear();
  libraryTitle_ = TranslateMain("Preferiti");
  const quint64 gen = ++libraryGen_;
  libraryItems_.clear();
  emit libraryChanged();
  setLoading(true);
  emit statusRequested(QString());
  client_->fetchFavorites(200, [this, gen](const QJsonDocument& doc,
                                           const QString& error) {
    if (gen != libraryGen_) return;
    setLoading(false);
    if (!error.isEmpty()) {
      emit statusRequested(error);
      return;
    }
    libraryItems_ = toList(doc.object().value(QStringLiteral("Items")).toArray());
    emit libraryChanged();
  });
}

void LibraryController::openItem(const QString& itemId) {
  const quint64 gen = ++itemGen_;
  // If it is a different title, don't show the previous card for 10 s.
  if (detail_.value(QStringLiteral("id")).toString() != itemId) {
    detail_.clear();
    detail_.insert(QStringLiteral("id"), itemId);
    emit detailChanged();
  }
  setLoading(true);
  client_->fetchItem(itemId, [this, gen](const QJsonDocument& doc,
                                         const QString& error) {
    if (gen != itemGen_) return;
    setLoading(false);
    if (!error.isEmpty()) {
      emit statusRequested(error);
      detail_.insert(QStringLiteral("name"), TranslateApp("Impossibile caricare"));
      detail_.insert(QStringLiteral("overview"), error);
      emit detailChanged();
      return;
    }
    const QJsonObject o = doc.object();
    detail_ = toMap(o);
    detail_.insert(QStringLiteral("communityRating"),
                   o.value(QStringLiteral("CommunityRating")).toDouble());
    detail_.insert(QStringLiteral("officialRating"),
                   o.value(QStringLiteral("OfficialRating")).toString());
    detail_.insert(QStringLiteral("tagline"),
                   o.value(QStringLiteral("Tagline")).toString());
    emit detailChanged();
    // The player layer builds audio/subtitle lists from the raw item.
    emit itemLoaded(o);

    const QString type = o.value(QStringLiteral("Type")).toString();
    if (type == QLatin1String("Series")) {
      seriesId_ = o.value(QStringLiteral("Id")).toString();
      seriesTitle_ = o.value(QStringLiteral("Name")).toString();
      seriesPoster_ = detail_.value(QStringLiteral("image")).toString();
      seriesOverview_ = o.value(QStringLiteral("Overview")).toString();
      seasons_.clear();
      emit seasonsChanged();
      const quint64 seasonGen = ++seasonsGen_;
      client_->fetchSeasons(
          seriesId_, [this, seasonGen](const QJsonDocument& seasonsDoc,
                                       const QString& seasonError) {
            if (seasonGen != seasonsGen_) return;
            if (!seasonError.isEmpty()) {
              emit statusRequested(seasonError);
              return;
            }
            seasons_ = toList(seasonsDoc.object()
                                  .value(QStringLiteral("Items"))
                                  .toArray());
            for (QVariant& value : seasons_) {
              QVariantMap map = value.toMap();
              map.insert(QStringLiteral("name"),
                         map.value(QStringLiteral("name")).toString());
              value = map;
            }
            emit seasonsChanged();
          });
    } else if (type == QLatin1String("Episode")) {
      const QString seriesId = o.value(QStringLiteral("SeriesId")).toString();
      if (!seriesId.isEmpty()) seriesId_ = seriesId;
    }
  });
}

void LibraryController::openSeason(int index) {
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
  client_->fetchEpisodes(
      seriesId_, currentSeasonId_,
      [this, gen](const QJsonDocument& doc, const QString& error) {
        if (gen != episodesGen_) return;
        setLoading(false);
        if (!error.isEmpty()) {
          emit statusRequested(error);
          return;
        }
        episodes_ = toList(doc.object().value(QStringLiteral("Items")).toArray());
        emit episodesChanged();
      });
}

void LibraryController::reopenSeries() {
  if (seriesId_.isEmpty()) return;
  openItem(seriesId_);
}

void LibraryController::patchLists(const QString& itemId, const QString& key,
                                   const QVariant& value) {
  auto patch = [&](QVariantList& list) {
    for (int i = 0; i < list.size(); ++i) {
      QVariantMap map = list.at(i).toMap();
      if (map.value(QStringLiteral("id")).toString() == itemId) {
        map.insert(key, value);
        list[i] = map;
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

void LibraryController::togglePlayed() {
  const QString id = detail_.value(QStringLiteral("id")).toString();
  if (id.isEmpty()) return;
  const bool played = detail_.value(QStringLiteral("played")).toBool();
  client_->setPlayed(
      id, !played, [this, id, played](const QJsonDocument&, const QString& error) {
        if (!error.isEmpty()) {
          emit statusRequested(error);
          return;
        }
        // Like the server: marking watched/unwatched also resets the resume
        // position, otherwise "Resume" and "Play from start" would remain.
        detail_.insert(QStringLiteral("played"), !played);
        detail_.insert(QStringLiteral("progress"), 0.0);
        detail_.insert(QStringLiteral("resumeTicks"), qint64(0));
        emit detailChanged();
        patchLists(id, QStringLiteral("played"), !played);
        patchLists(id, QStringLiteral("progress"), 0.0);
        patchLists(id, QStringLiteral("resumeTicks"), qint64(0));
        // "Continue watching" and "Next up" must be recalculated by the
        // server: a series can advance to the next episode.
        QTimer::singleShot(kHomeRefreshDelayMs, this, [this]() { emit homeRefreshRequested(); });
        emit noticeRequested(!played ? TranslateApp("Segnato come visto")
                                     : TranslateApp("Segnato come non visto"));
      });
}

void LibraryController::toggleFavorite() {
  const QString id = detail_.value(QStringLiteral("id")).toString();
  if (id.isEmpty()) return;
  const bool favorite = detail_.value(QStringLiteral("favorite")).toBool();
  client_->setFavorite(
      id, !favorite, [this, id, favorite](const QJsonDocument&, const QString& error) {
        if (!error.isEmpty()) {
          emit statusRequested(error);
          return;
        }
        detail_.insert(QStringLiteral("favorite"), !favorite);
        emit detailChanged();
        patchLists(id, QStringLiteral("favorite"), !favorite);
        emit noticeRequested(!favorite ? TranslateApp("Aggiunto ai preferiti")
                                       : TranslateApp("Rimosso dai preferiti"));
      });
}

void LibraryController::toggleSeasonPlayed(const QString& seasonId) {
  if (seasonId.isEmpty()) return;
  bool played = false;
  for (const QVariant& value : seasons_) {
    const QVariantMap map = value.toMap();
    if (map.value(QStringLiteral("id")).toString() == seasonId) {
      played = map.value(QStringLiteral("played")).toBool();
      break;
    }
  }
  client_->setPlayed(
      seasonId, !played,
      [this, seasonId, played](const QJsonDocument&, const QString& error) {
        if (!error.isEmpty()) {
          emit statusRequested(error);
          return;
        }
        for (int i = 0; i < seasons_.size(); ++i) {
          QVariantMap map = seasons_.at(i).toMap();
          if (map.value(QStringLiteral("id")).toString() == seasonId) {
            map.insert(QStringLiteral("played"), !played);
            map.insert(QStringLiteral("progress"), 0.0);
            seasons_[i] = map;
            break;
          }
        }
        if (seasonInfo_.value(QStringLiteral("id")).toString() == seasonId) {
          seasonInfo_.insert(QStringLiteral("played"), !played);
          seasonInfo_.insert(QStringLiteral("progress"), 0.0);
        }
        emit seasonsChanged();
        emit episodesChanged();
        emit noticeRequested(!played ? TranslateApp("Stagione segnata come vista")
                                     : TranslateApp("Stagione segnata come non vista"));
        QTimer::singleShot(kHomeRefreshDelayMs, this, [this]() { emit homeRefreshRequested(); });
      });
}

void LibraryController::toggleSeasonFavorite(const QString& seasonId) {
  if (seasonId.isEmpty()) return;
  const bool favorite = seasonInfo_.value(QStringLiteral("favorite")).toBool();
  client_->setFavorite(
      seasonId, !favorite,
      [this, seasonId, favorite](const QJsonDocument&, const QString& error) {
        if (!error.isEmpty()) {
          emit statusRequested(error);
          return;
        }
        if (seasonInfo_.value(QStringLiteral("id")).toString() == seasonId) {
          seasonInfo_.insert(QStringLiteral("favorite"), !favorite);
        }
        for (int i = 0; i < seasons_.size(); ++i) {
          QVariantMap map = seasons_.at(i).toMap();
          if (map.value(QStringLiteral("id")).toString() == seasonId) {
            map.insert(QStringLiteral("favorite"), !favorite);
            seasons_[i] = map;
            break;
          }
        }
        emit seasonsChanged();
        emit episodesChanged();
        emit noticeRequested(!favorite ? TranslateApp("Stagione aggiunta ai preferiti")
                                       : TranslateApp("Stagione rimossa dai preferiti"));
      });
}

QVariantMap LibraryController::toMap(const QJsonObject& item) const {
  QVariantMap map;
  const QString id = item.value(QStringLiteral("Id")).toString();
  map.insert(QStringLiteral("id"), id);
  map.insert(QStringLiteral("name"), item.value(QStringLiteral("Name")).toString());
  map.insert(QStringLiteral("type"), item.value(QStringLiteral("Type")).toString());
  map.insert(QStringLiteral("seriesId"),
             item.value(QStringLiteral("SeriesId")).toString());
  map.insert(QStringLiteral("seasonId"),
             item.value(QStringLiteral("SeasonId")).toString());
  map.insert(QStringLiteral("indexNumber"),
             item.value(QStringLiteral("IndexNumber")).toInt());
  map.insert(QStringLiteral("parentIndexNumber"),
             item.value(QStringLiteral("ParentIndexNumber")).toInt());
  map.insert(QStringLiteral("overview"),
             item.value(QStringLiteral("Overview")).toString());
  {
    QVariantList genres;
    for (const QJsonValue& genre : item.value(QStringLiteral("Genres")).toArray()) {
      genres.append(genre.toString());
    }
    map.insert(QStringLiteral("genres"), genres);
  }
  map.insert(QStringLiteral("childCount"),
             item.value(QStringLiteral("ChildCount")).toInt());
  map.insert(QStringLiteral("rating"),
             item.value(QStringLiteral("CommunityRating")).toDouble());
  map.insert(QStringLiteral("communityRating"),
             item.value(QStringLiteral("CommunityRating")).toDouble());
  map.insert(QStringLiteral("officialRating"),
             item.value(QStringLiteral("OfficialRating")).toString());

  // Technical details from the first source: resolution and audio.
  QString resolution;
  QString audio;
  const QJsonArray sources = item.value(QStringLiteral("MediaSources")).toArray();
  if (!sources.isEmpty()) {
    const QJsonArray streams =
        sources.at(0).toObject().value(QStringLiteral("MediaStreams")).toArray();
    for (const QJsonValue& streamValue : streams) {
      const QJsonObject stream = streamValue.toObject();
      const QString type = stream.value(QStringLiteral("Type")).toString();
      if (type == QLatin1String("Video") && resolution.isEmpty()) {
        const int height = stream.value(QStringLiteral("Height")).toInt();
        if (height >= 1900) resolution = QStringLiteral("4K");
        else if (height >= 1000) resolution = QStringLiteral("1080p");
        else if (height >= 700) resolution = QStringLiteral("720p");
        else if (height > 0) resolution = QStringLiteral("%1p").arg(height);
      } else if (type == QLatin1String("Audio") && audio.isEmpty()) {
        const QString codec =
            stream.value(QStringLiteral("Codec")).toString().toUpper();
        const int channels = stream.value(QStringLiteral("Channels")).toInt();
        QString layout;
        if (channels == 6) layout = QStringLiteral(" 5.1");
        else if (channels == 8) layout = QStringLiteral(" 7.1");
        else if (channels == 2) layout = QStringLiteral(" stereo");
        audio = codec + layout;
      }
    }
  }
  map.insert(QStringLiteral("resolution"), resolution);
  map.insert(QStringLiteral("audio"), audio);

  const int year = item.value(QStringLiteral("ProductionYear")).toInt();
  map.insert(QStringLiteral("year"), year);
  map.insert(QStringLiteral("dateCreated"),
             item.value(QStringLiteral("DateCreated")).toString());
  const qint64 runtime =
      static_cast<qint64>(item.value(QStringLiteral("RunTimeTicks")).toDouble());
  map.insert(QStringLiteral("runtimeTicks"), runtime);

  QString subtitle;
  if (year > 0) subtitle = QString::number(year);
  const QString duration = formatDuration(runtime);
  map.insert(QStringLiteral("duration"), duration);
  if (!duration.isEmpty()) {
    subtitle += (subtitle.isEmpty() ? QString() : QStringLiteral(" · ")) + duration;
  }

  // For episodes the useful title is "Series · S1 E3"
  const QString series = item.value(QStringLiteral("SeriesName")).toString();
  if (!series.isEmpty()) {
    const int season = item.value(QStringLiteral("ParentIndexNumber")).toInt();
    const int episode = item.value(QStringLiteral("IndexNumber")).toInt();
    subtitle = series;
    if (season > 0 && episode > 0) {
      subtitle += QStringLiteral(" · S%1 E%2").arg(season).arg(episode);
    }
  }
  map.insert(QStringLiteral("subtitle"), subtitle);

  const QJsonObject tags = item.value(QStringLiteral("ImageTags")).toObject();
  map.insert(QStringLiteral("image"),
             client_->imageUrl(id, QStringLiteral("Primary"), 400,
                               tags.value(QStringLiteral("Primary")).toString()));
  const QJsonArray backdrops =
      item.value(QStringLiteral("BackdropImageTags")).toArray();
  map.insert(QStringLiteral("backdrop"),
             backdrops.isEmpty()
                 ? QString()
                 : client_->imageUrl(id, QStringLiteral("Backdrop"), 1280,
                                     backdrops.at(0).toString()));

  // --- horizontal cards (Continue watching / Next up) ---------
  // Episodes use the 16:9 screenshot as Primary; movies use the Thumb.
  const double imageAspect =
      item.value(QStringLiteral("PrimaryImageAspectRatio")).toDouble();
  const QString thumbTag = tags.value(QStringLiteral("Thumb")).toString();
  const QString primaryTag = tags.value(QStringLiteral("Primary")).toString();
  QString landscape;
  if (!thumbTag.isEmpty()) {
    landscape = client_->imageUrl(id, QStringLiteral("Thumb"), 640, thumbTag);
  } else if (imageAspect >= 1.2 && !primaryTag.isEmpty()) {
    landscape = client_->imageUrl(id, QStringLiteral("Primary"), 640, primaryTag);
  } else if (!backdrops.isEmpty()) {
    landscape = client_->imageUrl(id, QStringLiteral("Backdrop"), 1280,
                                  backdrops.at(0).toString());
  } else {
    landscape = map.value(QStringLiteral("image")).toString();
  }
  map.insert(QStringLiteral("imageLandscape"), landscape);

  QString cardTitle = item.value(QStringLiteral("Name")).toString();
  QString cardSubtitle = year > 0 ? QString::number(year) : subtitle;
  if (item.value(QStringLiteral("Type")).toString() == QLatin1String("Episode")) {
    if (!series.isEmpty()) cardTitle = series;
    const int season = item.value(QStringLiteral("ParentIndexNumber")).toInt();
    const int episode = item.value(QStringLiteral("IndexNumber")).toInt();
    if (season > 0 && episode > 0) {
      cardSubtitle = QStringLiteral("S%1 E%2").arg(season).arg(episode);
      const QString episodeName = item.value(QStringLiteral("Name")).toString();
      if (!episodeName.isEmpty()) cardSubtitle += QStringLiteral(" · ") + episodeName;
    }
  }
  map.insert(QStringLiteral("cardTitle"), cardTitle);
  map.insert(QStringLiteral("cardSubtitle"), cardSubtitle);

  const QJsonObject userData = item.value(QStringLiteral("UserData")).toObject();
  map.insert(QStringLiteral("played"),
             userData.value(QStringLiteral("Played")).toBool());
  map.insert(QStringLiteral("favorite"),
             userData.value(QStringLiteral("IsFavorite")).toBool());
  map.insert(QStringLiteral("progress"),
             userData.value(QStringLiteral("PlayedPercentage")).toDouble());
  map.insert(QStringLiteral("resumeTicks"),
             static_cast<qint64>(
                 userData.value(QStringLiteral("PlaybackPositionTicks")).toDouble()));
  return map;
}

QVariantList LibraryController::toList(const QJsonArray& items) const {
  QVariantList out;
  out.reserve(items.size());
  for (const QJsonValue& value : items) out.append(toMap(value.toObject()));
  return out;
}
