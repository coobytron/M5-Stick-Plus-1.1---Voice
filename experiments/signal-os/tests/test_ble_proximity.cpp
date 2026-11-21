#include "../SIGNAL_OS/BLEProximityCore.h"
#include <assert.h>
#include <stdio.h>
using namespace bleprox;
int main(){
  Monitor m;
  assert(m.state(30000)==State::NoTarget);
  m.select("AA:BB:CC:DD:EE:FF","TEST BEACON",1000);
  assert(m.selected()&&m.state(1001)==State::Searching);
  assert(!m.observe("11:22:33:44:55:66",-48,1100));
  assert(!m.observe("AA:BB:CC:DD:EE:FF",-127,1100));
  assert(m.observe("AA:BB:CC:DD:EE:FF",-60,1200));
  assert(m.state(1200)==State::Near && m.rssi()==-60);
  assert(!m.observe("AA:BB:CC:DD:EE:FF",-30,1200));
  assert(!m.observe("AA:BB:CC:DD:EE:FF",-30,1100));
  m.tick(1700);
  assert(m.points()==1 && m.point(0)==-60);
  m.cycleThreshold();assert(m.threshold()==-75);
  m.cycleThreshold();assert(m.threshold()==-55);
  assert(m.state(1700)==State::Mid);
  assert(m.state(1200+3501)==State::Stale);
  m.tick(1200+4000);
  assert(m.point(m.points()-1)==kNoValue);
  assert(m.state(1200+12001)==State::Missing);
  assert(m.observe("AA:BB:CC:DD:EE:FF",-47,14000));
  assert(m.fresh(14000));
  m.select("BB:CC:DD:EE:FF:00","OTHER",15000);
  assert(m.state(15000)==State::Searching && m.points()==0);
  assert(!m.observe("AA:BB:CC:DD:EE:FF",-50,15100));
  m.select("AA:BB:CC:DD:EE:FF","WRAP",0xfffffff0u);
  assert(m.observe("AA:BB:CC:DD:EE:FF",-70,0xfffffff8u));
  assert(m.state(0x00000020u)==State::Mid);
  assert(m.state(0x00002000u)==State::Stale);
  m.select(nullptr,nullptr,42);
  assert(m.state(43)==State::NoTarget);
  puts("PASS BLE proximity pinning, thresholds, stale gaps, rollover");
}
