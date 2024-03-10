#pragma once

#include <Arduino.h>

#include "..\IrBroadcast.h"
#include "IrReflective.hpp"
#include "Ultrasonic.h"

enum class TrainDirection
{
    Undetected,
    Forward, // towards the fork
    Reverse // merging into fork
};

enum class TrainPosition
{
    Undetected,
    Entering,
    Passing,
    Exiting
};

// called when train is first detected and when clearing the switch
typedef void (*TrainDetectionCallback)(TrainPosition position, TrainDirection direction);

class TrackSwitch
{
private:
    IrReflective _irSensor;
    Ultrasonic _usSensor;
    PowerFunctionsIrBroadcast _pfIrClient;
    PowerFunctionsPort _motorPort;
    bool _switchState;

    TrainDirection _direction;
    TrainPosition _position;
    unsigned long _lastDetection;
    TrainDetectionCallback _onEnterAndExit;

    bool readForkSensor();
public:
    unsigned long lastExitMillis = 0;

    void logState();
    void begin(Ultrasonic usSensor, IrReflective irSensor, PowerFunctionsIrBroadcast pfIrClient, PowerFunctionsPort motorPort, bool defaultState = false);
    void registerCallback(TrainDetectionCallback callback);
    void switchTrack();
    void switchTrack(bool state);
    void update();
};