#include "../SIGNAL_OS/SignalOS_TrailEcho.h"
#include <assert.h>
#include <stdio.h>

static bool still(float& x,float& y,float& z){x=0;y=0;z=1;return true;}
static bool rough(float& x,float& y,float& z){static int i=0; x=0; y=(i++ & 1)?0.28f:-0.28f;z=1;return true;}

int main(){
  trailecho::Mode m;
  assert(m.begin());
  assert(!m.recording());
  assert(m.stored());
  m.shortA(0);
  assert(m.recording());
  // Recording a rough 2.1-second trajectory yields two bins and a saved file.
  for(uint32_t t=0;t<=2100;t+=20)m.update(t,rough);
  m.shortA(2120);
  assert(!m.recording());
  assert(m.data().count()==3); // 2 complete + partial second
  assert(SPIFFS.exists("/te-last.bin"));
  assert(SPIFFS.files["/te-last.bin"].size()==8+3*4);
  m.shortB();assert(m.page()==1);
  m.shortB();assert(m.page()==0);
  // Reboot restores the previous saved ride.
  trailecho::Mode restored;
  assert(restored.begin());
  assert(restored.data().count()==3);
  // A zero-duration recording must preserve the older complete ride.
  restored.shortA(3000);
  restored.shortA(3000);
  assert(SPIFFS.exists("/te-last.bin"));
  trailecho::Mode secondReboot;
  assert(secondReboot.begin());
  assert(secondReboot.data().count()==3);
  // Hold A while recording should finalize and retain data.
  secondReboot.shortA(6000);
  for(uint32_t t=6000;t<7200;t+=20)secondReboot.update(t,still);
  secondReboot.exit(7300);
  assert(!secondReboot.recording());
  assert(SPIFFS.exists("/te-last.bin"));
  // When flash is unavailable mode keeps a RAM-only ride.
  SPIFFS.working=false;
  trailecho::Mode ram;
  assert(!ram.begin());
  ram.shortA(0);
  for(uint32_t t=0;t<=1060;t+=20)ram.update(t,still);
  ram.shortA(1080);
  assert(ram.data().count()>=1 && !ram.stored());
  printf("PASS: Trail Echo mode (persist, reboot, short recording, hold exit, RAM fallback)\n");
}
