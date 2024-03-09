#include <Arduino.h>
#include <LegoinoCommon.h>

#include "IrReflective.hpp"
#include "Ultrasonic.h"

// forward is going towards the fork
// reervse is merging into the fork
enum class TrainDirection
{
    Undetected,
    Forward,
    Reverse
};

enum class TrainTrack
{
    Undetected,
    Main,
    Fork
};

enum class TrainState
{
    Undetected,
    Entering,
    Passing,
    Exiting
};

class TrackSwitch
{
private:
    Ultrasonic _usSensor;
    IrReflective _irSensor;
    byte _motorPort;

    TrainDirection _direction;
    TrainTrack _track;
    TrainState _state;

    TrainTrack readForkSensor();
    void logState();
public:
    void init(Ultrasonic usSensor, IrReflective irSensor, byte motorPort);
    void update();
    // onEnterCallback, onExitCallback
};