#include "TrackSwitch.h"

// currently assumes sensor is closer to the forked track
bool TrackSwitch::readForkSensor()
{
    float distance = _usSensor.getDistance();
    return distance > 10.0f && distance < 165.0f;

    // if (distance > 10.0f && distance < 35.0f)
    //     return TrainTrack::Fork;
    // else if (distance < 165.0f)
    //     return TrainTrack::Main;
    // else
    //     return TrainTrack::Undetected;
}

void TrackSwitch::logState()
{
    String state = "Undetected";
    String direction = "Undetected";
    // String track = "Undetected";

    switch (_position)
    {
    case TrainPosition::Entering:
        state = "entering";
        break;
    case TrainPosition::Passing:
        state = "passing";
        break;
    case TrainPosition::Exiting:
        state = "exiting";
        break;
    }

    switch (_direction)
    {
    case TrainDirection::Forward:
        direction = "forward";
        break;
    case TrainDirection::Reverse:
        direction = "reverse";
        break;
    }

    if (_position == TrainPosition::Undetected)
        log_w("Train not detected at switch");
    else
        log_w("Train is %s %s", state, direction);
}

void TrackSwitch::begin(Ultrasonic usSensor, IrReflective irSensor, PowerFunctionsIrBroadcast pfIrClient, PowerFunctionsPort motorPort, bool defaultState)
{
    _usSensor = usSensor;
    _irSensor = irSensor;

    _pfIrClient = pfIrClient;
    _motorPort = motorPort;
    _switchState = defaultState;
    switchTrack(_switchState);
}

void TrackSwitch::registerCallback(TrainDetectionCallback callback)
{
    _onEnterAndExit = callback;
}

void TrackSwitch::switchTrack()
{
    _switchState = !_switchState;
    switchTrack(_switchState);
}

void TrackSwitch::switchTrack(bool state)
{
    _switchState = !_switchState;

    if (state)
    {
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::REVERSE7, 0);
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::FORWARD7, 0);
        delay(300);
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::BRAKE, 0);
    }
    else
    {
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::FORWARD7, 0);
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::REVERSE7, 0);
        delay(300);
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::BRAKE, 0);
    }
}

void TrackSwitch::update()
{
    TrainPosition lastState = _position;
    TrainDirection lastDirection = _direction;

    bool irDetection = _irSensor.isDetected();
    bool usDetection = readForkSensor();

    // only used when train is detected and _direction is set
    bool inSensor = (_direction == TrainDirection::Forward) ? irDetection : usDetection;
    bool outSensor = (_direction == TrainDirection::Forward) ? usDetection : irDetection;

    switch (_position)
    {
    case TrainPosition::Undetected:
        if (irDetection)
        {
            log_w("1");
            _position = TrainPosition::Entering;
            _direction = TrainDirection::Forward;
            _lastDetection = millis();

            //if (_onEnterAndExit != nullptr)
            //    _onEnterAndExit(_position, _direction);
            log_w("2");
        }
        else if (usDetection)
        {
            log_w("3");
            _position = TrainPosition::Entering;
            _direction = TrainDirection::Reverse;
            _lastDetection = millis();
            //if (_onEnterAndExit != nullptr)
            //    _onEnterAndExit(_position, _direction);
            log_w("4");
        }
        break;
    case TrainPosition::Entering:
        // wait for the outgoing sensor to trigger
        if (outSensor)
            _position = TrainPosition::Passing;
        else if (inSensor)
            _lastDetection = millis();
        else if (millis() - _lastDetection > 5000)
            _position = TrainPosition::Undetected;
        break;
    case TrainPosition::Passing:
        // wait for the incoming sensor to clear
        if (!inSensor)
            _position = TrainPosition::Exiting;
        else if (outSensor)
            _lastDetection = millis();
        else if (millis() - _lastDetection > 5000)
            _position = TrainPosition::Undetected;
        break;
    case TrainPosition::Exiting:
        // wait for the outgoing sensor to clear (but debounce for to account for gaps between cars)
        if (!outSensor)
        {
            if (millis() - _lastDetection > 300)
            {
                log_w("Train detected leaving switch");
                //if (_onEnterAndExit != nullptr)
                //    _onEnterAndExit(_position, _direction);
                _position = TrainPosition::Undetected;
                lastExitMillis = millis();
            }
        }
        else
            _lastDetection = millis();
        break;
    }

    if (_position != lastState || _direction != lastDirection)
    {
        logState();
    }
}
