#ifndef QUALITY_H
#define QUALITY_H

#include <cstddef>

// Single source of truth for the bitrate choices shown in Settings and in the
// player menu. Values are kbps.
constexpr int kQualityKbps[] = {4000,  8000,  12000, 20000, 30000,
                                40000, 50000, 60000, 80000, 100000};
constexpr int kQualityCount =
    static_cast<int>(sizeof(kQualityKbps) / sizeof(kQualityKbps[0]));

inline int QualityIndex(int kbps) {
  for (int i = 0; i < kQualityCount; ++i) {
    if (kQualityKbps[i] == kbps) return i;
  }
  return 2;  // 12 Mbps default
}

inline bool IsValidQuality(int kbps) {
  for (int i = 0; i < kQualityCount; ++i) {
    if (kQualityKbps[i] == kbps) return true;
  }
  return false;
}

inline int ShiftQuality(int current, int delta) {
  int index = QualityIndex(current);
  index = (index + delta + kQualityCount) % kQualityCount;
  return kQualityKbps[index];
}

#endif  // QUALITY_H
