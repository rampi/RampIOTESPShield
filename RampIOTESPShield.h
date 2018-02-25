#ifndef RampIOTESPShield_H
#define RampIOTESPShield_H

#include "Arduino.h"
#include "ArduinoJson.h"
#include "Storage.h"
#include "RampIOTESPShieldConfiguration.h"

class RampIOTESPShield{
    public:
    RampIOTESPShield();
    void begin();
    void handleShield();
};

#endif