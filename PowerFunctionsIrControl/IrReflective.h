#pragma once

#include <Arduino.h>

class IrReflective
{
private:
    uint8_t _pin;

public:
    void begin(uint8_t pin)
    {
        _pin = pin;
        pinMode(_pin, INPUT);
    }

    bool isDetected()
    {
        bool detected = digitalRead(_pin) == 0;
        if (detected) {
            log_d("Detected");
        }
        return detected;
    }
};