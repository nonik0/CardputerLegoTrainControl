#include "TrackSwitch.h"

#define CAR_DETECTION_DEBOUNCE_MS 300 // max length of detection blips from gaps between cars
#define DETECTION_TIMEOUT_MS 1000     // time before tracking state is cleared due to invalid transition
#define NOISE_DEBOUNCE_MS 50          // max length to ignore for false positive detection blips

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
        // wait for train to trigger sensoor from either direction
        if (mergeDetection)
        {
            // wait for stable out sensor detection before updating position
            if (millis() - _lastClear > NOISE_DEBOUNCE_MS)
            {
                _position = TrainPosition::Entering;
                _direction = TrainDirection::Forking;
                _lastDetection = millis();
            }
        }
        else if (forkDetection)
        {
            // wait for stable out sensor detection before updating position
            if (millis() - _lastClear > NOISE_DEBOUNCE_MS)
            {
                _position = TrainPosition::Entering;
                _direction = TrainDirection::Merging;
                _lastDetection = millis();
            }
        }
        else
        {
            // reset direction after clean exit
            if (_direction != TrainDirection::Undetected)
            {
                _direction = TrainDirection::Undetected;
            }

            _lastClear = millis();
        }
        break;
    case TrainPosition::Entering:
        // wait for train to trigger out sensor
        if (outSensor)
        {
            // wait for stable out sensor detection before updating position
            if (millis() - _lastClear > NOISE_DEBOUNCE_MS)
            {
                _position = TrainPosition::Passing;
            }
        }
        else
        {
            _lastClear = millis();
        }
        // in sensor shouldn't clear while train is entering
        if (inSensor)
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
        // wait for train to clear in sensor
        if (!inSensor)
        {
            // debounce small gaps from between cars before updating position
            if (millis() - _lastDetection > CAR_DETECTION_DEBOUNCE_MS)
            {
                _position = TrainPosition::Exiting;
            }
        }
        else
        {
            _lastDetection = millis();
        }
        // out sensor shouldn't clear while train is passing
        if (outSensor)
        {
            // overloading name here, "_lastDetection2"
            _lastClear = millis();
        }
        else if (millis() - _lastClear > DETECTION_TIMEOUT_MS)
        {
            _position = TrainPosition::Undetected;
            _direction = TrainDirection::Undetected;
        }
        break;
    case TrainPosition::Exiting:
        // wait for the train to clear out sensor
        if (!outSensor)
        {
            // debounce small gaps from between cars before updating position
            if (millis() - _lastDetection > CAR_DETECTION_DEBOUNCE_MS)
            {
                _position = TrainPosition::Undetected;
                // direction not cleared yet to show clean exit
            }
        }
        else
        {
            _lastDetection = millis();
        }
        // in sensor shouldn't detect while train is exiting
        if (!inSensor)
        {
            _lastClear = millis();
        }
        else if (millis() - _lastClear > DETECTION_TIMEOUT_MS)
        {
            _position = TrainPosition::Undetected;
            _direction = TrainDirection::Undetected;
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
