// https://social.sbrick.com/custom/The_SBrick_BLE_Protocol.pdf

#pragma once

#define SBRICK_SERVICE_UUID "4dc591b0-857c-41de-b5f1-15abda665b0c"
#define SBRICK_CHARACTERISTIC_UUID_REMOTE_CONTROL "02b8cbcc-0e25-4bda-8790-a15f53e6010f"
#define SBRICK_CHARACTERISTIC_UUID_QUICK_DRIVE "489a6ae0-c1ab-4c9c-bdb2-11d373c1b7fb"

#define SBRICK_SERVICE_UUID_DEVICE_INFORMATION "0000180a-0000-1000-8000-00805f9b34fb"
#define SBRICK_CHARACTERISTIC_UUID_HARDWARE_REVISION "00002a27-0000-1000-8000-00805f9b34fb"

#define SBRICK_DEVICE_IDENTIFIER 0x07020D23FC198763 

#define SBRICK_ADC_CHANNEL_COUNT 10 // 2x for 4 ports, battery and temperature

enum struct SBrickCommandType
{
    BRAKE = 0x00,
    DRIVE = 0x01,
    SET_WATCHDOG_TIMEOUT = 0x0D,
    GET_WATCHDOG_TIMEOUT = 0x0E,
    QUERY_ADC = 0x0F,
    REBOOT = 0x12,
    BRAKE_WITH_PWM = 0x13,
    GET_CHANNEL_STATUS = 0x22,
    SET_DEVICE_NAME = 0x2A,
    GET_DEVICE_NAME = 0x2B,
    SET_VOLTAGE_MEASURE = 0x2C,
    GET_VOLTAGE_MEASURE = 0x2D,
    SET_VOLTAGE_NOTIF = 0x2E,
    GET_VOLTAGE_NOTIF = 0x2F,
    SEND_SIGNAL = 0x33
};

enum struct SBrickHubPort
{
    A = 0x00,
    C = 0x01, // weird
    B = 0x02, // weird
    D = 0x03,
};

const char SBrickPortToChar[] = {'A', 'C', 'B', 'D'};

enum struct SBrickAdcChannel 
{
    A_C1 = 0x00,
    A_C2 = 0x01,
    C_C1 = 0x02,
    C_C2 = 0x03,
    B_C1 = 0x04,
    B_C2 = 0x05,
    D_C1 = 0x06,
    D_C2 = 0x07,
    Voltage = 0x08, // battery voltage
    Temperature = 0x09 // internal temperature
};

const byte SBrickAdcChannelToPort[10] = {
    (byte)SBrickHubPort::A,
    (byte)SBrickHubPort::A,
    (byte)SBrickHubPort::C,
    (byte)SBrickHubPort::C,
    (byte)SBrickHubPort::B,
    (byte)SBrickHubPort::B,
    (byte)SBrickHubPort::D,
    (byte)SBrickHubPort::D,
    0xFF, // battery voltage
    0xFF  // internal temperature
};

const byte SBrickPortToAdcChannel[4][2] = {
    {(byte)SBrickAdcChannel::A_C1, (byte)SBrickAdcChannel::A_C2}, // SBrickHubPort::A
    {(byte)SBrickAdcChannel::C_C1, (byte)SBrickAdcChannel::C_C2}, // SBrickHubPort::C
    {(byte)SBrickAdcChannel::B_C1, (byte)SBrickAdcChannel::B_C2}, // SBrickHubPort::B
    {(byte)SBrickAdcChannel::D_C1, (byte)SBrickAdcChannel::D_C2}  // SBrickHubPort::D
};

enum struct SBrickRecordType
{
    ProductType = 0x00,
    Deprecated = 0x01,
    DeviceIdentifier = 0x02,
    SimpleSecurityStatus = 0x03,
    CommandResponse = 0x04,
    ThermalProtectionStatus = 0x05,
    VoltageMeasurement = 0x06,
    SignalCompleted = 0x07,
};

enum struct SBrickCommandResponseReturnCodes {
    Success = 0x00,
    InvalidDataLength = 0x01,
    InvalidParameter = 0x02,
    NoSuchCommand = 0x03,
    NoAuthNeeded = 0x04,
    AuthError = 0x05,
    AuthNeeded = 0x06,
    AuthzError = 0x07,
    ThermalProtectionActive = 0x08,
    SystemStateInvalidForCommand = 0x09
};

enum struct WedoSensor {
    NotFound = 0x00,
    Motion = 0x01,
    Tilt = 0x02,
    Unknown = 0x0FF
};

enum struct WedoTilt {
    Neutral = 0x00,
    Backward = 0x01,
    Forward = 0x02,
    Right = 0x03,
    Left = 0x04,
};

#define SBRICK_MAX_CHANNEL_SPEED 254
#define SBRICK_MIN_CHANNEL_SPEED -254