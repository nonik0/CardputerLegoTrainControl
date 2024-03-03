#include "Ultrasonic.h"

void Ultrasonic::begin(uint8_t trig, uint8_t echo)
{
    _trig = trig;
    _echo = echo;
    pinMode(_trig, OUTPUT);
    pinMode(_echo, INPUT);
}

float Ultrasonic::getDuration()
{
    digitalWrite(_trig, LOW);
    delayMicroseconds(2);
    digitalWrite(_trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(_trig, LOW);
    return pulseIn(_echo, HIGH);
}

float Ultrasonic::getDistance()
{
    float distance = getDuration() * 0.34 / 2;
    return distance > 4500.00 ? 4500.00 : distance;
}