#ifndef TSDEMUX_H
#define TSDEMUX_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// Minimal MPEG-TS demuxer: extracts H.264 (Annex-B) and AAC (ADTS) from an HLS
// segment. It does not use ffmpeg for demuxing: PAT/PMT/PES are just a few header
// bytes, and keeping them here avoids shipping libavformat on the device.
class TsDemux {
 public:
  using PacketCallback =
      std::function<void(const uint8_t* data, size_t size, int64_t pts90k)>;

  void setVideoCallback(PacketCallback cb) { videoCb_ = std::move(cb); }
  void setAudioCallback(PacketCallback cb) { audioCb_ = std::move(cb); }

  void feed(const uint8_t* data, size_t size);
  void flush();

  int videoPid() const { return videoPid_; }
  int audioPid() const { return audioPid_; }

 private:
  void parsePat(const uint8_t* section, size_t size);
  void parsePmt(const uint8_t* section, size_t size);
  void onPesStart(int pid);
  void emitPes(int pid);

  PacketCallback videoCb_;
  PacketCallback audioCb_;

  int pmtPid_ = -1;
  int videoPid_ = -1;
  int audioPid_ = -1;

  std::vector<uint8_t> videoPes_;
  std::vector<uint8_t> audioPes_;
  bool videoOpen_ = false;
  bool audioOpen_ = false;
};

#endif  // TSDEMUX_H
