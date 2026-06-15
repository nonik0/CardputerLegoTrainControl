#pragma once

#include <Arduino.h>

#include "..\IrBroadcast.h"

enum class TrainDirection
{
    Undetected,
    Forking, // towards the fork
    Merging  // merging into fork
};

enum class TrainPosition
{
    Undetected,
    Entering,
    Passing,
    Exiting
};

// called when train is first detected and when clearing the switch
typedef void (*DetectionCallback)(TrainPosition position, TrainDirection direction);

class TrackSwitch
{
private:
    std::function<bool()> _joinSensor;
    std::function<bool()> _forkSensor;
    PowerFunctionsIrBroadcast _pfIrClient;
    PowerFunctionsPort _motorPort;
    uint8_t _motorChannel;
    bool _switchState;

    TrainDirection _direction;
    TrainPosition _position;
    unsigned long _lastDetection;
    unsigned long _lastClear;
    DetectionCallback _onDetectionCallback;

public:
    unsigned long lastExitMillis = 0;

    void logState();
    void begin(std::function<bool()> joinSensor, std::function<bool()> forkSensor, PowerFunctionsIrBroadcast pfIrClient, PowerFunctionsPort motorPort, uint8_t motorChannel, bool defaultState = false);
    void registerCallback(DetectionCallback callback);
    void switchTrack();
    void switchTrack(bool state);
    void update();
};