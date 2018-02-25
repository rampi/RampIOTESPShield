#include "RampIOTESPShield.h"

RampIOTESPShield shield;

void setup() {
  shield.begin();
}

void loop() {
  shield.handleShield();
}