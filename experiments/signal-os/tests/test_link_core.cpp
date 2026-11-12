#include "../SIGNAL_OS/TrailLinkCore.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace traillink;

int main() {
  static_assert(kPacketBytes <= 32, "nRF24 payload limit is 32 bytes");
  uint8_t wire[kPacketBytes] = {};
  Packet msg{Kind::Ok, 0x1234abcd, 65535, 321, 73};
  assert(encode(msg, wire, sizeof wire));
  assert(wire[0]=='T' && wire[1]=='L' && wire[2]==1 && wire[3]==2);
  assert(wire[4]==0xcd && wire[5]==0xab && wire[6]==0x34 && wire[7]==0x12);
  Packet decoded;
  assert(decode(wire, sizeof wire, decoded));
  assert(decoded.sender==msg.sender && decoded.sequence==msg.sequence);
  assert(decoded.kind==Kind::Ok && decoded.batteryPercent==73);
  assert(!decode(wire, sizeof wire-1, decoded));
  wire[11]^=0xff;
  assert(!decode(wire, sizeof wire, decoded)); // accidental corruption caught
  wire[11]^=0xff;
  wire[2]=9; write16(wire+14,crc16(wire,14));
  assert(!decode(wire,sizeof wire,decoded)); // unsupported version
  wire[2]=1; wire[3]=255; write16(wire+14,crc16(wire,14));
  assert(!decode(wire,sizeof wire,decoded)); // invalid message
  wire[3]=1; wire[13]=1; write16(wire+14,crc16(wire,14));
  assert(!decode(wire,sizeof wire,decoded)); // future flags unsupported
  assert(!encode(Packet{Kind::Ping, 0, 1, 5, 100},wire,sizeof wire));
  assert(!encode(Packet{Kind::Ping, 1, 1, 5, 101},wire,sizeof wire));
  assert(encode(Packet{Kind::Ping, 1, 1, 5, 255},wire,sizeof wire));

  Tracker tracker(0x1234abcd);
  assert(!tracker.fresh(1000));
  assert(!tracker.receive(Packet{Kind::Ping,42,0,0,99},1000)); // wrong peer
  assert(tracker.receive(Packet{Kind::Ping,0x1234abcd,65535,0,99},1000));
  assert(tracker.fresh(31000));
  assert(!tracker.fresh(31001));
  assert(!tracker.receive(Packet{Kind::Ping,0x1234abcd,65535,0,99},2000)); // duplicate
  assert(tracker.receive(Packet{Kind::Wait,0x1234abcd,0,1,98},2100)); // rollover
  assert(!tracker.receive(Packet{Kind::Ok,0x1234abcd,65535,1,97},2200)); // older
  assert(tracker.receivedCount()==2 && tracker.last().kind==Kind::Wait);
  assert(tracker.receive(Packet{Kind::Ok,0x1234abcd,1,1,97},0xfffffff0));
  assert(tracker.fresh(0x00000010)); // millisecond clock rollover
  assert(tracker.ageMs(0x00000010)==32);
  assert(tracker.receive(Packet{Kind::Meet,0x1234abcd,0,1,96},0x00010000)); // after reboot and long silence
  assert(tracker.last().kind==Kind::Meet);
  puts("PASS: Trail Link protocol (packet/CRC/length/version, peer/filter/sequence, clock rollover)");
}
