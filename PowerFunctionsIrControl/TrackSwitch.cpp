#include "TrackSwitch.h"

#define DETECTION_TIMEOUT_MS 3000
#define DETECTION_DEBOUNCE_MS 300

void TrackSwitch::logState()
{
    String position, direction;

    if (_position == TrainPosition::Undetected)
    {
        if (_direction != TrainDirection::Undetected)
        {
            log_i("Train has exited");
        }
        else
        {
            log_w("Train detection error");
        }
        return;
    }

    switch (_position)
    {
    case TrainPosition::Entering:
        position = "entering";
        break;
    case TrainPosition::Passing:
        position = "passing";
        break;
    case TrainPosition::Exiting:
        position = "exiting";
        break;
    }

    switch (_direction)
    {
    case TrainDirection::Forking:
        direction = "forward";
        break;
    case TrainDirection::Merging:
        direction = "reverse";
        break;
    }

    log_i("Train is %s %s", position, direction);
}

void TrackSwitch::begin(std::function<bool()> mergeSensor, std::function<bool()> forkSensor, PowerFunctionsIrBroadcast pfIrClient, PowerFunctionsPort motorPort, uint8_t motorChannel, bool defaultState)
{
    _joinSensor = mergeSensor;
    _forkSensor = forkSensor;

    _pfIrClient = pfIrClient;
    _motorPort = motorPort;
    _motorChannel = motorChannel;
    _switchState = defaultState;
    switchTrack(_switchState);
}

void TrackSwitch::registerCallback(DetectionCallback callback)
{
    _onDetectionCallback = callback;
}

void TrackSwitch::switchTrack()
{
    _switchState = !_switchState;
    switchTrack(_switchState);
}

void TrackSwitch::switchTrack(bool state)
{
    if (state)
    {
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::REVERSE7, _motorChannel, false);
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::FORWARD7, _motorChannel, true);
        delay(200);
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::BRAKE, _motorChannel, true);
    }
    else
    {
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::FORWARD7, _motorChannel, false);
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::REVERSE7, _motorChannel, true);
        delay(200);
        _pfIrClient.single_pwm(_motorPort, PowerFunctionsPwm::BRAKE, _motorChannel, true);
    }
}

void TrackSwitch::update()
{
    TrainPosition lastPosition = _position;
    TrainDirection lastDirection = _direction;
    bool cleanExit = false;

    bool mergeDetection = _joinSensor();
    bool forkDetection = _forkSensor();

    // only used when train is detected and _direction is set
    bool inSensor = (_direction == TrainDirection::Forking) ? mergeDetection : forkDetection;
    bool outSensor = (_direction == TrainDirection::Forking) ? forkDetection : mergeDetection;

    switch (lastPosition)
    {
    case TrainPosition::Undetected:
        if (mergeDetection)
        {
            _position = TrainPosition::Entering;
            _direction = TrainDirection::Forking;
            _lastDetection = millis();
        }
        else if (forkDetection)
        {
            _position = TrainPosition::Entering;
            _direction = TrainDirection::Merging;
            _lastDetection = millis();
        }
        // reset direction after clean exit
        else if (_direction != TrainDirection::Undetected)
        {
            _direction = TrainDirection::Undetected;
        }
        break;
    case TrainPosition::Entering:
        // wait for the outgoing sensor to trigger
        if (outSensor)
        {
            _position = TrainPosition::Passing;
        }
        else if (inSensor)
        {
            _lastDetection = millis();
        }
        else if (millis() - _lastDetection > DETECTION_TIMEOUT_MS)
        {
            _position = TrainPosition::Undetected;
            _direction = TrainDirection::Undetected;
        }
        break;
    case TrainPosition::Passing:
        // wait for the outgoing sensor to clear (but debounce for to account for gaps between cars)
        if (!inSensor)
        {
            if (millis() - _lastDetection > DETECTION_DEBOUNCE_MS)
            {
                _position = TrainPosition::Exiting;
            }
        }
        else if (outSensor)
        {
            _lastDetection = millis();
        }
        else if (millis() - _lastDetection > DETECTION_TIMEOUT_MS)
        {
            _position = TrainPosition::Undetected;
            _direction = TrainDirection::Undetected;
        }
        break;
    case TrainPosition::Exiting:
        // wait for the outgoing sensor to clear (but debounce for to account for gaps between cars)
        if (!outSensor)
        {
            if (millis() - _lastDetection > DETECTION_DEBOUNCE_MS)
            {
                _position = TrainPosition::Undetected;
                // direction not cleared yet to show clean exit
            }
        }
        else
        {
            _lastDetection = millis();
        }
        break;
    }

    if (_position != lastPosition)
    {
        if (_onDetectionCallback != nullptr)
        {
            _onDetectionCallback(_position, _direction);
        }

        logState();
    }
}
