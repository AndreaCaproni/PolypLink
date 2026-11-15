#include "player.h"

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
#include <ctime>
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

// Traccia su file, indipendente dal sistema di log di Qt: serve quando il
// thread del player non riesce a scrivere nel log principale.
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

bool IsStartCode(const uint8_t* d, size_t n, size_t i) {
  return i + 3 < n && d[i] == 0x00 && d[i + 1] == 0x00 && d[i + 2] == 0x01;
}

// Inizio di un access unit: AUD oppure prima slice del picture.
bool IsAccessUnitStart(const uint8_t* d, size_t n, size_t i) {
  if (!IsStartCode(d, n, i)) return false;
  const int type = d[i + 3] & 0x1F;
  if (type == 9) return true;
  if (type == 1 || type == 5) return i + 4 < n && (d[i + 4] & 0x80) != 0;
  return false;
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
  const bool ok = wait(15000);
  qInfo("[diag] ~Player: wait %lld ms -> %s", static_cast<long long>(t.elapsed()),
        ok ? "ok" : "TIMEOUT");
}

static void* tid() { return reinterpret_cast<void*>(QThread::currentThreadId()); }

void Player::begin(const Config& config) {
  cfg_ = config;
  stop_.store(false);
  paused_.store(false);
  position_.store(config.startTicks / 1e7);
  start();
}

void Player::requestStop() {
  qInfo("[diag] requestStop (thread %p, producerState=%d, coda=%d)", tid(),
        producerState_.load(), static_cast<int>(segmentUrls_.size()));
  stop_.store(true);
  // sveglia subito chi aspetta un segmento: senza questo lo stop poteva
  // restare appeso fino a 15 s e il thread veniva distrutto ancora attivo.
  queueCv_.notify_all();
}

// Il thread di rete deve essere un QThread: in un std::thread i QTimer (timeout
// e abort su stop) non funzionano e il player restava bloccato per secondi.
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
    // alla ripresa dai al decoder un pre-roll piu' generoso per 1,5 s
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    resumeLeadUntil_ = ts.tv_sec + ts.tv_nsec / 1e9 + 1.5;
  }
  paused_.store(paused);
}

bool Player::fetch(QNetworkAccessManager& nam, const QString& url, QByteArray* out) {
  QNetworkRequest req{QUrl(url)};
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  if (!cfg_.authHeader.isEmpty()) {
    req.setRawHeader("Authorization", cfg_.authHeader.toUtf8());
  }

  QNetworkReply* reply = nam.get(req);

  // Timeout esplicito: Qt 5.14 non ha setTransferTimeout().
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
  timeout.start(15000);

  // Se ci chiedono di fermarci, la richiesta in corso viene abortita subito:
  // cosi' il thread esce in fretta e i contesti SLVideo vengono rilasciati.
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

  const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const bool ok = reply->error() == QNetworkReply::NoError && status < 400;
  if (ok) {
    *out = reply->readAll();
  } else {
    error_ = QStringLiteral("HTTP %1: %2").arg(status).arg(reply->errorString());
  }

  QString shortUrl = url;
  if (shortUrl.size() > 110) shortUrl = shortUrl.left(110) + "...";
  qInfo("[player] GET %d %d byte %s", status, static_cast<int>(out->size()),
        qPrintable(shortUrl));
  reply->deleteLater();
  return ok;
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
// Un solo contesto video/audio per tutto il processo: distruggerli e ricrearli
// a ogni film lascia il display in uno stato in cui il piano video non si
// riaggancia (il primo film si vedeva, i successivi restavano neri).
CSLVideoContext* g_videoCtx = nullptr;
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
  for (int tentativo = 0; tentativo < 6 && videoCtx == nullptr && !stop_.load(); ++tentativo) {
    if (tentativo > 0) msleep(300);
    videoCtx = SLVideo_CreateContext();
  }
  qInfo("[player] SLVideo_CreateContext -> %p", static_cast<void*>(videoCtx));
  Trace("SLVideo_CreateContext -> %p", static_cast<void*>(videoCtx));
  if (videoCtx == nullptr) {
    error_ = QStringLiteral("SLVideo_CreateContext fallita");
    return false;
  }
  // low latency = 1: I/P frame, il caso per cui il decoder e' progettato
  CSLVideoStream* stream = SLVideo_CreateStream(videoCtx, k_ESLVideoFormatH264, 1);
  if (stream == nullptr) {
    error_ = QStringLiteral("SLVideo_CreateStream fallita");
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
    error_ = QStringLiteral("decoder AAC non trovato in libavcodec");
    return false;
  }
  aacCtx_ = avcodec_alloc_context3(codec);
  if (aacCtx_ == nullptr || avcodec_open2(aacCtx_, codec, nullptr) < 0) {
    error_ = QStringLiteral("apertura decoder AAC fallita");
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
    // HACK (da Moonlight): crea un overlay a schermo intero, mostralo e
    // nascondilo per forzare il compositore a riconfigurare il piano video.
    // Senza questo, il film successivo resta congelato sull'ultimo frame.
    CSLVideoOverlay* flushOverlay =
        SLVideo_CreateOverlay(static_cast<CSLVideoContext*>(videoCtx_), 1280, 720);
    if (flushOverlay != nullptr) {
      SLVideo_SetOverlayDisplayFullscreen(flushOverlay);
      SLVideo_ShowOverlay(flushOverlay);
      SLVideo_HideOverlay(flushOverlay);
      SLVideo_FreeOverlay(flushOverlay);
    }
    SLVideo_FreeContext(static_cast<CSLVideoContext*>(videoCtx_));
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

bool Player::loadPlaylist(QNetworkAccessManager& nam) {
  QByteArray body;
  if (!fetch(nam, cfg_.masterUrl, &body)) return false;

  QString text = QString::fromUtf8(body);
  const QUrl base(cfg_.masterUrl);

  if (!text.contains(QLatin1String("#EXTINF"))) {
    const QString variant = firstUriAfterStreamInf(text, base);
    if (variant.isEmpty()) {
      error_ = QStringLiteral("playlist senza varianti");
      return false;
    }
    // dati della variante, per il pannello statistiche
  for (const QString& line : text.split(QLatin1Char('\n'))) {
    if (!line.startsWith(QLatin1String("#EXT-X-STREAM-INF"))) continue;
    const QString res = line.section(QLatin1String("RESOLUTION="), 1, 1).section(QLatin1Char(','), 0, 0);
    const QString bw = line.section(QLatin1String("BANDWIDTH="), 1, 1).section(QLatin1Char(','), 0, 0);
    const QString codecs = line.section(QLatin1String("CODECS=\""), 1, 1).section(QLatin1Char('"'), 0, 0);
    const QString rate = line.section(QLatin1String("FRAME-RATE="), 1, 1).section(QLatin1Char(','), 0, 0);
    stats_.insert("resolution", res);
    stats_.insert("bandwidth", bw);
    stats_.insert("codecs", codecs);
    stats_.insert("framerate", rate);
    break;
  }
  qInfo("[player] variante: %s", qPrintable(variant));
    cfg_.masterUrl = variant;
    if (!fetch(nam, variant, &body)) return false;
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

  if (segmentUrls_.isEmpty()) {
    segmentUrls_ = fresh;
  } else if (fresh.size() > segmentUrls_.size()) {
    segmentUrls_ = fresh;
  }

  // Avvio a una posizione: il server mette in ogni segmento il suo
  // runtimeTicks assoluto. Si parte dal segmento che copre startTicks e il
  // server riavvia la transcodifica da li' (verificato: 200 in ~2,5 s).
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
    qInfo("[player] primo segmento: %s", qPrintable(segmentUrls_.first().right(70)));
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
        if (fetch(nam, url, out)) {
          ++nextSegment_;
          return true;
        }
        // 400/404 mentre la transcode sta partendo: riprova
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
        nextSegment_ - 1, static_cast<int>(body.size()),
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
  return true;
}

// Thread di rete: scarica i segmenti e li mette in coda. Non tocca mai
// SLVideo/SLAudio, cosi' non puo' far scattare la riproduzione.
void Player::producerLoop() {
  qInfo("[diag] producer start (thread %p)", tid());
  producerState_.store(1);
  QNetworkAccessManager nam;
  if (!loadPlaylist(nam)) {
    producerState_.store(9);
    producerError_ = error_.isEmpty() ? QStringLiteral("playlist non caricata") : error_;
    producerFinished_.store(true);
    queueCv_.notify_all();
    return;
  }
  qInfo("[player] producer: playlist pronta (%d segmenti)",
        static_cast<int>(segmentUrls_.size()));
  producerState_.store(2);

  while (!stop_.load()) {
    size_t pending = 0;
    {
      std::lock_guard<std::mutex> lock(queueMutex_);
      pending = segmentQueue_.size();
    }
    if (pending >= 4) {
      producerState_.store(3);  // coda piena
      msleep(50);
      continue;
    }
    producerState_.store(2);    // scarica segmento

    QByteArray body;
    if (fetchNextSegment(nam, &body)) {
      {
        std::lock_guard<std::mutex> lock(queueMutex_);
        segmentQueue_.push_back(std::move(body));
      }
      queueCv_.notify_all();
    } else if (stop_.load() || playlistEnded_) {
      break;
    } else if (!error_.isEmpty()) {
      producerError_ = error_;
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
  // I frame possono essere spezzati su piu' PES (tipico a 1080p): accumulo il
  // flusso elementare e ricostruisco gli access unit dai NAL, non dai PES.
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
  if (pts < 0) pts = lastVideoPts_ + 3754;  // ~23.976 fps di riserva
  lastVideoPts_ = pts;
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
  // ripartiamo dagli ultimi 4 byte gia' visti: una start code puo' essere
  // spezzata tra due payload. Le start code gia' gestite vengono saltate.
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
      newAu = hasVcl && i + 4 < n && (auBuf_[i + 4] & 0x80);  // prima slice
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
    // tieni un solo PTS di riferimento prima del taglio
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
    pkt->data = out;
    pkt->size = outSize;
    if (avcodec_send_packet(aacCtx_, pkt) == 0) {
      AVFrame* frame = av_frame_alloc();
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
        const int needed = maxOut * kAudioChannels * static_cast<int>(sizeof(int16_t));
        if (needed > pcmOutCapacity_) {
          pcmOut_ = static_cast<uint8_t*>(av_realloc(pcmOut_, needed));
          pcmOutCapacity_ = needed;
        }
        uint8_t* outPtr = pcmOut_;
        const int produced =
            swr_convert(swr_, &outPtr, maxOut,
                        const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
        if (produced > 0) {
          const int16_t* samples = reinterpret_cast<const int16_t*>(pcmOut_);
          pcm_.insert(pcm_.end(), samples, samples + produced * kAudioChannels);
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
    error_ = QStringLiteral("SLAudio_BeginFrame nullo");
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
    return true;  // non e' fatale: si prosegue
  }
  videoQueue_.pop_front();
  if (submittedFrames_ == 0) emit firstFrame();
  ++submittedFrames_;
  return true;
}

void Player::run() {
  qInfo("[diag] player start (thread %p)", tid());
  error_.clear();
  QElapsedTimer wall;
  wall.start();

  if (!openStreams()) {
    closeStreams();
    emit failed(error_);
    return;
  }
  producer_ = new ProducerThread(this);
  producer_->start();

  // aspetta che il producer abbia playlist e un paio di segmenti
  {
    std::unique_lock<std::mutex> lock(queueMutex_);
    queueCv_.wait_for(lock, std::chrono::seconds(25), [this] {
      return segmentQueue_.size() >= 2 || producerFinished_.load();
    });
  }
  if (!producerError_.isEmpty()) {
    error_ = producerError_;
    stop_.store(true);
    if (producer_ != nullptr) {
    producer_->wait(5000);
    delete producer_;
    producer_ = nullptr;
  }
    closeStreams();
    emit failed(error_);
    return;
  }

  // Il decoder ha bisogno del framerate atteso: senza, presenta i frame al
  // suo ritmo interno e il video sembra scattoso anche se i dati sono a posto.
  {
    double fps = stats_.value("framerate").toString().toDouble();
    if (fps <= 1.0) fps = 25.0;
    // Il decoder del Link vuole un framerate intero (Moonlight passa interi):
    // 23.976 -> 24, 29.97 -> 30.
    const unsigned fpsInt = static_cast<unsigned>(fps + 0.5);
    SLVideo_SetStreamTargetFramerate(static_cast<CSLVideoStream*>(videoStream_),
                                     fpsInt, 1);
    SLVideo_SetStreamTargetBitrate(static_cast<CSLVideoStream*>(videoStream_), 12000);
    videoFps_ = static_cast<double>(fpsInt);
    qInfo("[player] framerate target: %.3f fps", fps);
    Trace("framerate target: %.3f", fps);
  }

  qInfo("[player] avvio: %s", qPrintable(cfg_.title));
  emit started();

  // Prebuffer: demux dei primi segmenti gia' scaricati dal producer.
  for (int i = 0; i < 2 && !stop_.load(); ++i) {
    QByteArray segment;
    if (!takeSegment(&segment, 15000)) break;
    demuxSegment(segment);
  }
  // Allineamento: il clock audio parte dal primo campione inviato, che non e'
  // per forza il primo frame video. Misuro lo scarto in PTS e lo compenso.
  basePts_ = firstVideoPts_;
  if (firstAudioPts_ >= 0 && (basePts_ < 0 || firstAudioPts_ < basePts_)) {
    basePts_ = firstAudioPts_;
  }
  if (basePts_ < 0) basePts_ = 0;

  // Se il primo campione audio cade piu' avanti del primo frame video, la
  // compensazione corretta e' SILENZIO in testa all'audio: spostare il clock
  // farebbe saltare i frame video (il video "partiva in ritardo").
  const double audioDelay =
      (firstAudioPts_ >= 0) ? (firstAudioPts_ - basePts_) / 90000.0 : 0.0;
  audioOffsetSec_ = 0.0;
  if (audioDelay > 0.05) {
    const size_t silence = static_cast<size_t>(audioDelay * kAudioRate) * kAudioChannels;
    pcm_.insert(pcm_.begin(), silence, 0);
    qInfo("[player] allineamento: %.3f s di silenzio in testa all'audio", audioDelay);
  } else {
    qInfo("[player] allineamento: audio e video partono insieme");
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
      error_ = QStringLiteral("Connessione persa: il server non ha inviato dati.");
      failedNow = true;
      break;
    }

    const size_t pcmFrames = (pcm_.size() - pcmRead_) / kAudioChannels;

    // Demux a pezzi: un segmento intero bloccherebbe il player per 100-300 ms
    // ogni 3 s (il microlag). Prendiamo il segmento e lo diamo al demuxer in
    // chunk allineati ai pacchetti TS da 188 byte.
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

    // Niente nuovi segmenti da 5 s: mostra "connessione persa" senza chiudere.
    if (!paused_.load() && !stalled_ && lastData_.elapsed() > 5000) {
      stalled_ = true;
      emit stalled(true);
    }

    // Il producer ha chiuso: o e' finito il film, o e' morto per un errore.
    // In entrambi i casi non si resta sul fermo immagine.
    if (producerFinished_.load() && !stop_.load()) {
      bool queueEmpty = false;
      {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queueEmpty = segmentQueue_.empty();
      }
      const bool noMedia = videoQueue_.empty() &&
                           (pcm_.size() - pcmRead_) < static_cast<size_t>(kAudioChannels);
      if (queueEmpty && noMedia) {
        if (!producerError_.isEmpty()) {
          error_ = producerError_;
          failedNow = true;
        }
        break;
      }
    }

    // Il tempo lo detta l'orologio di sistema. L'audio serve solo come
    // correzione lenta di deriva: cosi' i salti del buffer audio non fanno
    // piu' correre il clock (era la causa degli scarti a raffica).
    const double nowWall = wall.elapsed() / 1000.0;
    if (lastWall_ < 0.0) lastWall_ = nowWall;
    const double wallDelta = nowWall - lastWall_;
    lastWall_ = nowWall;

    if (paused_.load()) {
      msleep(20);
      continue;
    }

    // audio
    if (queuedAudioFrames() < 4800 && pcmFrames >= static_cast<size_t>(kAudioBlockFrames)) {
      if (!pushAudioBlock()) {
        failedNow = true;
        break;
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

    // Video: in low-latency mode GetQueuedVideoMS() restituisce sempre 0,
    // quindi il ritmo lo detta il tempo: un frame ogni 1/fps.
    if (!videoQueue_.empty() && videoStream_ != nullptr) {
      struct timespec ts {};
      clock_gettime(CLOCK_MONOTONIC, &ts);
      const double nowMono = ts.tv_sec + ts.tv_nsec / 1e9;
      if (nextVideoTime_ <= 0.0) nextVideoTime_ = nowMono;

      const double interval = 1.0 / (videoFps_ > 1.0 ? videoFps_ : 25.0);
      if (nowMono >= nextVideoTime_) {
        pushVideoFrame();
        nextVideoTime_ += interval;
        // se siamo in ritardo di oltre mezzo secondo, riallinea senza accumulare
        if (nextVideoTime_ < nowMono - 0.5) nextVideoTime_ = nowMono;
      }
    }

    static qint64 lastStats = 0;
    static int framesInWindow = 0;
    static int lastSubmitted = 0;
    if (wall.elapsed() - lastStats > 1000) {
      lastStats = wall.elapsed();
      const int submitted = submittedFrames_;
      QVariantMap st = stats_;
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
      framesInWindow = 0;
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
    const bool joined = producer_->wait(15000);
    qInfo("[diag] join producer: %lld ms -> %s (state=%d)",
          static_cast<long long>(pt.elapsed()), joined ? "ok" : "TIMEOUT",
          producerState_.load());
    if (joined) {
      delete producer_;
    } else {
      // Meglio perdere il thread di rete che far abortire l'app: un QThread
      // distrutto mentre gira e' un qFatal.
      qWarning("[player] producer non termina: lo lascio andare");
      producer_->setParent(nullptr);
    }
    producer_ = nullptr;
  }
  qInfo("[diag] closeStreams inizio");
  closeStreams();
  qInfo("[diag] closeStreams fine");
  if (failedNow) emit failed(error_);
  else emit finished();
  qInfo("[diag] player end");
}
