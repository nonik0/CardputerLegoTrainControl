#include "TrackSwitch.h"

#define DETECTION_TIMEOUT_MS 1500 // time before tracking state is cleared due to invalid transition
#define NOISE_DEBOUNCE_MS 50      // max length to ignore for false positive detection blips

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
    // callbacks sent when position changes
    TrainPosition lastPosition = _position;

    bool joinReading = _joinSensor();
    if (joinReading != _lastJoinReading)
    {
        _lastJoinReading = joinReading;
        _lastJoinToggle = millis();
    }

    bool forkReading = _forkSensor();
    if (forkReading != _lastForkReading)
    {
        _lastForkReading = forkReading;
        _lastForkToggle = millis();
    }

    if (_position == TrainPosition::Undetected)
    {
        // start tracking when either sensor is stably triggered (ignoring case where both trigger in debounce window for now)
        if (joinReading && millis() - _lastJoinToggle > NOISE_DEBOUNCE_MS)
        {
            _position = TrainPosition::Entering;
            _direction = TrainDirection::Forking;
            _lastEnterDetection = millis();
            _lastPositionChange = _lastEnterDetection;
        }
        else if (forkReading && millis() - _lastForkToggle > NOISE_DEBOUNCE_MS)
        {
            _position = TrainPosition::Entering;
            _direction = TrainDirection::Merging;
            _lastEnterDetection = millis();
            _lastPositionChange = _lastEnterDetection;
        }
        else if (_direction != TrainDirection::Undetected)
        {
            _direction = TrainDirection::Undetected;
        }
    }
    else
    {
        bool enterReading = (_direction == TrainDirection::Forking) ? joinReading : forkReading;
        bool exitReading = (_direction == TrainDirection::Forking) ? forkReading : joinReading;
        unsigned long lastEnterToggle = (_direction == TrainDirection::Forking) ? _lastForkToggle : _lastJoinToggle;
        unsigned long lastExitToggle = (_direction == TrainDirection::Forking) ? _lastJoinToggle : _lastForkToggle;

        switch (_position)
        {
        case TrainPosition::Entering:
            // wait for train to trigger exit sensor
            if (enterReading && exitReading && millis() - lastExitToggle > NOISE_DEBOUNCE_MS)
            {
                _position = TrainPosition::Passing;
                _lastPositionChange = millis();

                // use train speed to determine sensor debounce/timeout thresholds for exit
                auto trainSensorToSensorMs = lastExitToggle - _lastEnterDetection - NOISE_DEBOUNCE_MS;
                _carGapThresholdMs = trainSensorToSensorMs / 4; // interval sensors need to be stable to transition to next correct state
                _speedTimeoutThresholdMs = trainSensorToSensorMs;    // interval that times outs any invalid sensors and resets state to undetected
                log_i("Threshold=%d/%dms", _carGapThresholdMs, _speedTimeoutThresholdMs);
            }

            // enter sensor shouldn't clear while train is entering
            if (!enterReading && millis() - lastEnterToggle > DETECTION_TIMEOUT_MS)
            {
                _position = TrainPosition::Undetected;
                _direction = TrainDirection::Undetected;
            }
            break;
        case TrainPosition::Passing:
            // wait for train to clear enter sensor, ignore car gaps with stable threshold
            if (!enterReading && exitReading && millis() - lastEnterToggle > _carGapThresholdMs)
            {
                if (millis() - lastExitToggle < NOISE_DEBOUNCE_MS)
                {
                    log_w("Exit sensor not stable while passing");
                }

                _position = TrainPosition::Exiting;
                _lastPositionChange = millis();
            }

            // exit sensor shouldn't clear while train is passing
            if (!exitReading && millis() - lastExitToggle > _speedTimeoutThresholdMs)
            {
                _position = TrainPosition::Undetected;
                _direction = TrainDirection::Undetected;
            }
            break;
        case TrainPosition::Exiting:
            // wait for the train to clear out sensor
            if (!enterReading && !exitReading && millis() - lastExitToggle < NOISE_DEBOUNCE_MS)
            {
                if (millis() - lastEnterToggle < NOISE_DEBOUNCE_MS)
                {
                    log_w("Enter sensor not stable while exiting");
                }

                _position = TrainPosition::Undetected;
                _lastPositionChange = millis();
                // direction not cleared yet to show clean exit, i.e. direction of exit is known
            }

            // if enter sensor toggles back on, go back to passing state 
            if (enterReading && millis() - lastEnterToggle > _carGapThresholdMs)
            {
                _position = TrainPosition::Passing;
                _lastPositionChange = millis();
                //_position = TrainPosition::Undetected;
                //_direction = TrainDirection::Undetected;
            }

            break;
        }
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
