#include "../SIGNAL_OS/TrailBridgeWire.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace trailbridge;
static void replayTests() {
  using namespace trailbridge;
  Replay a = {0x0badf00d, 3, 5, 812, 140, 2, 0x11223344}, b = {};
  uint8_t p[kPacketSize];
  encodeReplay(a, p);
  assert(p[3] == kReplay);
  assert(decodeReplay(p, kPacketSize, b));
  assert(a.rideId == b.rideId && a.index == b.index && a.total == b.total && a.rmsMg == b.rmsMg &&
         a.peak10Mg == b.peak10Mg && a.impacts == b.impacts && a.sendId == b.sendId);
  Motion m = {};
  assert(!decode(p, kPacketSize, m));  // older receivers ignore replays
  p[13] ^= 0x40; assert(!decodeReplay(p, kPacketSize, b)); p[13] ^= 0x40;
  Replay bad = a; bad.index = 5; encodeReplay(bad, p); assert(!decodeReplay(p, kPacketSize, b));
  bad = a; bad.sendId = 0; encodeReplay(bad, p); assert(!decodeReplay(p, kPacketSize, b));
  bad = a; bad.total = kMaxReplayBins + 1; bad.index = 0; encodeReplay(bad, p); assert(!decodeReplay(p, kPacketSize, b));
  bad = a; bad.rideId = 0; encodeReplay(bad, p); assert(decodeReplay(p, kPacketSize, b));  // unknown ride is allowed

  static ReplayCollector c;
  for (uint16_t i = 0; i < 5; ++i) {
    if (i == 2) continue;  // lost on the first pass
    Replay r = a; r.index = i; r.rmsMg = uint16_t(100 + i);
    assert(c.add(r));
  }
  assert(c.received() == 4 && c.missing() == 1 && !c.complete() && !c.has(2));
  Replay again = a; again.index = 1; assert(!c.add(again));  // second pass repeat
  again.index = 2; again.rmsMg = 102; assert(c.add(again));
  assert(c.complete() && c.rmsMg(2) == 102 && c.rmsMg(4) == 104 && c.rideId() == a.rideId);
  Replay other = a; other.sendId = 9;
  assert(c.startsNewTransfer(other) && !c.add(other));
  c.clear(); assert(!c.active() && c.add(other) && c.sendId() == 9);
  puts("PASS replay packet / collector");
}

int main(){
  static_assert(kPacketSize==22,"wire format");
  Motion a={0x12345678,65535,254,1420,255,4,0xdeadbeef}, b={};
  uint8_t p[kPacketSize];encode(a,p);
  assert(p[0]=='T'&&p[1]=='E'&&p[4]==0x78&&p[5]==0x56&&p[20]!=(uint8_t)0);
  assert(decode(p,kPacketSize,b));
  assert(memcmp(&a,&b,sizeof(a))==0);
  assert(!decode(p,kPacketSize-1,b));
  p[3]=77;assert(!decode(p,kPacketSize,b));p[3]=kMotion;
  p[12]^=0xff;assert(!decode(p,kPacketSize,b));p[12]^=0xff;
  Receiver rx;
  assert(rx.accept(a));assert(!rx.accept(a));
  assert(rx.duplicates()==1);
  Motion c=a;c.sequence=0;assert(rx.accept(c)); // sequence wrap
  c.sequence=3;assert(rx.accept(c));assert(rx.gaps()==2);
  c.rideId++;assert(rx.accept(c));
  c.rideId=0;encode(c,p);assert(!decode(p,kPacketSize,b));
  puts("PASS trail bridge protocol / crc / ordering / rollover");
  replayTests();
}
