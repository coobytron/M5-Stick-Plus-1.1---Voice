#include "../SIGNAL_OS/SignalOS_RFModel.h"
#include <assert.h>
#include <stdio.h>
int main(){
  using namespace signalrf;
  assert(centerKHz(0)==433200 && centerKHz(8)==434800);
  assert(strength(-130)==0 && strength(-110)==0 && strength(-55)==100);
  assert(strength(-80)>0 && strength(-80)<100);
  QuietWatch watch;
  assert(!watch.quiet(20000));
  watch.arm(true,100);
  assert(!watch.seen() && !watch.quiet(30000)); // no data != lost
  watch.feed(-95,150);
  assert(!watch.seen());
  watch.feed(-70,200);
  assert(watch.seen() && !watch.quiet(200+11999));
  assert(watch.quiet(200+12000));
  watch.feed(-60,200+12001);
  assert(!watch.quiet(200+12001));
  watch.arm(false,1000);
  assert(!watch.quiet(900000));
  watch.arm(true,0xfffffff0u);
  watch.feed(-70,0xfffffff5u);
  assert(watch.quiet(0xfffffff5u+12000u)); // millis wrap
  puts("PASS: RF 424 band model and RF quiet-watch state");
}
