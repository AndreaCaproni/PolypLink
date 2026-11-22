#include "tsdemux.h"

#include <cstring>

namespace {

int64_t parsePts(const uint8_t* p) {
  return (static_cast<int64_t>(p[0] & 0x0E) << 29) | (static_cast<int64_t>(p[1]) << 22) |
         (static_cast<int64_t>(p[2] & 0xFE) << 14) | (static_cast<int64_t>(p[3]) << 7) |
         (static_cast<int64_t>(p[4] & 0xFE) >> 1);
}

}  // namespace

void TsDemux::feed(const uint8_t* data, size_t size) {
  size_t offset = 0;
  while (offset + 188 <= size) {
    const uint8_t* pkt = data + offset;
    if (pkt[0] != 0x47) {
      ++offset;  // resynchronize
      continue;
    }
    offset += 188;

    const bool pusi = (pkt[1] & 0x40) != 0;
    const int pid = ((pkt[1] & 0x1F) << 8) | pkt[2];
    const int afc = (pkt[3] >> 4) & 0x03;

    size_t payloadOffset = 4;
    if (afc == 0 || afc == 2) continue;  // no payload
    if (afc == 3) {
      const size_t afLen = pkt[4];
      payloadOffset = 5 + afLen;
      if (payloadOffset >= 188) continue;
    }
    const uint8_t* payload = pkt + payloadOffset;
    const size_t payloadSize = 188 - payloadOffset;

    if (pid == 0) {
      if (pusi && payloadSize > 1) {
        const uint8_t pointer = payload[0];
        if (pointer + 1 < payloadSize) parsePat(payload + 1 + pointer, payloadSize - 1 - pointer);
      }
      continue;
    }

    if (pid == pmtPid_) {
      if (pusi && payloadSize > 1) {
        const uint8_t pointer = payload[0];
        if (pointer + 1 < payloadSize) parsePmt(payload + 1 + pointer, payloadSize - 1 - pointer);
      }
      continue;
    }

    if (pid == videoPid_ || pid == audioPid_) {
      std::vector<uint8_t>& pes = (pid == videoPid_) ? videoPes_ : audioPes_;
      bool& open = (pid == videoPid_) ? videoOpen_ : audioOpen_;

      if (pusi) {
        if (open) emitPes(pid);
        pes.clear();
        open = true;
      }
      if (!open) continue;
      pes.insert(pes.end(), payload, payload + payloadSize);
    }
  }
}

void TsDemux::flush() {
  if (videoOpen_) emitPes(videoPid_);
  if (audioOpen_) emitPes(audioPid_);
  videoPes_.clear();
  audioPes_.clear();
  videoOpen_ = false;
  audioOpen_ = false;
}

void TsDemux::parsePat(const uint8_t* section, size_t size) {
  if (size < 12 || section[0] != 0x00) return;
  const size_t sectionLength = ((section[1] & 0x0F) << 8) | section[2];
  if (sectionLength + 3 > size) return;

  const uint8_t* p = section + 8;
  const uint8_t* end = section + 3 + sectionLength - 4;  // without CRC
  while (p + 4 <= end) {
    const int program = (p[0] << 8) | p[1];
    const int pid = ((p[2] & 0x1F) << 8) | p[3];
    if (program != 0) {
      pmtPid_ = pid;
      return;
    }
    p += 4;
  }
}

void TsDemux::parsePmt(const uint8_t* section, size_t size) {
  if (size < 12 || section[0] != 0x02) return;
  const size_t sectionLength = ((section[1] & 0x0F) << 8) | section[2];
  if (sectionLength + 3 > size) return;

  const size_t programInfoLength = ((section[10] & 0x0F) << 8) | section[11];
  const uint8_t* p = section + 12 + programInfoLength;
  const uint8_t* end = section + 3 + sectionLength - 4;

  while (p + 5 <= end) {
    const int streamType = p[0];
    const int pid = ((p[1] & 0x1F) << 8) | p[2];
    const size_t esInfoLength = ((p[3] & 0x0F) << 8) | p[4];

    if (streamType == 0x1B && videoPid_ < 0) videoPid_ = pid;        // H.264
    else if (streamType == 0x0F && audioPid_ < 0) audioPid_ = pid;   // AAC ADTS

    p += 5 + esInfoLength;
  }
}

void TsDemux::emitPes(int pid) {
  std::vector<uint8_t>& pes = (pid == videoPid_) ? videoPes_ : audioPes_;
  if (pes.size() < 9) return;
  if (pes[0] != 0x00 || pes[1] != 0x00 || pes[2] != 0x01) return;

  const uint8_t flags = pes[7];
  const size_t headerLength = 9 + pes[8];
  if (headerLength > pes.size()) return;

  // The PTS flag promises 5 bytes starting at pes[9]: without this check
  // a truncated PES (small header_data_length but flag set) would read
  // past the buffer. With PTS+DTS the minimum header is 19 bytes.
  if ((flags & 0x80) && headerLength < 14) return;
  if ((flags & 0x40) && headerLength < 19) return;

  int64_t pts = -1;
  if (flags & 0x80) pts = parsePts(&pes[9]);

  const uint8_t* payload = pes.data() + headerLength;
  const size_t payloadSize = pes.size() - headerLength;
  if (payloadSize == 0) return;

  if (pid == videoPid_ && videoCb_) videoCb_(payload, payloadSize, pts);
  else if (pid == audioPid_ && audioCb_) audioCb_(payload, payloadSize, pts);
}
