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

    // state for tracking with callback for updates
    TrainDirection _direction;
    TrainPosition _position;
    DetectionCallback _onDetectionCallback;

    // tracking for detection timing
    unsigned long _firstDetection;
    unsigned long _lastDetection;
    unsigned long _lastClear;
    unsigned long _stableInterval;
    unsigned long _timeoutInterval;

public:
    void logState();
    void begin(std::function<bool()> joinSensor, std::function<bool()> forkSensor, PowerFunctionsIrBroadcast pfIrClient, PowerFunctionsPort motorPort, uint8_t motorChannel, bool defaultState = false);
    void registerCallback(DetectionCallback callback);
    void switchTrack();
    void switchTrack(bool state);
    void update();
};