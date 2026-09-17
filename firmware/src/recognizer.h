#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

// Original six-axis, time-warped personal template recognizer.
namespace wand {
constexpr int STEPS=48, AXES=6, CLASSES=12, EXAMPLES=3, MAX_SAMPLES=440;
static const char* const names[CLASSES]={"left","right","up","down","circle","zigzag","custom1","custom2","custom3","custom4","custom5","custom6"};
struct Frame { float v[AXES]; };
struct Pattern { int16_t v[STEPS][AXES]; };
struct Result { int label=-1; float distance=100, runnerUp=100, margin=0; };
// Resample in time; preserve signs and relative channel weights, normalize amplitude.
inline bool normalize(const Frame* input,int count,Pattern& out) {
  if(count<12 || count>MAX_SAMPLES) return false;
  Frame points[STEPS]; float power=0;
  for(int i=0;i<STEPS;i++) {
    float t=float(i)*(count-1)/(STEPS-1); int a=int(t), b=std::min(a+1,count-1);
    for(int j=0;j<AXES;j++) {
      float x=input[a].v[j]+(input[b].v[j]-input[a].v[j])*(t-a);
      if(!std::isfinite(x)) return false;
      points[i].v[j]=x; power+=x*x;
    }
  }
  float rms=std::sqrt(power/(STEPS*AXES));
  if(rms<0.015f) return false;
  for(int i=0;i<STEPS;i++) for(int j=0;j<AXES;j++)
    out.v[i][j]=int16_t(std::max(-12.f,std::min(12.f,points[i].v[j]/rms))*1000);
  return true;
}
inline float distance(const Pattern& a,const Pattern& b) {
  // Sakoe-Chiba band limits unrealistic time distortions; rolling rows bound RAM.
  float prev[STEPS+1],row[STEPS+1]; std::fill(prev,prev+STEPS+1,1e9f);prev[0]=0;
  for(int i=1;i<=STEPS;i++) {
    std::fill(row,row+STEPS+1,1e9f);
    for(int j=std::max(1,i-8);j<=std::min(STEPS,i+8);j++) {
      float d=0;
      for(int k=0;k<AXES;k++) {float e=(a.v[i-1][k]-b.v[j-1][k])/1000.f;d+=e*e;}
      row[j]=d/AXES+std::min(prev[j-1],std::min(prev[j],row[j-1]));
    }
    std::copy(row,row+STEPS+1,prev);
  }
  return std::sqrt(prev[STEPS]/STEPS);
}
inline Pattern activePart(const Pattern& p) {
  float energy[STEPS]={},peak=0;
  for(int i=0;i<STEPS;i++){for(int k=0;k<AXES;k++){float x=p.v[i][k]/1000.f;energy[i]+=x*x;}peak=std::max(peak,energy[i]);}
  if(peak<0.001f)return p;
  int first=0,last=STEPS-1;
  while(first<last&&energy[first]<peak*0.04f)first++;
  while(last>first&&energy[last]<peak*0.04f)last--;
  first=std::max(0,first-2);last=std::min(STEPS-1,last+2);
  if(last-first+1<12)return p;
  Frame input[STEPS];for(int i=first;i<=last;i++)for(int k=0;k<AXES;k++)input[i-first].v[k]=p.v[i][k]/1000.f;
  Pattern out;if(!normalize(input,last-first+1,out))return p;return out;
}
struct Library {
  Pattern patterns[CLASSES][EXAMPLES]; uint8_t counts[CLASSES]={};
  Result classify(const Pattern& p) const {
    Result r; int available=0;Pattern query=activePart(p);
    for(int c=0;c<CLASSES;c++) {
      if(!counts[c]) continue; available++;
      float best=100;
      for(int k=0;k<counts[c] && k<EXAMPLES;k++) best=std::min(best,distance(query,activePart(patterns[c][k])));
      if(best<r.distance) {r.runnerUp=r.distance;r.distance=best;r.label=c;}
      else r.runnerUp=std::min(r.runnerUp,best);
    }
    r.margin=r.runnerUp-r.distance;
    // These are provisional gates, not a calibrated probability or accuracy claim.
    if(available<2 || r.distance>0.65f || r.margin<0.12f) r.label=-1;
    return r;
  }
};
}
