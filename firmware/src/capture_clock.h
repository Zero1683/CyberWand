#pragma once
#include <cstdint>
namespace wand {
// Captures last seconds, never >2^31 ms. A timestamp slightly in the future
// (loop snapshot taken before startCapture) is zero elapsed, not 49 days.
inline uint32_t captureElapsed(uint32_t now,uint32_t start){int32_t delta=int32_t(now-start);return delta>0?uint32_t(delta):0;}
inline bool captureExpired(uint32_t now,uint32_t start,uint32_t lastSample){return captureElapsed(now,start)>4000||captureElapsed(now,lastSample)>100;}
}
