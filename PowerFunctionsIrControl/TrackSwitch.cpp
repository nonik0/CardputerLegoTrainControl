#include "TrackSwitch.h"

#define DETECTION_TIMEOUT_MS 1500 // time before tracking state is cleared due to invalid transition
#define NOISE_DEBOUNCE_MS 50      // max length to ignore for false positive detection blips

void TrackSwitch::logState()
{
    if (_position == TrainPosition::Undetected)
    {
        switch (_direction)
        {
        case TrainDirection::Undetected:
            log_w("Train detection error");
            break;
        case TrainDirection::Forking:
            log_i("Train has exited forward");
            break;
        case TrainDirection::Merging:
            log_i("Train has exited reverse");
            break;
        }
        return;
    }

    const char *position =
        (_position == TrainPosition::Entering) ? "entering" : (_position == TrainPosition::Passing) ? "passing"
                                                                                                    : "exiting";
    const char *direction =
        (_direction == TrainDirection::Forking) ? "forward" : "reverse";

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
    const unsigned long now = millis();

    // callbacks sent when position changes
    TrainPosition lastPosition = _position;

    // helper functions for tracking/updating state and timestamps for train detection
    auto isStable = [&](unsigned long t, unsigned long ms = NOISE_DEBOUNCE_MS)
    {
        return now - t > ms;
    };
    auto setPosition = [&](TrainPosition p, TrainDirection d = TrainDirection::Undetected)
    {
        if (d == TrainDirection::Undetected)
        {
            d = _direction;
        }

        // update specific timestamps for tracking for certain position transitions
        if (_position == TrainPosition::Undetected && p == TrainPosition::Entering && d != TrainDirection::Undetected)
        {
            _lastEnterDetection = now;
        }
        else if (_position == TrainPosition::Entering && p == TrainPosition::Passing && d != TrainDirection::Undetected)
        {
            // use train speed to determine sensor debounce/timeout thresholds for exit
            auto trainSensorToSensorMs = now - _lastEnterDetection - NOISE_DEBOUNCE_MS; // remove debounce from timing (now~=toggle+debounce)
            _carGapThresholdMs = trainSensorToSensorMs / 4;   // interval sensors need to be stable to transition to next correct state
            _speedTimeoutThresholdMs = trainSensorToSensorMs; // interval that times outs any invalid sensors and resets state to undetected
            log_i("Threshold=%d/%dms", _carGapThresholdMs, _speedTimeoutThresholdMs);
        }

        _position = p;
        _direction = d;
        _lastPositionChange = now;
    };
    auto resetDetection = [&]()
    {
        setPosition(TrainPosition::Undetected, TrainDirection::Undetected);
    };

    bool joinReading = _joinSensor();
    if (joinReading != _joinReading)
    {
        _joinReading = joinReading;
        _joinToggle = now;
    }

    bool forkReading = _forkSensor();
    if (forkReading != _forkReading)
    {
        _forkReading = forkReading;
        _forkToggle = now;
    }

    if (_position == TrainPosition::Undetected)
    {
        // start tracking when either sensor is stably triggered
        if (joinReading && !forkReading && isStable(_joinToggle))
        {
            setPosition(TrainPosition::Entering, TrainDirection::Forking);
        }
        else if (forkReading && !joinReading && isStable(_forkToggle))
        {
            setPosition(TrainPosition::Entering, TrainDirection::Merging);
        }
        else if (_direction != TrainDirection::Undetected)
        {
            // clean exit leaves direction known for the later callback to include, this clears the remaining tracking state at
            // the next update/cycle after the callback has been invoked and it's safe to fully clear detection state
            resetDetection();
        }
    }
    else
    {
        const bool isForward = _direction == TrainDirection::Forking;
        const bool enterReading = isForward ? joinReading : forkReading;
        const bool exitReading = isForward ? forkReading : joinReading;
        const unsigned long enterToggle = isForward ? _joinToggle : _forkToggle;
        const unsigned long exitToggle = isForward ? _forkToggle : _joinToggle;

        switch (_position)
        {
        case TrainPosition::Entering:
            // wait for train to trigger exit sensor
            if (enterReading && exitReading && isStable(exitToggle))
            {
                if (!isStable(enterToggle))
                {
                    log_w("Enter sensor not stable while entering");
                }

                setPosition(TrainPosition::Passing);
            }

            // enter sensor shouldn't clear while train is entering
            if (!enterReading && isStable(enterToggle))
            {
                log_w("Enter sensor cleared while entering, resetting detection");
                resetDetection();
            }
            break;
        case TrainPosition::Passing:
            // wait for train to clear enter sensor, ignore car gaps with longer debounce threshold
            if (!enterReading && exitReading && isStable(enterToggle, _carGapThresholdMs))
            {
                if (!isStable(exitToggle))
                {
                    log_w("Exit sensor not stable while passing");
                }

                setPosition(TrainPosition::Exiting);
            }

            // exit sensor shouldn't clear while train is passing
            if (!exitReading && isStable(exitToggle, _speedTimeoutThresholdMs))
            {
                log_w("Exit sensor cleared while passing, resetting detection");
                resetDetection();
            }
            break;
        case TrainPosition::Exiting:
            // wait for the train to clear both sensors
            if (!enterReading && !exitReading && isStable(exitToggle))
            {
                if (!isStable(enterToggle))
                {
                    log_w("Enter sensor not stable while exiting");
                }

                // direction is unchanged to signify a clean exit, i.e. the direction of train's exit is known
                setPosition(TrainPosition::Undetected);
            }

            // if enter sensor toggles back on, go back to passing state
            if (enterReading && isStable(enterToggle, _carGapThresholdMs))
            {
                log_w("Enter sensor toggled back on while exiting, going back to passing state");
                setPosition(TrainPosition::Passing);
                // resetDetection();
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
