#ifndef PLAYER_H
#define PLAYER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QThread>

#include <SLAudio.h>
#include <SLVideo.h>

#include <cstdint>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

class QNetworkAccessManager;
class TsDemux;
struct AVCodecContext;
struct AVCodecParserContext;
struct SwrContext;

// Motore di riproduzione: HLS -> demux TS -> H.264 a SLVideo, AAC -> PCM a SLAudio.
//
// Gira su un thread dedicato perche' la decodifica audio e il pacing video non
// devono mai bloccare la UI. Il clock master e' l'audio: il video insegue.
class Player : public QThread {
  Q_OBJECT

 public:
  struct Config {
    QString masterUrl;
    QString itemId;
    QString playSessionId;
    QString title;
    QString authHeader;
    qint64 startTicks = 0;
    qint64 durationTicks = 0;
  };

  explicit Player(QObject* parent = nullptr);
  ~Player() override;

  void begin(const Config& config);
  void requestStop();
  void setPaused(bool paused);
  bool paused() const { return paused_.load(); }
  double positionSeconds() const { return position_.load(); }
  QString lastError() const { return error_; }

 signals:
  void started();
  void positionChanged(double seconds);
  void failed(const QString& message);
  void finished();
  // Nessun dato per un po': l'app mostra "connessione persa" senza chiudere.
  void stalled(bool stalled);
  void statsChanged(const QVariantMap& stats);

 protected:
  void run() override;

 private:
  bool fetch(QNetworkAccessManager& nam, const QString& url, QByteArray* out);
  bool openStreams();
  void closeStreams();
  bool loadPlaylist(QNetworkAccessManager& nam);
  bool fetchNextSegment(QNetworkAccessManager& nam, QByteArray* out);
  void demuxSegment(const QByteArray& body);
  bool takeSegment(QByteArray* out, int timeoutMs);
  void producerLoop();
  void onVideoPacket(const uint8_t* data, size_t size, int64_t pts);
  void onAudioPacket(const uint8_t* data, size_t size, int64_t pts);
  void scanAccessUnits();
  void emitAccessUnit(size_t from, size_t to);
  void decodeAudio(const uint8_t* data, size_t size, int64_t pts);
  double audioClock() const;
  uint32_t queuedAudioFrames() const;
  bool pushAudioBlock();
  bool pushVideoFrame();

  Config cfg_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> paused_{false};
  std::atomic<double> position_{0.0};
  QString error_;

  QNetworkAccessManager* namRef_ = nullptr;

  // HLS
  QString playlistUrl_;
  QStringList segmentUrls_;

  // coda dei segmenti scaricati da un thread separato: la rete non deve mai
  // bloccare la presentazione dei frame
  std::deque<QByteArray> segmentQueue_;
  std::mutex queueMutex_;
  std::condition_variable queueCv_;
  class ProducerThread;
  ProducerThread* producer_ = nullptr;
  std::atomic<bool> producerFinished_{false};
  // Solo per il build diagnostico: in che fase si trova il thread di rete.
  std::atomic<int> producerState_{0};
  QElapsedTimer lastData_;
  bool stalled_ = false;
  QString producerError_;
  int nextSegment_ = 0;
  // Il server non applica StartTimeTicks alla playlist: l'avvio a una
  // posizione si fa partendo dal segmento che copre quel tempo.
  bool startApplied_ = false;
  qint64 startOffsetTicks_ = 0;
  bool playlistEnded_ = false;

  // demux
  TsDemux* demux_ = nullptr;

  // video
  std::vector<uint8_t> auBuf_;
  std::vector<std::pair<size_t, int64_t>> auPts_;
  // scansione incrementale degli access unit (evita O(n^2) per segmento)
  size_t auScanPos_ = 0;
  qint64 auLastStartCode_ = -1;
  bool auHasVcl_ = false;
  // demux a pezzi: il feed di un segmento intero blocca il player per 100-300 ms
  QByteArray demuxBuf_;
  int demuxPos_ = 0;
  QElapsedTimer demuxTimer_;
  int demuxIndex_ = 0;
  std::deque<std::pair<std::vector<uint8_t>, int64_t>> videoQueue_;
  int64_t lastVideoPts_ = 0;
  int submittedFrames_ = 0;
  int failedFrames_ = 0;
  int lastSegmentMs_ = 0;
  QVariantMap stats_;
  int64_t basePts_ = -1;
  int64_t firstVideoPts_ = -1;
  int64_t firstAudioPts_ = -1;
  double audioOffsetSec_ = 0.0;
  double playClock_ = 0.0;
  double lastWall_ = -1.0;
  int droppedFrames_ = 0;
  double resumeLeadUntil_ = 0.0;
  double videoFps_ = 25.0;
  double nextVideoTime_ = 0.0;

  // audio
  std::vector<int16_t> pcm_;
  size_t pcmRead_ = 0;
  uint64_t audioFramesSubmitted_ = 0;

  // output hardware
  CSLVideoContext* videoCtx_ = nullptr;
  CSLVideoStream* videoStream_ = nullptr;
  CSLAudioContext* audioCtx_ = nullptr;
  CSLAudioStream* audioStream_ = nullptr;

  // ffmpeg
  AVCodecContext* aacCtx_ = nullptr;
  AVCodecParserContext* aacParser_ = nullptr;
  SwrContext* swr_ = nullptr;
  uint8_t* pcmOut_ = nullptr;
  int pcmOutCapacity_ = 0;
};

#endif  // PLAYER_H
