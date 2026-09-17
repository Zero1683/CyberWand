#include "../src/motion_gate.h"
#include <cassert>
#include <cstdio>
int main(){
  wand::MotionGate g;g.reset(0);
  for(unsigned t=0;t<1000;t+=10)assert(g.update(t,2,.02f)==g.None);
  assert(g.armed);
  assert(g.update(1000,50,.2f)==g.None);assert(g.update(1020,2,.02f)==g.None);
  assert(g.update(1100,60,.2f)==g.None);assert(g.update(1140,60,.2f)==g.Start);
  assert(g.update(1500,2,.02f)==g.None);assert(g.update(1650,60,.2f)==g.None);
  assert(g.update(1700,2,.02f)==g.None);assert(g.update(1920,2,.02f)==g.Stop);
  for(unsigned t=1930;t<2600;t+=10)assert(g.update(t,60,.2f)==g.None);
  assert(!g.armed); // Must settle again after the cooldown; continuous movement cannot retrigger.
  for(unsigned t=2600;t<3000;t+=10)g.update(t,2,.02f);
  assert(g.armed);g.update(3000,0,.3f);assert(g.update(3040,0,.3f)==g.Start);
  g.reset(0xFFFFFF00u);g.update(0xFFFFFF00u,0,0);g.update(500,0,0);assert(g.armed);
  puts("PASS: stationary, impulse reject, onset, pause hysteresis, end, cooldown, accel trigger, timer wrap");
}
