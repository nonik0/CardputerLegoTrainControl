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
        return digitalRead(_pin) == 0;
    }
};