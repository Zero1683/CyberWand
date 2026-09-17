#include "../src/capture_clock.h"
#include <cassert>
#include <cstdio>
int main(){
 assert(!wand::captureExpired(1000,1001,1000)); // Actual 0-sample rejection regression.
 assert(!wand::captureExpired(1000,1000,1001));
 assert(wand::captureExpired(5002,1000,5000));
 assert(wand::captureExpired(2000,1500,1899));
 assert(wand::captureElapsed(50,0xfffffff0u)==66);
 puts("PASS: stale loop timestamp, future sample, true timeout, stalled IMU, millis wrap");
}
