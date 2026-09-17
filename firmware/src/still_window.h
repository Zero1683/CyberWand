#pragma once
#include <cmath>
#include <cstdint>
namespace wand {
// Determine settled motion from raw acceleration stability, not gravity residual.
struct StillWindow {
  bool tracking=false; uint32_t since=0; float anchor[3]={};
  void reset(){tracking=false;}
  bool update(uint32_t now,float speed,const float* a){
    float norm=0,delta=0;
    for(int i=0;i<3;i++){norm+=a[i]*a[i];float d=a[i]-anchor[i];delta+=d*d;}
    if(speed>=8 || norm<0.85f*0.85f || norm>1.15f*1.15f){tracking=false;return false;}
    if(!tracking || delta>0.025f*0.025f){tracking=true;since=now;for(int i=0;i<3;i++)anchor[i]=a[i];return false;}
    return now-since>=200;
  }
};
}
