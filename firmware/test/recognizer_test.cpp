#include "../src/recognizer.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace wand;
static Frame data[MAX_SAMPLES];
Pattern pulse(int axis,float sign,int n,float scale=1,float warp=1) {
  for(int i=0;i<n;i++) {
    float t=std::pow(float(i)/(n-1),warp);
    for(int k=0;k<AXES;k++)data[i].v[k]=0;
    data[i].v[axis]=sign*scale*std::sin(3.14159265f*t);
  }
  Pattern p;assert(normalize(data,n,p));return p;
}
int main() {
  Library lib;
  Pattern p;
  assert(!normalize(data,0,p));assert(!normalize(data,MAX_SAMPLES+1,p));
  for(auto& f:data)for(float& x:f.v)x=0;
  assert(!normalize(data,100,p));
  data[20].v[0]=std::numeric_limits<float>::quiet_NaN();assert(!normalize(data,48,p));
  for(int c=0;c<4;c++){lib.patterns[c][0]=pulse(c/2,c%2?-1:1,104);lib.counts[c]=1;}
  auto same=pulse(0,1,208,0.6f,1.2f);auto r=lib.classify(same);
  assert(r.label==0);assert(r.distance<0.3f);
  assert(lib.classify(pulse(0,-1,75,1.3f)).label==1);
  assert(lib.classify(pulse(1,1,150)).label==2);
  assert(lib.classify(pulse(2,1,150)).label==-1);
  // Auto capture includes pre-roll and a quiet tail; button training does not.
  for(auto& frame:data)for(float& x:frame.v)x=0;
  for(int i=40;i<140;i++)data[i].v[0]=std::sin(3.14159265f*(i-40)/99);
  Pattern padded;assert(normalize(data,200,padded));assert(lib.classify(padded).label==0);
  lib.patterns[1][0]=lib.patterns[0][0];assert(lib.classify(same).label==-1);
  Library custom;custom.patterns[11][0]=pulse(0,1,104);custom.counts[11]=1;custom.patterns[0][0]=pulse(0,-1,104);custom.counts[0]=1;assert(custom.classify(same).label==11);
  Library empty;assert(empty.classify(same).label==-1);
  empty.patterns[0][0]=same;empty.counts[0]=1;assert(empty.classify(same).label==-1);
  printf("PASS: duration/amplitude/time-warp variation, signs, unknown, ambiguity, empty/one-class library, invalid input\n");
}
