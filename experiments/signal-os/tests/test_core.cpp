#include "../SIGNAL_OS/TrailEchoCore.h"
#include <assert.h>
#include <stdio.h>

int main() {
  using namespace trailecho;
  Core c;
  assert(c.count() == 0 && !c.recording());
  c.start(1000);
  for (int i = 0; i < 150; ++i) {
    float vibration = (i % 2) ? 0.12f : -0.12f;
    c.sample(0, vibration, 1, 1000 + i*20);
  }
  c.stop(4050);
  assert(c.count() == 3);
  assert(c.averageRmsMg() > 50 && c.maxRmsMg() < 300);
  assert(!c.recording());
  // A static still unit should settle close to zero.
  c.start(0);
  for(int i=0;i<200;i++) c.sample(0,0,1, i*20);
  c.stop(4010);
  assert(c.maxRmsMg() == 0);
  // A distinct spike is counted once, not for each adjacent >threshold sample.
  c.start(0);
  for(int i=0;i<100;i++) c.sample((i==20 || i==21) ? 3.0f : 0,0,1, i*20);
  c.stop(2000);
  assert(c.totalImpacts() == 1);
  // Another distinct shock 300ms later counts twice.
  c.start(0);
  for(int i=0;i<100;i++) c.sample((i==20 || i==40) ? 3.0f : 0,0,1, i*20);
  c.stop(2000);
  assert(c.totalImpacts() == 2);
  // Fixed memory capacity, no overflow.
  c.start(0);
  for(uint32_t sec = 1; sec <= Core::kCapacity + 10; ++sec) c.advance(sec * 1000);
  assert(c.count() == Core::kCapacity && !c.recording());
  // Deserialize records through load path.
  c.reset();
  assert(c.addLoadedBin({150, 20, 2}));
  assert(c.count() == 1 && c.totalImpacts() == 2);
  printf("PASS: Trail Echo core (signal, static, impacts, capacity, load)\n");
}
