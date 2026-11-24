#ifndef PLAYER_H
#define PLAYER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
#include <QUrl>
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

// Playback engine: HLS -> TS demux -> H.264 to SLVideo, AAC -> PCM to SLAudio.
//
// It runs on a dedicated thread because audio decoding and video pacing must
// never block the UI. The master clock is audio: video follows it.
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
    int maxBitrateKbps = 12000;
    // Manual audio/video offset: positive delays the audio, negative advances it.
    int audioDelayMs = 0;
  };

  explicit Player(QObject* parent = nullptr);
  ~Player() override;

  // Longer than the network timeout: a stop must not leave two players alive.
  static constexpr int kStopTimeoutMs = 20000;

  void begin(const Config& config);
  void requestStop();
  // Release the process-wide audio context. Call only after the player thread
  // has stopped; the video context is released per playback by closeStreams().
  static void releaseAudioContext();
  void setPaused(bool paused);
  bool paused() const { return paused_.load(); }
  double positionSeconds() const { return position_.load(); }
  QString lastError() const { return error_; }
  // Producer reached the end of the playlist without an error. The player may
  // still be draining its video/audio queue, so this is not "connection lost".
  bool endOfStream() const;

 signals:
  void started();
  void positionChanged(double seconds);
  void failed(const QString& message);
  void finished();
  // No data for a while: the app shows "connection lost" without closing.
  void stalled(bool stalled);
  void statsChanged(const QVariantMap& stats);

 protected:
  void run() override;

 private:
  bool fetch(QNetworkAccessManager& nam, const QString& url, QByteArray* out,
             qint64 maxBytes, QString* finalUrl = nullptr);
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
  void setError(const QString& text);
  QString errorText() const;
  void setProducerError(const QString& text);
  bool hasProducerError() const;
  QString producerErrorText() const;

  Config cfg_;
  QUrl allowedOrigin_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> paused_{false};
  std::atomic<double> position_{0.0};
  mutable std::mutex stateMutex_;
  QString error_;

  QNetworkAccessManager* namRef_ = nullptr;

  // HLS
  QString playlistUrl_;
  QStringList segmentUrls_;
  QString lastSegmentUrl_;
  std::atomic<int> segmentCount_{0};
  std::atomic<int> lastSegmentIndex_{-1};

  // queue of segments downloaded by a separate thread: the network must never
  // block frame presentation
  std::deque<QByteArray> segmentQueue_;
  size_t segmentQueueBytes_ = 0;
  std::mutex queueMutex_;
  std::condition_variable queueCv_;
  class ProducerThread;
  ProducerThread* producer_ = nullptr;
  std::atomic<bool> producerFinished_{false};
  // Diagnostic build only: which phase the network thread is in.
  std::atomic<int> producerState_{0};
  QElapsedTimer lastData_;
  bool stalled_ = false;
  QString producerError_;
  int nextSegment_ = 0;
  // The server does not apply StartTimeTicks to the playlist: starting at a
  // position is done by starting from the segment covering that time.
  bool startApplied_ = false;
  qint64 startOffsetTicks_ = 0;
  bool playlistEnded_ = false;

  // demux
  TsDemux* demux_ = nullptr;

  // video
  std::vector<uint8_t> auBuf_;
  std::vector<std::pair<size_t, int64_t>> auPts_;
  // incremental scanning of access units (avoids O(n^2) per segment)
  size_t auScanPos_ = 0;
  qint64 auLastStartCode_ = -1;
  bool auHasVcl_ = false;
  // demux in chunks: feeding a whole segment blocks the player for 100-300 ms
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
