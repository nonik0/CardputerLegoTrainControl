#include "TrackSwitch.h"

// currently assumes sensor is closer to the forked track
TrainTrack TrackSwitch::readForkSensor()
{
    float distance = _usSensor.getDistance();

    if (distance > 10.0f && distance < 35.0f)
        return TrainTrack::Fork;
    else if (distance < 165.0f)
        return TrainTrack::Main;
    else
        return TrainTrack::Undetected;
}

void TrackSwitch::logState()
{
    String state = "Undetected";
    String direction = "Undetected";
    String track = "Undetected";

    switch (_state)
    {
    case TrainState::Entering:
        state = "entering";
        break;
    case TrainState::Passing:
        state = "passing";
        break;
    case TrainState::Exiting:
        state = "exiting";
        break;
    }

    switch (_direction)
    {
    case TrainDirection::Forward:
        direction = "forward";
        break;
    case TrainDirection::Reverse:
        direction = "backward";
        break;
    }

    switch (_track)
    {
    case TrainTrack::Main:
        track = "main";
        break;
    case TrainTrack::Fork:
        track = "fork";
        break;
    }

    if (_state == TrainState::Undetected)
        log_w("Train left switch");
    else
        log_w("Train is %s %s on %s track", state, direction, track);
}

void TrackSwitch::begin(Ultrasonic usSensor, IrReflective irSensor, byte motorPort)
{
    _usSensor = usSensor;
    _irSensor = irSensor;
    _motorPort = motorPort;
}

void TrackSwitch::update()
{
    TrainTrack forkSensorReading;

    TrainState lastState = _state;
    TrainDirection lastDirection = _direction;
    TrainTrack lastTrack = _track;

    switch (_state)
    {
    case TrainState::Undetected:
        if (_irSensor.isDetected())
        {
            _state = TrainState::Entering;
            _direction = TrainDirection::Forward;
            _track = TrainTrack::Main;
        }
        else if ((forkSensorReading = readForkSensor()) != TrainTrack::Undetected)
        {
            _state = TrainState::Entering;
            _direction = TrainDirection::Reverse;
            _track = forkSensorReading;
        }
        break;
    case TrainState::Entering:
        // wait for the opposite sensor to be triggered
        if (_direction == TrainDirection::Forward)
        {
            if ((forkSensorReading = readForkSensor()) != TrainTrack::Undetected)
            {
                _state = TrainState::Passing;
                _track = forkSensorReading;
            }
        }
        else if (_direction == TrainDirection::Reverse)
        {
            if (_irSensor.isDetected())
            {
                _state = TrainState::Passing;
                _track = TrainTrack::Main;
            }
        }
        break;
    case TrainState::Passing:
        // wait for the initial sensor to trigger to clear
        if (_direction == TrainDirection::Forward)
        {
            if (!_irSensor.isDetected()) // TODO debounce?
            {
                _state = TrainState::Exiting;
            }
        }
        else if (_direction == TrainDirection::Forward)
        {
            if ((forkSensorReading = readForkSensor()) == TrainTrack::Undetected) // TODO: debounce
            {
                _state = TrainState::Exiting;
            }
        }
        break;
    case TrainState::Exiting:
        // wait for the opposite sensor to clear
        if (_direction == TrainDirection::Forward)
        {
            if ((forkSensorReading = readForkSensor()) == TrainTrack::Undetected) // TODO: debounce
            {
                _state = TrainState::Undetected;
            }
        }
        else if (_direction == TrainDirection::Forward)
        {
            if (!_irSensor.isDetected()) // TODO debounce?
            {
                _state = TrainState::Undetected;
            }
        }
        break;
    }

    if (_state != lastState || _direction != lastDirection || _track != lastTrack)
    {
        // TODO: invoke any applicable callbacks
        logState();
    }
}

// void switchTrack()
//  {
//    switchDirection = !switchDirection;

//   if (switchDirection)
//   {
//     client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::REVERSE7, 0);
//     delay(50);
//     client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::FORWARD7, 0);
//     delay(500);
//     client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::BRAKE, 0);
//   }
//   else
//   {
//     client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::FORWARD7, 0);
//     delay(50);
//     client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::REVERSE7, 0);
//     delay(500);
//     client.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::BRAKE, 0);
//   }
// }