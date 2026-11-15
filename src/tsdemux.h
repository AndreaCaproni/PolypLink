#ifndef TSDEMUX_H
#define TSDEMUX_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// Demuxer MPEG-TS minimale: estrae H.264 (Annex-B) e AAC (ADTS) da un segmento
// HLS. Non usa ffmpeg per il demux: PAT/PMT/PES sono pochi byte di header e
// tenerli qui evita di portare libavformat sul dispositivo.
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
