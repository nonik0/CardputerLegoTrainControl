#pragma once

#include <Arduino.h>

class Ultrasonic {
   private:
    uint8_t _trig;
    uint8_t _echo;

   public:
    void begin(uint8_t trig = 26, uint8_t echo = 36);
    float getDuration();
    float getDistance();
};
