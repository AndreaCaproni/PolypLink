#include "player.h"

#include "redact.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <SLAudio.h>
#include <SLVideo.h>

#include "tsdemux.h"

namespace {

// File trace, independent of the Qt logging system: needed when the
// player thread cannot write to the main log.
void Trace(const char* fmt, ...) {
  FILE* f = fopen("/tmp/player.trace", "a");
  if (f == nullptr) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  fputc('\n', f);
  fclose(f);
}

constexpr int kAudioRate = 48000;
constexpr int kAudioChannels = 2;
constexpr int kAudioBlockFrames = 960;  // 20 ms
constexpr size_t kAudioBlockBytes = kAudioBlockFrames * kAudioChannels * sizeof(int16_t);

// Hard limits for network data and decoded buffers on a 256 MB device.
constexpr int kNetworkTimeoutMs = 15000;
constexpr qint64 kMaxPlaylistBytes = 4 * 1024 * 1024;
constexpr qint64 kMaxSegmentBytes = 64 * 1024 * 1024;
constexpr size_t kMaxSegmentQueueBytes = 96 * 1024 * 1024;
constexpr size_t kMaxAccessUnitBytes = 16 * 1024 * 1024;
constexpr size_t kMaxVideoFrames = 240;
constexpr size_t kMaxPcmSamples = static_cast<size_t>(kAudioRate) * kAudioChannels * 5;
constexpr double kMaxAlignmentSilenceSec = 2.0;

// Remove token query parameters without rewriting unrelated parameters.
QString StripTokenQuery(const QString& url) {
  const int queryStart = url.indexOf(QLatin1Char('?'));
  if (queryStart < 0) return url;
  const int fragmentStart = url.indexOf(QLatin1Char('#'), queryStart);
  const QString base = url.left(queryStart);
  const QString query = url.mid(
      queryStart + 1,
      fragmentStart < 0 ? -1 : fragmentStart - queryStart - 1);
  const QString suffix = fragmentStart < 0 ? QString() : url.mid(fragmentStart);

  QStringList kept;
  for (const QString& part : query.split(QLatin1Char('&'))) {
    const QString key = part.section(QLatin1Char('='), 0, 0).toLower();
    if (key == QLatin1String("api_key") || key == QLatin1String("apikey") ||
        key == QLatin1String("api-key") || key == QLatin1String("access_token") ||
        key == QLatin1String("accesstoken") || key == QLatin1String("token") ||
        key == QLatin1String("secret") || key == QLatin1String("pw") ||
        key == QLatin1String("password")) {
      continue;
    }
    kept.append(part);
  }
  if (kept.isEmpty()) return base + suffix;
  return base + QLatin1Char('?') + kept.join(QLatin1Char('&')) + suffix;
}

int EffectivePort(const QUrl& url) {
  if (url.port() >= 0) return url.port();
  if (url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0) {
    return 443;
  }
  if (url.scheme().compare(QLatin1String("http"), Qt::CaseInsensitive) == 0) {
    return 80;
  }
  return -1;
}

bool SameOrigin(const QUrl& a, const QUrl& b) {
  return a.scheme().compare(b.scheme(), Qt::CaseInsensitive) == 0 &&
         a.host().compare(b.host(), Qt::CaseInsensitive) == 0 &&
         EffectivePort(a) == EffectivePort(b);
}

// A same-host HTTP -> HTTPS upgrade is safe for the token and is common
// behind a reverse proxy. Downgrades and other hosts are rejected.
bool IsAllowedRedirect(const QUrl& from, const QUrl& to) {
  if (SameOrigin(from, to)) return true;
  return from.scheme().compare(QLatin1String("http"), Qt::CaseInsensitive) == 0 &&
         to.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0 &&
         from.host().compare(to.host(), Qt::CaseInsensitive) == 0;
}

QString firstUriAfterStreamInf(const QString& text, const QUrl& base) {
  const QStringList lines = text.split(QLatin1Char('\n'));
  bool wantUri = false;
  for (const QString& raw : lines) {
    const QString line = raw.trimmed();
    if (line.isEmpty()) continue;
    if (line.startsWith(QLatin1String("#EXT-X-STREAM-INF"))) {
      wantUri = true;
      continue;
    }
    if (wantUri && !line.startsWith(QLatin1Char('#'))) {
      return base.resolved(QUrl(line)).toString();
    }
  }
  return QString();
}

}  // namespace

Player::Player(QObject* parent) : QThread(parent) {}

Player::~Player() {
  QElapsedTimer t;
  t.start();
  requestStop();
  const bool ok = wait(kStopTimeoutMs);
  qInfo("[diag] ~Player: wait %lld ms -> %s", static_cast<long long>(t.elapsed()),
        ok ? "ok" : "TIMEOUT");
}

static void* tid() { return reinterpret_cast<void*>(QThread::currentThreadId()); }

void Player::setError(const QString& text) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  error_ = text;
}

QString Player::errorText() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  return error_;
}

void Player::setProducerError(const QString& text) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  producerError_ = text;
}

bool Player::hasProducerError() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  return !producerError_.isEmpty();
}

QString Player::producerErrorText() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  return producerError_;
}

void Player::begin(const Config& config) {
  cfg_ = config;
  allowedOrigin_ = QUrl(config.masterUrl);
  stop_.store(false);
  paused_.store(false);
  position_.store(config.startTicks / 1e7);
  start();
}

void Player::requestStop() {
  qInfo("[diag] requestStop (thread %p, producerState=%d, coda=%d)", tid(),
        producerState_.load(), segmentCount_.load());
  stop_.store(true);
  // wake immediately whoever is waiting for a segment: without this the stop could
  // hang for up to 15 s and the thread was destroyed while still active.
  queueCv_.notify_all();
}

// The network thread must be a QThread: in a std::thread the QTimers (timeout
// and abort on stop) do not work and the player stayed stuck for seconds.
class Player::ProducerThread : public QThread {
 public:
  explicit ProducerThread(Player* owner) : owner_(owner) {}
  void run() override { owner_->producerLoop(); }
 private:
  Player* owner_;
};

void Player::setPaused(bool paused) {
  if (paused_.load() == paused) return;
  if (!paused) {
    // on resume give the decoder a more generous pre-roll for 1.5 s
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    resumeLeadUntil_ = ts.tv_sec + ts.tv_nsec / 1e9 + 1.5;
  }
  paused_.store(paused);
}

bool Player::fetch(QNetworkAccessManager& nam, const QString& url, QByteArray* out,
                   qint64 maxBytes, QString* finalUrl) {
  out->clear();
  QUrl current(StripTokenQuery(url));
  if (!current.isValid() || !SameOrigin(current, allowedOrigin_)) {
    setError(QStringLiteral("URL fuori dal server Jellyfin"));
    return false;
  }

  for (int redirects = 0; redirects < 5; ++redirects) {
    QNetworkRequest req(current);
    // Follow redirects manually: Qt would forward Authorization headers.
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, false);
    req.setAttribute(QNetworkRequest::MaximumDownloadBufferSizeAttribute,
                     static_cast<int>(maxBytes));
    if (!cfg_.authHeader.isEmpty()) {
      req.setRawHeader("Authorization", cfg_.authHeader.toUtf8());
    }

    QNetworkReply* reply = nam.get(req);
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout.start(kNetworkTimeoutMs);

    QTimer stopper;
    stopper.setInterval(100);
    QObject::connect(&stopper, &QTimer::timeout, reply, [this, reply]() {
      if (stop_.load()) {
        qInfo("[diag] abort richiesta in corso per stop");
        reply->abort();
      }
    });
    stopper.start();

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300 && status < 400) {
      const QByteArray location = reply->rawHeader("Location");
      reply->deleteLater();
      if (location.isEmpty()) {
        setError(QStringLiteral("redirect senza Location"));
        return false;
      }
      const QUrl next = current.resolved(QUrl::fromEncoded(location));
      if (!IsAllowedRedirect(current, next)) {
        setError(QStringLiteral("redirect verso un host non consentito"));
        return false;
      }
      if (!SameOrigin(current, next)) {
        // Upgrade to HTTPS: subsequent segments use the new origin.
        allowedOrigin_ = next;
      }
      current = next;
      continue;
    }

    const bool ok =
        reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    if (ok) {
      const QVariant length = reply->header(QNetworkRequest::ContentLengthHeader);
      if (length.isValid() && length.toLongLong() > maxBytes) {
        setError(QStringLiteral("risposta troppo grande"));
      } else {
        *out = reply->readAll();
        if (out->size() > maxBytes) {
          out->clear();
          setError(QStringLiteral("risposta troppo grande"));
        }
      }
    } else {
      setError(QStringLiteral("HTTP %1: %2").arg(status).arg(reply->errorString()));
    }

    QString shortUrl = RedactSecrets(current.toString());
    if (shortUrl.size() > 110) shortUrl = shortUrl.left(110) + "...";
    qInfo("[player] GET %d %d byte %s", status, static_cast<int>(out->size()),
          qPrintable(shortUrl));
    if (ok && !out->isEmpty() && finalUrl != nullptr) {
      *finalUrl = current.toString();
    }
    reply->deleteLater();
    return ok && !out->isEmpty();
  }

  setError(QStringLiteral("troppi redirect"));
  return false;
}

namespace {
void SlVideoLogCallback(void*, ESLVideoLog level, const char* message) {
  const char* tag = "info";
  if (level == k_ESLVideoLogError) tag = "ERRORE";
  else if (level == k_ESLVideoLogWarning) tag = "warn";
  else if (level == k_ESLVideoLogDebug) tag = "debug";
  qInfo("[slvideo:%s] %s", tag, message);
}
void SlAudioLogCallback(void*, ESLAudioLog level, const char* message) {
  const char* tag = "info";
  if (level == k_ESLAudioLogError) tag = "ERRORE";
  else if (level == k_ESLAudioLogWarning) tag = "warn";
  qInfo("[slaudio:%s] %s", tag, message);
}
}  // namespace

namespace {

constexpr int kOutputWidth = 1280;
constexpr int kOutputHeight = 720;

// The Marvell compositor keeps the previous plane configuration after a
// stream is destroyed: the next playback can show the last frame of the old
// video until the display re-evaluates the plane. A short-lived overlay,
// presented and hidden before the context is released, forces that refresh.
// This is a clean-room workaround built only on the public SLVideo API.
class PlaneRefresh {
 public:
  explicit PlaneRefresh(CSLVideoContext* context)
      : overlay_(context == nullptr
                     ? nullptr
                     : SLVideo_CreateOverlay(context, kOutputWidth, kOutputHeight)) {}

  ~PlaneRefresh() { release(); }

  PlaneRefresh(const PlaneRefresh&) = delete;
  PlaneRefresh& operator=(const PlaneRefresh&) = delete;

  void run() {
    if (overlay_ == nullptr) return;
    SLVideo_SetOverlayDisplayFullscreen(overlay_);
    SLVideo_ShowOverlay(overlay_);
    SLVideo_HideOverlay(overlay_);
  }

  // Must be called before the owning video context is freed.
  void release() {
    if (overlay_ != nullptr) {
      SLVideo_FreeOverlay(overlay_);
      overlay_ = nullptr;
    }
  }

 private:
  CSLVideoOverlay* overlay_ = nullptr;
};

// One audio context for the whole process: recreating it for every movie
// leaves the display in a state where the video plane does not reattach.
// The video context is created and released per playback below.
CSLAudioContext* g_audioCtx = nullptr;
}  // namespace

bool Player::openStreams() {
  SLVideo_SetLogLevel(k_ESLVideoLogWarning);
  SLVideo_SetLogFunction(SlVideoLogCallback, nullptr);
  SLAudio_SetLogLevel(k_ESLAudioLogWarning);
  SLAudio_SetLogFunction(SlAudioLogCallback, nullptr);
  Trace("openStreams: creo SLVideo");
  qInfo("[player] creo contesto SLVideo");
  CSLVideoContext* videoCtx = nullptr;
  for (int attempt = 0; attempt < 6 && videoCtx == nullptr && !stop_.load();
       ++attempt) {
    if (attempt > 0) msleep(300);
    videoCtx = SLVideo_CreateContext();
  }
  qInfo("[player] SLVideo_CreateContext -> %p", static_cast<void*>(videoCtx));
  Trace("SLVideo_CreateContext -> %p", static_cast<void*>(videoCtx));
  if (videoCtx == nullptr) {
    setError(QStringLiteral("SLVideo_CreateContext fallita"));
    return false;
  }

  // low latency = 1: I/P frames, the case the decoder is designed for
  CSLVideoStream* stream = SLVideo_CreateStream(videoCtx, k_ESLVideoFormatH264, 1);
  if (stream == nullptr) {
    setError(QStringLiteral("SLVideo_CreateStream fallita"));
    SLVideo_FreeContext(videoCtx);
    return false;
  }
  videoCtx_ = videoCtx;
  videoStream_ = stream;
  qInfo("[player] SLVideo stream ok");

  if (g_audioCtx == nullptr) g_audioCtx = SLAudio_CreateContext();
  CSLAudioContext* audioCtx = g_audioCtx;
  if (audioCtx != nullptr) {
    audioCtx_ = audioCtx;
    audioStream_ = SLAudio_CreateStream(audioCtx, kAudioRate, kAudioChannels,
                                        static_cast<int>(kAudioBlockBytes), 1);
  }
  qInfo("[player] SLAudio stream -> %p", static_cast<void*>(audioStream_));
  Trace("SLAudio stream -> %p", static_cast<void*>(audioStream_));
  if (audioStream_ == nullptr) {
    qWarning("[player] audio non disponibile, riproduco in silenzio");
  }

  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
  if (codec == nullptr) {
    setError(QStringLiteral("decoder AAC non trovato in libavcodec"));
    return false;
  }
  aacCtx_ = avcodec_alloc_context3(codec);
  if (aacCtx_ == nullptr || avcodec_open2(aacCtx_, codec, nullptr) < 0) {
    setError(QStringLiteral("apertura decoder AAC fallita"));
    return false;
  }
  aacParser_ = av_parser_init(AV_CODEC_ID_AAC);

  pcmOutCapacity_ = 64 * 1024;
  pcmOut_ = static_cast<uint8_t*>(av_malloc(pcmOutCapacity_));
  pcm_.reserve(kAudioRate * 4);

  demux_ = new TsDemux();
  demux_->setVideoCallback([this](const uint8_t* d, size_t n, int64_t pts) {
    onVideoPacket(d, n, pts);
  });
  demux_->setAudioCallback([this](const uint8_t* d, size_t n, int64_t pts) {
    onAudioPacket(d, n, pts);
  });

  Trace("stream aperti");
  qInfo("[player] stream aperti");
  return true;
}

void Player::closeStreams() {
  if (demux_ != nullptr) {
    demux_->flush();
    delete demux_;
    demux_ = nullptr;
  }
  if (videoStream_ != nullptr) {
    SLVideo_FreeStream(static_cast<CSLVideoStream*>(videoStream_));
    videoStream_ = nullptr;
  }
  if (videoCtx_ != nullptr) {
    CSLVideoContext* context = static_cast<CSLVideoContext*>(videoCtx_);
    PlaneRefresh refresh(context);
    refresh.run();
    // The overlay must be gone before the context is released.
    refresh.release();
    SLVideo_FreeContext(context);
    videoCtx_ = nullptr;
  }
  if (audioStream_ != nullptr) {
    SLAudio_FreeStream(static_cast<CSLAudioStream*>(audioStream_));
    audioStream_ = nullptr;
  }
  audioCtx_ = nullptr;
  if (swr_ != nullptr) {
    swr_free(&swr_);
    swr_ = nullptr;
  }
  if (aacParser_ != nullptr) {
    av_parser_close(aacParser_);
    aacParser_ = nullptr;
  }
  if (aacCtx_ != nullptr) {
    avcodec_free_context(&aacCtx_);
    aacCtx_ = nullptr;
  }
  if (pcmOut_ != nullptr) {
    av_free(pcmOut_);
    pcmOut_ = nullptr;
  }
}

void Player::releaseAudioContext() {
  // The video context is released per playback by closeStreams(); only the
  // process-wide audio context is left to clean up here.
  if (g_audioCtx != nullptr) {
    SLAudio_FreeContext(g_audioCtx);
    g_audioCtx = nullptr;
  }
}

bool Player::loadPlaylist(QNetworkAccessManager& nam) {
  QByteArray body;
  QString masterUrl;
  if (!fetch(nam, cfg_.masterUrl, &body, kMaxPlaylistBytes, &masterUrl)) {
    return false;
  }
  cfg_.masterUrl = masterUrl;

  QString text = QString::fromUtf8(body);
  const QUrl base(masterUrl);

  if (!text.contains(QLatin1String("#EXTINF"))) {
    const QString variant = firstUriAfterStreamInf(text, base);
    if (variant.isEmpty()) {
      setError(QStringLiteral("playlist senza varianti"));
      return false;
    }
    // variant data, for the stats panel
  for (const QString& line : text.split(QLatin1Char('\n'))) {
    if (!line.startsWith(QLatin1String("#EXT-X-STREAM-INF"))) continue;
    const QString res = line.section(QLatin1String("RESOLUTION="), 1, 1).section(QLatin1Char(','), 0, 0);
    const QString bw = line.section(QLatin1String("BANDWIDTH="), 1, 1).section(QLatin1Char(','), 0, 0);
    const QString codecs = line.section(QLatin1String("CODECS=\""), 1, 1).section(QLatin1Char('"'), 0, 0);
    const QString rate = line.section(QLatin1String("FRAME-RATE="), 1, 1).section(QLatin1Char(','), 0, 0);
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      stats_.insert("resolution", res);
      stats_.insert("bandwidth", bw);
      stats_.insert("codecs", codecs);
      stats_.insert("framerate", rate);
    }
    break;
  }
  qInfo("[player] variante: %s", qPrintable(RedactSecrets(variant)));
    QString finalVariant;
    if (!fetch(nam, variant, &body, kMaxPlaylistBytes, &finalVariant)) {
      return false;
    }
    cfg_.masterUrl = finalVariant;
    text = QString::fromUtf8(body);
  }

  playlistUrl_ = cfg_.masterUrl;
  const QUrl segBase(playlistUrl_);

  QStringList fresh;
  playlistEnded_ = false;
  bool expectSegment = false;
  for (const QString& raw : text.split(QLatin1Char('\n'))) {
    const QString line = raw.trimmed();
    if (line.isEmpty()) continue;
    if (line.startsWith(QLatin1String("#EXT-X-ENDLIST"))) {
      playlistEnded_ = true;
      continue;
    }
    if (line.startsWith(QLatin1String("#EXTINF"))) {
      expectSegment = true;
      continue;
    }
    if (line.startsWith(QLatin1Char('#'))) continue;
    if (expectSegment) {
      fresh.append(segBase.resolved(QUrl(line)).toString());
      expectSegment = false;
    }
  }

  constexpr int kMaxPlaylistSegments = 10000;
  if (fresh.isEmpty()) {
    setError(QStringLiteral("playlist vuota"));
    return false;
  }
  if (fresh.size() > kMaxPlaylistSegments) {
    setError(QStringLiteral("playlist troppo grande"));
    return false;
  }

  if (segmentUrls_.isEmpty()) {
    segmentUrls_ = fresh;
  } else {
    // Keep the pending segment aligned when the server uses a sliding window
    // or grows the VOD playlist.
    const QString pending =
        (nextSegment_ >= 0 && nextSegment_ < segmentUrls_.size())
            ? segmentUrls_.at(nextSegment_)
            : lastSegmentUrl_;
    int found = pending.isEmpty() ? -1 : fresh.indexOf(pending);
    if (found < 0 && !lastSegmentUrl_.isEmpty()) {
      found = fresh.indexOf(lastSegmentUrl_);
      nextSegment_ = found >= 0 ? found + 1 : 0;
    } else if (found >= 0) {
      nextSegment_ = found;
    } else {
      nextSegment_ = 0;
    }
    segmentUrls_ = fresh;
  }
  segmentCount_.store(segmentUrls_.size());

  // Starting at a position: the server puts its absolute runtimeTicks in
  // each segment. We start from the segment covering startTicks and the
  // server restarts transcoding from there (verified: 200 in ~2.5 s).
  if (!startApplied_ && !segmentUrls_.isEmpty()) {
    startApplied_ = true;
    const qint64 target = cfg_.startTicks;
    int idx = 0;
    for (int i = 0; i < segmentUrls_.size(); ++i) {
      const QString rt = QUrlQuery(QUrl(segmentUrls_.at(i)))
                             .queryItemValue(QStringLiteral("runtimeTicks"));
      if (rt.isEmpty()) continue;
      const qint64 t = rt.toLongLong();
      if (t <= target) {
        idx = i;
        startOffsetTicks_ = t;
      } else {
        break;
      }
    }
    nextSegment_ = idx;
    if (idx > 0) {
      qInfo("[player] parto dal segmento %d (%.1f s di %.1f s richiesti)", idx,
            startOffsetTicks_ / 1e7, target / 1e7);
    }
  }

  qInfo("[player] playlist: %d segmenti%s", static_cast<int>(segmentUrls_.size()),
        playlistEnded_ ? " (fine)" : "");
  if (!segmentUrls_.isEmpty()) {
    qInfo("[player] primo segmento: %s", qPrintable(RedactSecrets(segmentUrls_.first()).right(70)));
  }
  return !segmentUrls_.isEmpty();
}

bool Player::fetchNextSegment(QNetworkAccessManager& nam, QByteArray* out) {
  for (;;) {
    if (stop_.load()) return false;
    if (nextSegment_ < segmentUrls_.size()) {
      const QString url = segmentUrls_.at(nextSegment_);
      for (int tentativo = 0; tentativo < 4; ++tentativo) {
        if (stop_.load()) return false;
        if (tentativo > 0) msleep(300);
        if (fetch(nam, url, out, kMaxSegmentBytes)) {
          lastSegmentUrl_ = url;
          lastSegmentIndex_.store(nextSegment_);
          ++nextSegment_;
          return true;
        }
        // 400/404 while the transcode is starting: retry
        qInfo("[player] segmento %d non pronto, riprovo (%d)", nextSegment_, tentativo + 1);
      }
      return false;
    }
    if (playlistEnded_) return false;
    msleep(300);
    if (!loadPlaylist(nam)) return false;
  }
}

void Player::demuxSegment(const QByteArray& body) {
  if (demux_ == nullptr) return;
  QElapsedTimer timer;
  timer.start();
  const auto beforeVideo = videoQueue_.size();
  const auto beforePcm = pcm_.size();
  demux_->feed(reinterpret_cast<const uint8_t*>(body.constData()),
               static_cast<size_t>(body.size()));
  demux_->flush();
  lastSegmentMs_ = static_cast<int>(timer.elapsed());
  lastData_.restart();
  if (stalled_) {
    stalled_ = false;
    emit stalled(false);
  }
  qInfo("[player] segmento %d: %d byte, TS=%s, pid video=%d audio=%d, +%d AU, +%d campioni (demux %d ms)",
        lastSegmentIndex_.load(), static_cast<int>(body.size()),
        (!body.isEmpty() && static_cast<uint8_t>(body[0]) == 0x47) ? "si" : "NO",
        demux_->videoPid(), demux_->audioPid(),
        static_cast<int>(videoQueue_.size() - beforeVideo),
        static_cast<int>(pcm_.size() - beforePcm), lastSegmentMs_);
}

bool Player::takeSegment(QByteArray* out, int timeoutMs) {
  std::unique_lock<std::mutex> lock(queueMutex_);
  if (!queueCv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] {
        return !segmentQueue_.empty() || producerFinished_.load() || stop_.load();
      })) {
    return false;
  }
  if (segmentQueue_.empty()) return false;
  *out = std::move(segmentQueue_.front());
  segmentQueue_.pop_front();
  segmentQueueBytes_ -= static_cast<size_t>(out->size());
  return true;
}

// Network thread: downloads segments and queues them. It never touches
// SLVideo/SLAudio, so it cannot make playback stutter.
void Player::producerLoop() {
  qInfo("[diag] producer start (thread %p)", tid());
  producerState_.store(1);
  QNetworkAccessManager nam;
  if (!loadPlaylist(nam)) {
    producerState_.store(9);
    setProducerError(errorText().isEmpty() ? QStringLiteral("playlist non caricata") : errorText());
    producerFinished_.store(true);
    queueCv_.notify_all();
    return;
  }
  qInfo("[player] producer: playlist pronta (%d segmenti)",
        static_cast<int>(segmentUrls_.size()));
  producerState_.store(2);

  while (!stop_.load()) {
    size_t pending = 0;
    size_t pendingBytes = 0;
    {
      std::lock_guard<std::mutex> lock(queueMutex_);
      pending = segmentQueue_.size();
      pendingBytes = segmentQueueBytes_;
    }
    if (pending >= 4 || pendingBytes >= kMaxSegmentQueueBytes) {
      producerState_.store(3);  // queue full
      msleep(50);
      continue;
    }
    producerState_.store(2);    // download segment

    QByteArray body;
    if (fetchNextSegment(nam, &body)) {
      {
        std::lock_guard<std::mutex> lock(queueMutex_);
        segmentQueueBytes_ += static_cast<size_t>(body.size());
        segmentQueue_.push_back(std::move(body));
      }
      queueCv_.notify_all();
    } else if (stop_.load() || playlistEnded_) {
      break;
    } else if (!errorText().isEmpty()) {
      setProducerError(errorText());
      break;
    } else {
      msleep(150);
    }
  }

  producerState_.store(9);
  qInfo("[diag] producer end (thread %p)", tid());
  producerFinished_.store(true);
  queueCv_.notify_all();
}

void Player::onVideoPacket(const uint8_t* data, size_t size, int64_t pts) {
  if (size == 0) return;
  if (size > kMaxAccessUnitBytes ||
      auBuf_.size() + size > kMaxAccessUnitBytes) {
    qWarning("[player] access unit oltre il limite, scarto il buffer");
    auBuf_.clear();
    auPts_.clear();
    auScanPos_ = 0;
    auLastStartCode_ = -1;
    auHasVcl_ = false;
    return;
  }
  // Frames can be split across multiple PES (typical at 1080p): I accumulate the
  // elementary stream and rebuild the access units from the NALs, not from the PES.
  auPts_.push_back({auBuf_.size(), pts});
  auBuf_.insert(auBuf_.end(), data, data + size);
  scanAccessUnits();
}

void Player::emitAccessUnit(size_t from, size_t to) {
  if (to <= from) return;
  int64_t pts = -1;
  for (const auto& b : auPts_) {
    if (b.first <= from) pts = b.second;
    else break;
  }
  if (pts < 0) pts = lastVideoPts_ + 3754;  // ~23.976 fps fallback
  lastVideoPts_ = pts;
  if (videoQueue_.size() >= kMaxVideoFrames) {
    qWarning("[player] coda video piena, scarto il frame nuovo");
    return;
  }
  videoQueue_.emplace_back(
      std::vector<uint8_t>(auBuf_.begin() + static_cast<long>(from),
                           auBuf_.begin() + static_cast<long>(to)),
      pts);
  if (firstVideoPts_ < 0) firstVideoPts_ = pts;
}

void Player::scanAccessUnits() {
  const size_t n = auBuf_.size();
  if (n < 8) return;

  size_t auStart = 0;
  bool hasVcl = auHasVcl_;
  // restart from the last 4 bytes already seen: a start code can be
  // split across two payloads. Already handled start codes are skipped.
  size_t i = auScanPos_ > 4 ? auScanPos_ - 4 : 0;

  while (i + 4 < n) {
    if (!(auBuf_[i] == 0 && auBuf_[i + 1] == 0 && auBuf_[i + 2] == 1)) {
      ++i;
      continue;
    }
    const int type = auBuf_[i + 3] & 0x1F;
    const size_t scStart = (i > 0 && auBuf_[i - 1] == 0) ? i - 1 : i;
    if (auLastStartCode_ >= 0 && static_cast<qint64>(scStart) <= auLastStartCode_) {
      i += 3;
      continue;
    }
    auLastStartCode_ = static_cast<qint64>(scStart);

    const bool isVcl = (type == 1 || type == 5);
    bool newAu = false;
    if (type == 9) {
      newAu = i > auStart;                                    // AUD
    } else if (isVcl) {
      newAu = hasVcl && i + 4 < n && (auBuf_[i + 4] & 0x80);  // first slice
    } else if (type == 6 || type == 7 || type == 8) {
      newAu = hasVcl;                                         // SEI/SPS/PPS
    }

    if (newAu) {
      emitAccessUnit(auStart, scStart);
      auStart = scStart;
      hasVcl = false;
    }
    if (isVcl) hasVcl = true;
    i += 3;
  }

  const size_t scannedTo = i;
  if (auStart > 0) {
    auBuf_.erase(auBuf_.begin(), auBuf_.begin() + static_cast<long>(auStart));
    // keep a single PTS of reference before the cut
    size_t keep = 0;
    while (keep + 1 < auPts_.size() && auPts_[keep + 1].first <= auStart) ++keep;
    if (keep > 0) auPts_.erase(auPts_.begin(), auPts_.begin() + static_cast<long>(keep));
    for (auto& b : auPts_) b.first = (b.first > auStart) ? b.first - auStart : 0;
    if (auLastStartCode_ >= 0) {
      auLastStartCode_ = (auLastStartCode_ > static_cast<qint64>(auStart))
                             ? auLastStartCode_ - static_cast<qint64>(auStart)
                             : 0;
    }
  }

  size_t newEnd = (scannedTo > auStart) ? scannedTo - auStart : 0;
  auScanPos_ = newEnd;
  auHasVcl_ = hasVcl;
}

void Player::onAudioPacket(const uint8_t* data, size_t size, int64_t pts) {
  if (firstAudioPts_ < 0 && pts >= 0) firstAudioPts_ = pts;
  decodeAudio(data, size, pts);
}

void Player::decodeAudio(const uint8_t* data, size_t size, int64_t pts) {
  if (aacParser_ == nullptr || aacCtx_ == nullptr) return;

  uint8_t* out = nullptr;
  int outSize = 0;

  while (size > 0) {
    const int used = av_parser_parse2(aacParser_, aacCtx_, &out, &outSize, data,
                                      static_cast<int>(size), pts, pts, 0);
    if (used < 0) break;
    data += used;
    size -= static_cast<size_t>(used);
    if (used == 0) break;

    if (outSize <= 0) continue;

    AVPacket* pkt = av_packet_alloc();
    if (pkt == nullptr) {
      qWarning("[player] av_packet_alloc fallita");
      return;
    }
    pkt->data = out;
    pkt->size = outSize;
    if (avcodec_send_packet(aacCtx_, pkt) == 0) {
      AVFrame* frame = av_frame_alloc();
      if (frame == nullptr) {
        qWarning("[player] av_frame_alloc fallita");
        av_packet_free(&pkt);
        return;
      }
      while (avcodec_receive_frame(aacCtx_, frame) == 0) {
        if (swr_ == nullptr) {
          AVChannelLayout outLayout{};
          av_channel_layout_default(&outLayout, kAudioChannels);
          if (swr_alloc_set_opts2(&swr_, &outLayout, AV_SAMPLE_FMT_S16, kAudioRate,
                                  &frame->ch_layout,
                                  static_cast<AVSampleFormat>(frame->format),
                                  frame->sample_rate, 0, nullptr) < 0 ||
              swr_init(swr_) < 0) {
            qWarning("[player] swresample non inizializzabile");
            swr_free(&swr_);
            av_frame_free(&frame);
            av_packet_free(&pkt);
            return;
          }
          qInfo("[player] audio: %d Hz, %d canali -> 48000/2",
                frame->sample_rate, frame->ch_layout.nb_channels);
        }

        const int maxOut = swr_get_out_samples(swr_, frame->nb_samples);
        constexpr int kMaxChannels = kAudioChannels;
        if (maxOut <= 0 ||
            maxOut > std::numeric_limits<int>::max() /
                         (kMaxChannels * static_cast<int>(sizeof(int16_t)))) {
          qWarning("[player] dimensione audio non valida");
          av_frame_free(&frame);
          av_packet_free(&pkt);
          return;
        }
        const qint64 needed64 =
            static_cast<qint64>(maxOut) * kMaxChannels * sizeof(int16_t);
        if (needed64 > 32 * 1024 * 1024) {
          qWarning("[player] buffer audio troppo grande");
          av_frame_free(&frame);
          av_packet_free(&pkt);
          return;
        }
        const int needed = static_cast<int>(needed64);
        if (needed > pcmOutCapacity_) {
          auto* resized = static_cast<uint8_t*>(av_realloc(pcmOut_, needed));
          if (resized == nullptr) {
            qWarning("[player] av_realloc audio fallita");
            av_frame_free(&frame);
            av_packet_free(&pkt);
            return;
          }
          pcmOut_ = resized;
          pcmOutCapacity_ = needed;
        }
        uint8_t* outPtr = pcmOut_;
        const int produced =
            swr_convert(swr_, &outPtr, maxOut,
                        const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
        if (produced > 0) {
          const int16_t* samples = reinterpret_cast<const int16_t*>(pcmOut_);
          pcm_.insert(pcm_.end(), samples, samples + produced * kAudioChannels);
          if (pcm_.size() - pcmRead_ > kMaxPcmSamples) {
            const size_t drop = pcm_.size() - pcmRead_ - kMaxPcmSamples;
            pcm_.erase(pcm_.begin() + static_cast<long>(pcmRead_),
                       pcm_.begin() + static_cast<long>(pcmRead_ + drop));
          }
        }
        av_frame_unref(frame);
      }
      av_frame_free(&frame);
    }
    av_packet_free(&pkt);
  }
}

uint32_t Player::queuedAudioFrames() const {
  if (audioStream_ == nullptr) return 0;
  return SLAudio_GetQueuedAudioSamples(static_cast<CSLAudioStream*>(audioStream_));
}

double Player::audioClock() const {
  if (audioStream_ == nullptr || audioFramesSubmitted_ == 0) return -1.0;
  const uint64_t queued = queuedAudioFrames();
  const uint64_t played = audioFramesSubmitted_ > queued ? audioFramesSubmitted_ - queued : 0;
  return static_cast<double>(played) / kAudioRate;
}

bool Player::pushAudioBlock() {
  if (audioStream_ == nullptr) return false;
  const size_t availableSamples = pcm_.size() - pcmRead_;
  if (availableSamples < kAudioBlockFrames * kAudioChannels) return false;

  void* dst = SLAudio_BeginFrame(static_cast<CSLAudioStream*>(audioStream_));
  if (dst == nullptr) {
    setError(QStringLiteral("SLAudio_BeginFrame nullo"));
    return false;
  }
  memcpy(dst, pcm_.data() + pcmRead_, kAudioBlockBytes);
  SLAudio_SubmitFrame(static_cast<CSLAudioStream*>(audioStream_));
  pcmRead_ += kAudioBlockFrames * kAudioChannels;
  audioFramesSubmitted_ += kAudioBlockFrames;

  if (pcmRead_ > pcm_.size() / 2) {
    pcm_.erase(pcm_.begin(), pcm_.begin() + static_cast<long>(pcmRead_));
    pcmRead_ = 0;
  }
  return true;
}

bool Player::pushVideoFrame() {
  if (videoStream_ == nullptr || videoQueue_.empty()) return false;

  const auto& frame = videoQueue_.front();
  auto* stream = static_cast<CSLVideoStream*>(videoStream_);
  if (SLVideo_BeginFrame(stream, static_cast<int>(frame.first.size())) != 0 ||
      SLVideo_WriteFrameData(stream, const_cast<uint8_t*>(frame.first.data()),
                             static_cast<int>(frame.first.size())) != 0 ||
      SLVideo_SubmitFrame(stream) != 0) {
    qWarning("[player] frame video rifiutato (%d byte)",
             static_cast<int>(frame.first.size()));
    ++failedFrames_;
    videoQueue_.pop_front();
    return true;  // it is not fatal: we continue
  }
  videoQueue_.pop_front();
  ++submittedFrames_;
  return true;
}

void Player::run() {
  qInfo("[diag] player start (thread %p)", tid());
  setError(QString());
  QElapsedTimer wall;
  wall.start();

  if (!openStreams()) {
    closeStreams();
    emit failed(errorText());
    return;
  }
  producer_ = new ProducerThread(this);
  producer_->start();

  // wait until the producer has the playlist and a couple of segments
  {
    std::unique_lock<std::mutex> lock(queueMutex_);
    queueCv_.wait_for(lock, std::chrono::seconds(25), [this] {
      return segmentQueue_.size() >= 2 || producerFinished_.load();
    });
  }
  if (hasProducerError()) {
    setError(producerErrorText());
    stop_.store(true);
    if (producer_ != nullptr) {
      const bool joined = producer_->wait(kStopTimeoutMs);
      if (joined) {
        delete producer_;
      } else {
        qWarning("[player] producer non termina: lo lascio andare");
        producer_->setParent(nullptr);
      }
      producer_ = nullptr;
    }
    closeStreams();
    emit failed(errorText());
    return;
  }

  // The decoder needs the expected framerate: without it, it presents frames at
  // its internal pace and the video looks choppy even if the data is fine.
  {
    double fps;
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      fps = stats_.value("framerate").toString().toDouble();
    }
    if (!std::isfinite(fps) || fps <= 1.0 || fps > 240.0) fps = 25.0;
    // The Link decoder wants an integer framerate (Moonlight passes integers):
    // 23.976 -> 24, 29.97 -> 30.
    const unsigned fpsInt = static_cast<unsigned>(std::lround(fps));
    SLVideo_SetStreamTargetFramerate(static_cast<CSLVideoStream*>(videoStream_),
                                     fpsInt, 1);
    SLVideo_SetStreamTargetBitrate(static_cast<CSLVideoStream*>(videoStream_), cfg_.maxBitrateKbps);
    videoFps_ = static_cast<double>(fpsInt);
    qInfo("[player] framerate target: %.3f fps", fps);
    Trace("framerate target: %.3f", fps);
  }

  qInfo("[player] avvio: %s", qPrintable(cfg_.title));
  emit started();

  // Prebuffer: demux of the first segments already downloaded by the producer.
  for (int i = 0; i < 2 && !stop_.load(); ++i) {
    QByteArray segment;
    if (!takeSegment(&segment, kNetworkTimeoutMs)) break;
    demuxSegment(segment);
  }
  // Alignment: the audio clock starts from the first submitted sample, which is
  // not necessarily the first video frame. I measure the offset in PTS and compensate for it.
  basePts_ = firstVideoPts_;
  if (firstAudioPts_ >= 0 && (basePts_ < 0 || firstAudioPts_ < basePts_)) {
    basePts_ = firstAudioPts_;
  }
  if (basePts_ < 0) basePts_ = 0;

  // If the first audio sample falls later than the first video frame, the
  // correct compensation is SILENCE at the head of the audio: shifting the clock
  // would make the video frames skip (the video "started late").
  const double audioDelay =
      (firstAudioPts_ >= 0) ? (firstAudioPts_ - basePts_) / 90000.0 : 0.0;
  const double videoLead =
      (firstAudioPts_ >= 0 && firstVideoPts_ >= 0)
          ? (firstVideoPts_ - firstAudioPts_) / 90000.0
          : 0.0;
  audioOffsetSec_ = 0.0;
  qInfo("[player] allineamento PTS: video=%lld audio=%lld (video-audio=%.3f s)",
        static_cast<long long>(firstVideoPts_),
        static_cast<long long>(firstAudioPts_), videoLead);
  if (audioDelay > 0.05 && audioDelay <= kMaxAlignmentSilenceSec) {
    const size_t frames = static_cast<size_t>(audioDelay * kAudioRate);
    const size_t silence = frames * kAudioChannels;
    pcm_.insert(pcm_.begin(), silence, 0);
    qInfo("[player] allineamento: %.3f s di silenzio in testa all'audio", audioDelay);
  } else if (audioDelay > kMaxAlignmentSilenceSec) {
    qWarning("[player] allineamento PTS anomalo (%.3f s), silenzio ignorato", audioDelay);
  } else {
    qInfo("[player] allineamento: audio e video partono insieme");
  }

  const int delayMs = std::clamp(cfg_.audioDelayMs, -2000, 2000);
  if (delayMs != 0) {
    const int frames = std::abs(delayMs) * kAudioRate / 1000;
    const size_t interleaved = static_cast<size_t>(frames) * kAudioChannels;
    if (delayMs > 0) {
      pcm_.insert(pcm_.begin(), interleaved, 0);
    } else {
      pcmRead_ = std::min(interleaved, pcm_.size());
    }
    qInfo("[player] ritardo audio utente: %d ms", delayMs);
  }

  playClock_ = 0.0;
  lastWall_ = -1.0;

  qInfo("[player] prebuffer: %d AU video, %d campioni audio",
        static_cast<int>(videoQueue_.size()),
        static_cast<int>(pcm_.size() - pcmRead_));

  lastData_.start();
  qint64 lastPosition = 0;
  bool failedNow = false;
  bool gotMedia = false;
  const qint64 startMs = wall.elapsed();

  static double lastLoopTime = -1.0;
  while (!stop_.load()) {
    const double nowLoop = wall.elapsed() / 1000.0;
    if (lastLoopTime > 0.0 && nowLoop - lastLoopTime > 0.40) {
      qWarning("[player] STALLO loop: %.0f ms (ultimo demux %d ms)",
               (nowLoop - lastLoopTime) * 1000.0, lastSegmentMs_);
    }
    lastLoopTime = nowLoop;

    if (!gotMedia && (!videoQueue_.empty() || !pcm_.empty())) {
      gotMedia = true;
      qInfo("[player] dati in arrivo: %d AU video, %d campioni audio",
            static_cast<int>(videoQueue_.size()), static_cast<int>(pcm_.size()));
    }
    if (!gotMedia && wall.elapsed() - startMs > 8000) {
      setError(QStringLiteral("Connessione persa: il server non ha inviato dati."));
      failedNow = true;
      break;
    }

    const size_t pcmFrames = (pcm_.size() - pcmRead_) / kAudioChannels;

    // Demux in chunks: a whole segment would block the player for 100-300 ms
    // every 3 s (the microlag). We take the segment and feed it to the demuxer in
    // chunks aligned to 188-byte TS packets.
    if (demuxPos_ >= demuxBuf_.size()) {
      demuxBuf_.clear();
      demuxPos_ = 0;
      if (videoQueue_.size() < 60 || pcmFrames < 24000) {
        QByteArray segment;
        if (takeSegment(&segment, 0)) {
          demuxBuf_ = std::move(segment);
          demuxTimer_.start();
        }
      }
    }
    if (demuxPos_ < demuxBuf_.size()) {
      constexpr int kChunk = 188 * 128;  // ~24 KB
      const int n = std::min<int>(kChunk, demuxBuf_.size() - demuxPos_);
      demux_->feed(reinterpret_cast<const uint8_t*>(demuxBuf_.constData()) + demuxPos_,
                   static_cast<size_t>(n));
      demuxPos_ += n;
      if (demuxPos_ >= demuxBuf_.size()) {
        demux_->flush();
        lastSegmentMs_ = static_cast<int>(demuxTimer_.elapsed());
        qInfo("[player] segmento %d: %d byte, TS=%s, demux %d ms",
              ++demuxIndex_, static_cast<int>(demuxBuf_.size()),
              (!demuxBuf_.isEmpty() &&
               static_cast<uint8_t>(demuxBuf_[0]) == 0x47) ? "si" : "NO",
              lastSegmentMs_);
        lastData_.restart();
        if (stalled_) {
          stalled_ = false;
          emit stalled(false);
        }
      }
    }

    // No new segments for 5 s: show "connection lost" without closing.
    if (!paused_.load() && !stalled_ && lastData_.elapsed() > 5000) {
      stalled_ = true;
      emit stalled(true);
    }

    // The producer has closed: either the movie ended, or it died from an error.
    // In both cases we do not stay on the freeze frame.
    if (producerFinished_.load() && !stop_.load()) {
      bool queueEmpty = false;
      {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queueEmpty = segmentQueue_.empty();
      }
      const bool noMedia = videoQueue_.empty() &&
                           (pcm_.size() - pcmRead_) < static_cast<size_t>(kAudioChannels);
      if (queueEmpty && noMedia) {
        if (hasProducerError()) {
          setError(producerErrorText());
          failedNow = true;
        }
        break;
      }
    }

    // Time is dictated by the system clock. Audio only serves as a
    // slow drift correction: so audio buffer jumps no longer make
    // the clock race (it was the cause of the bursts of drops).
    const double nowWall = wall.elapsed() / 1000.0;
    if (lastWall_ < 0.0) lastWall_ = nowWall;
    const double wallDelta = nowWall - lastWall_;
    lastWall_ = nowWall;

    if (paused_.load()) {
      msleep(20);
      continue;
    }

    // Do not keep feeding the decoder past the end of the media. Starting a
    // transcode near the end can make the server append segments beyond the
    // duration; the Link must stop here instead of playing them.
    const double durationSec = cfg_.durationTicks / 1e7;
    if (durationSec > 1.0 && position_.load() >= durationSec - 0.5) {
      qInfo("[player] fine media a %.1f s", position_.load());
      break;
    }

    // audio
    if (queuedAudioFrames() < 4800 && pcmFrames >= static_cast<size_t>(kAudioBlockFrames)) {
      const bool firstAudioBlock = audioFramesSubmitted_ == 0;
      if (!pushAudioBlock()) {
        failedNow = true;
        break;
      }
      if (firstAudioBlock && audioFramesSubmitted_ > 0) {
        qInfo("[player] primo blocco audio a t=%.2f s", nowWall);
      }
    }

    playClock_ += wallDelta;

    {
      const double ac = audioClock();
      if (ac >= 0.0) {
        const double av = audioOffsetSec_ + ac;
        const double drift = av - playClock_;
        if (drift > 0.10 || drift < -0.10) playClock_ += drift * 0.10;
      }
    }

    // Video: in low-latency mode GetQueuedVideoMS() always returns 0,
    // so the pace is dictated by time: one frame every 1/fps.
    if (!videoQueue_.empty() && videoStream_ != nullptr) {
      struct timespec ts {};
      clock_gettime(CLOCK_MONOTONIC, &ts);
      const double nowMono = ts.tv_sec + ts.tv_nsec / 1e9;
      if (nextVideoTime_ <= 0.0) nextVideoTime_ = nowMono;

      const double interval = 1.0 / (videoFps_ > 1.0 ? videoFps_ : 25.0);
      if (nowMono >= nextVideoTime_) {
        const bool firstVideoFrame = submittedFrames_ == 0;
        pushVideoFrame();
        if (firstVideoFrame && submittedFrames_ > 0) {
          qInfo("[player] primo frame video a t=%.2f s", nowWall);
        }
        nextVideoTime_ += interval;
        // if we are more than half a second late, resync without accumulating
        if (nextVideoTime_ < nowMono - 0.5) nextVideoTime_ = nowMono;
      }
    }

    static qint64 lastStats = 0;
    static int lastSubmitted = 0;
    if (wall.elapsed() - lastStats > 1000) {
      lastStats = wall.elapsed();
      const int submitted = submittedFrames_;
      QVariantMap st;
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        st = stats_;
      }
      st.insert("position", position_.load());
      st.insert("duration", cfg_.durationTicks / 1e7);
      st.insert("fps", submitted - lastSubmitted);
      st.insert("queueVideo", static_cast<int>(videoQueue_.size()));
      st.insert("queueAudio", queuedAudioFrames());
      st.insert("pcm", static_cast<int>((pcm_.size() - pcmRead_) / kAudioChannels));
      st.insert("segmentMs", lastSegmentMs_);
      st.insert("failed", failedFrames_);
      st.insert("dropped", droppedFrames_);
      lastSubmitted = submitted;
      emit statsChanged(st);
    }

    static qint64 lastDiag = 0;
    if (wall.elapsed() - lastDiag > 2000) {
      lastDiag = wall.elapsed();
      Trace("t=%.1f clk=%.2f qa=%u sub=%llu pcm=%zu qv=%zu pos=%.1f",
            wall.elapsed() / 1000.0, audioClock(), queuedAudioFrames(),
            static_cast<unsigned long long>(audioFramesSubmitted_),
            (pcm_.size() - pcmRead_) / kAudioChannels, videoQueue_.size(),
            submittedFrames_, droppedFrames_, position_.load());
      qInfo("[player] t=%.1fs clk=%.2f coda_audio=%u inviati=%llu pcm=%zu coda_video=%zu sub=%d drop=%d pos=%.1f",
            wall.elapsed() / 1000.0, audioClock(), queuedAudioFrames(),
            static_cast<unsigned long long>(audioFramesSubmitted_),
            (pcm_.size() - pcmRead_) / kAudioChannels, videoQueue_.size(),
            submittedFrames_, droppedFrames_, position_.load());
    }

    if (wall.elapsed() - lastPosition > 500) {
      lastPosition = wall.elapsed();
      position_.store(startOffsetTicks_ / 1e7 + playClock_);
      emit positionChanged(position_.load());
    }

    msleep(2);
  }

  stop_.store(true);
  qInfo("[diag] player loop uscito");
  if (producer_ != nullptr) {
    QElapsedTimer pt;
    pt.start();
    const bool joined = producer_->wait(kStopTimeoutMs);
    qInfo("[diag] join producer: %lld ms -> %s (state=%d)",
          static_cast<long long>(pt.elapsed()), joined ? "ok" : "TIMEOUT",
          producerState_.load());
    if (joined) {
      delete producer_;
    } else {
      // Better to lose the network thread than abort the app: a QThread
      // destroyed while running is a qFatal.
      qWarning("[player] producer non termina: lo lascio andare");
      producer_->setParent(nullptr);
    }
    producer_ = nullptr;
  }
  qInfo("[diag] closeStreams inizio");
  closeStreams();
  qInfo("[diag] closeStreams fine");
  if (failedNow) emit failed(errorText());
  else emit finished();
  qInfo("[diag] player end");
}
