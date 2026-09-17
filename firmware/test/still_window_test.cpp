#include "../src/still_window.h"
#include "../src/motion_gate.h"
#include <cassert>
#include <cstdio>
int main(){
 wand::StillWindow s;float a[3]={0,0,1};
 assert(!s.update(0,0,a));assert(!s.update(199,0,a));assert(s.update(200,0,a));
 assert(!s.update(210,20,a));assert(!s.update(220,0,a));
 a[0]=.1f;assert(!s.update(400,0,a));assert(s.update(600,0,a));
 s.reset();a[0]=0;assert(!s.update(0xffffff80u,0,a));assert(s.update(100,0,a));
 // A wrong gravity estimate must recover while physically stationary, then re-arm.
 s.reset();wand::MotionGate gate;gate.reset(0);float gx=.3f,gz=.95f;
 for(unsigned t=0;t<2000;t+=10){float n=std::sqrt(gx*gx+gz*gz);gx/=n;gz/=n;
 if(s.update(t,0,a)){gx+=.08f*(a[0]-gx);gz+=.08f*(a[2]-gz);}
 float e=std::sqrt(gx*gx+(1-gz)*(1-gz));gate.update(t,0,e);}
 assert(gate.armed);puts("PASS settled window, motion reset, wrap and gravity-drift rearming");
}
