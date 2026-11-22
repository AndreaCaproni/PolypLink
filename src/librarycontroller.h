#ifndef LIBRARY_CONTROLLER_H
#define LIBRARY_CONTROLLER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class JellyfinClient;

// Home rows, library grids, item details and season/episode browsing. Playback
// remains in AppController; when an item is loaded this class emits the raw
// JSON so the player layer can build its audio/subtitle track lists.
class LibraryController : public QObject {
  Q_OBJECT

 public:
  explicit LibraryController(JellyfinClient* client, QObject* parent = nullptr);

  QVariantList views() const { return views_; }
  QVariantList resume() const { return resume_; }
  QVariantList nextUp() const { return nextUp_; }
  QVariantList latest() const { return latest_; }
  bool loading() const { return loading_; }

  QString libraryTitle() const { return libraryTitle_; }
  QVariantList libraryItems() const { return libraryItems_; }
  int libraryIndex() const { return libraryIndex_; }

  QVariantMap detail() const { return detail_; }

  QVariantList seasons() const { return seasons_; }
  QString seriesTitle() const { return seriesTitle_; }
  QString seriesPoster() const { return seriesPoster_; }
  QVariantList episodes() const { return episodes_; }
  QString seasonTitle() const { return seasonTitle_; }
  QVariantMap seasonInfo() const { return seasonInfo_; }

  void loadHome();
  void clear();

  void setLoading(bool value);

  void openView(int index);
  void openFavorites();
  void openItem(const QString& itemId);
  void openSeason(int index);
  void reopenSeries();

  void togglePlayed();
  void toggleFavorite();
  void toggleSeasonPlayed(const QString& seasonId);
  void toggleSeasonFavorite(const QString& seasonId);

  void patchLists(const QString& itemId, const QString& key,
                  const QVariant& value);

  QVariantMap toMap(const QJsonObject& item) const;
  QVariantList toList(const QJsonArray& items) const;

 signals:
  void viewsChanged();
  void homeChanged();
  void libraryChanged();
  void detailChanged();
  void seasonsChanged();
  void episodesChanged();
  void loadingChanged();
  void statusRequested(const QString& status);
  void noticeRequested(const QString& message);
  void homeRefreshRequested();
  void itemLoaded(const QJsonObject& item);

 private:
  void loadLatest(quint64 generation);

  JellyfinClient* client_;

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

  quint64 itemGen_ = 0;
  quint64 libraryGen_ = 0;
  quint64 homeGen_ = 0;
  quint64 seasonsGen_ = 0;
  quint64 episodesGen_ = 0;
};

#endif  // LIBRARY_CONTROLLER_H
