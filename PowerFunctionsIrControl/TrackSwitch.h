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
    bool _joinReading;
    bool _forkReading;
    unsigned long _joinToggle;
    unsigned long _forkToggle;

    PowerFunctionsIrBroadcast _pfIrClient;
    PowerFunctionsPort _motorPort;
    uint8_t _motorChannel;
    bool _switchState;

    // state for tracking with callback for updates
    TrainDirection _direction;
    TrainPosition _position;
    DetectionCallback _onDetectionCallback;

    // tracking for detection timing
    unsigned long _lastEnterDetection;
    unsigned long _lastPositionChange;
    unsigned long _carGapThresholdMs;
    unsigned long _speedTimeoutThresholdMs;

public:
    void logState();
    void begin(std::function<bool()> joinSensor, std::function<bool()> forkSensor, PowerFunctionsIrBroadcast pfIrClient, PowerFunctionsPort motorPort, uint8_t motorChannel, bool defaultState = false);
    void registerCallback(DetectionCallback callback);
    void setTrackState(bool state);
    void switchTrack();
    void switchTrack(bool state);
    void update();
};