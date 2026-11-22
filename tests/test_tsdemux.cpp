// TS demuxer test: verifies that a truncated PES does not cause the PTS to be
// read past the buffer and that malformed PES packets never reach the decoder.
#include "tsdemux.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++g_failures;
  }
}

// 188-byte TS packet with exactly the given payload. If it is shorter than
// 184 bytes it uses an adaptation field, so we can build truncated PES packets
// at will.
std::vector<uint8_t> tsPacket(int pid, bool pusi,
                              const std::vector<uint8_t>& payload) {
  std::vector<uint8_t> pkt(188, 0xFF);
  pkt[0] = 0x47;
  pkt[1] = static_cast<uint8_t>((pusi ? 0x40 : 0x00) | ((pid >> 8) & 0x1F));
  pkt[2] = static_cast<uint8_t>(pid & 0xFF);

  if (payload.size() == 184) {
    pkt[3] = 0x10;  // payload only
    std::memcpy(pkt.data() + 4, payload.data(), payload.size());
  } else {
    const size_t afLen = 183 - payload.size();
    pkt[3] = 0x30;  // adaptation field + payload
    pkt[4] = static_cast<uint8_t>(afLen);
    if (afLen > 0) pkt[5] = 0x00;  // flag adaptation field
    std::memcpy(pkt.data() + 5 + afLen, payload.data(), payload.size());
  }
  return pkt;
}

std::vector<uint8_t> pat() {
  std::vector<uint8_t> section(16, 0);
  section[0] = 0x00;              // table_id PAT
  section[1] = 0x00;              // section_length = 13
  section[2] = 0x0D;
  section[3] = 0x00;              // transport_stream_id
  section[4] = 0x01;
  section[5] = 0xC1;              // version/current
  section[8] = 0x00;              // program_number = 1
  section[9] = 0x01;
  section[10] = 0xE1;             // PMT PID = 0x100
  section[11] = 0x00;

  std::vector<uint8_t> payload;
  payload.push_back(0x00);        // pointer_field
  payload.insert(payload.end(), section.begin(), section.end());
  return payload;
}

std::vector<uint8_t> pmt() {
  std::vector<uint8_t> section(21, 0);
  section[0] = 0x02;              // table_id PMT
  section[1] = 0x00;              // section_length = 18
  section[2] = 0x12;
  section[3] = 0x00;              // program_number = 1
  section[4] = 0x01;
  section[5] = 0xC1;              // version/current
  section[8] = 0xE1;              // PCR PID = 0x101
  section[9] = 0x01;
  section[10] = 0xF0;             // program_info_length = 0
  section[11] = 0x00;
  section[12] = 0x1B;             // H.264
  section[13] = 0xE1;             // elementary PID = 0x101
  section[14] = 0x01;
  section[15] = 0xF0;             // ES_info_length = 0
  section[16] = 0x00;

  std::vector<uint8_t> payload;
  payload.push_back(0x00);        // pointer_field
  payload.insert(payload.end(), section.begin(), section.end());
  return payload;
}

void feedTables(TsDemux& demux) {
  const auto patPacket = tsPacket(0, true, pat());
  demux.feed(patPacket.data(), patPacket.size());
  const auto pmtPacket = tsPacket(0x100, true, pmt());
  demux.feed(pmtPacket.data(), pmtPacket.size());
}

struct Sink {
  int videoCalls = 0;
  int audioCalls = 0;
  size_t lastSize = 0;
  int64_t lastPts = -1;

  void attach(TsDemux& demux) {
    demux.setVideoCallback([this](const uint8_t*, size_t size, int64_t pts) {
      ++videoCalls;
      lastSize = size;
      lastPts = pts;
    });
    demux.setAudioCallback([this](const uint8_t*, size_t, int64_t) {
      ++audioCalls;
    });
  }
};

// Valid PES: PTS = 90000 and 5 bytes of payload.
std::vector<uint8_t> validPes() {
  std::vector<uint8_t> pes = {0x00, 0x00, 0x01, 0xE0,  // start code + video
                              0x00, 0x0D,              // packet_length
                              0x80, 0x80, 0x05};       // PTS present, header 5
  const std::vector<uint8_t> pts = {0x21, 0x00, 0x05, 0xBF, 0x21};
  pes.insert(pes.end(), pts.begin(), pts.end());
  pes.insert(pes.end(), {0x00, 0x00, 0x00, 0x01, 0x65});
  return pes;
}

// Truncated PES: 9 bytes in total, header_data_length = 0 but PTS flag set.
// The old code read pes[9..13] past the end of the buffer.
std::vector<uint8_t> truncatedPes() {
  return {0x00, 0x00, 0x01, 0xE0, 0x00, 0x00, 0x80, 0x80, 0x00};
}

// PES with header_data_length = 0 and 5 bytes of payload: without the minimum
// header check, the payload bytes would be read as the PTS and delivered to the
// decoder as if they were the correct PES.
std::vector<uint8_t> shortHeaderPes() {
  std::vector<uint8_t> pes = {0x00, 0x00, 0x01, 0xE0, 0x00, 0x08,
                              0x80, 0x80, 0x00};
  pes.insert(pes.end(), {0x21, 0x00, 0x05, 0xBF, 0x21});
  return pes;
}

void feedPes(TsDemux& demux, const std::vector<uint8_t>& pes) {
  const auto packet = tsPacket(0x101, true, pes);
  demux.feed(packet.data(), packet.size());
  demux.flush();
}

void testValidPes() {
  TsDemux demux;
  Sink sink;
  sink.attach(demux);
  feedTables(demux);
  feedPes(demux, validPes());

  check(demux.videoPid() == 0x101, "PMT: video PID individuato");
  check(sink.videoCalls == 1, "PES valido: una chiamata video");
  check(sink.lastSize == 5, "PES valido: payload di 5 byte");
  check(sink.lastPts == 90000, "PES valido: PTS 90000");
  check(sink.audioCalls == 0, "PES valido: nessuna chiamata audio");
}

void testTruncatedPes() {
  TsDemux demux;
  Sink sink;
  sink.attach(demux);
  feedTables(demux);
  feedPes(demux, truncatedPes());

  check(sink.videoCalls == 0, "PES troncato: scartato senza callback");
}

void testShortHeaderPes() {
  TsDemux demux;
  Sink sink;
  sink.attach(demux);
  feedTables(demux);
  feedPes(demux, shortHeaderPes());

  check(sink.videoCalls == 0,
        "PES con header troppo corto: non letto come payload valido");
}

}  // namespace

int main() {
  testValidPes();
  testTruncatedPes();
  testShortHeaderPes();

  if (g_failures != 0) {
    std::fprintf(stderr, "tsdemux tests: %d falliti\n", g_failures);
    return 1;
  }
  std::printf("tsdemux tests: ok\n");
  return 0;
}
