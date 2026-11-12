#include "../SIGNAL_OS/TrailBridgeWire.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace trailbridge;
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
}
