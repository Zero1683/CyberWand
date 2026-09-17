#pragma once
#include <cstdint>
namespace wand {
// All timers use elapsed unsigned milliseconds, including across millis wrap.
struct MotionGate {
  enum Event {None,Start,Stop};
  bool armed=false,active=false,quiet=false,onset=false;
  uint32_t quietAt=0,onsetAt=0,endedAt=0;
  void reset(uint32_t now){armed=active=quiet=onset=false;endedAt=now;}
  Event update(uint32_t now,float speed,float acceleration) {
    bool low=speed<12 && acceleration<0.09f;
    bool high=speed>25 || acceleration>0.16f;
    if(low){if(!quiet){quiet=true;quietAt=now;}}else quiet=false;
    if(active){if(quiet&&now-quietAt>=220){reset(now);return Stop;}return None;}
    if(!armed){if(quiet&&now-quietAt>=300&&now-endedAt>=650)armed=true;return None;}
    if(high){if(!onset){onset=true;onsetAt=now;}if(now-onsetAt>=40){active=true;onset=false;return Start;}}
    else onset=false;
    return None;
  }
};
}
