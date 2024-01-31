#define CIRCUIT_CUBE_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CIRCUIT_CUBE_WRITE_CHARACTERISTIC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CIRCUIT_CUBE_NOTIFY_CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#define CIRCUIT_CUBE_SERVICE_DEVICE_INFORMATION_UUID "0000180a-0000-1000-8000-00805f9b34fb"
#define CIRCUIT_CUBE_CHARACTERISTIC_HARDWARE_REVISION_UUID "00002a27-0000-1000-8000-00805f9b34fb"
#define CIRCUIT_CUBE_CHARACTERISTIC_FIRMWARE_REVISION_UUID "00002a26-0000-1000-8000-00805f9b34fb"


// // Turn off power to all motors command: <0>
// private static readonly byte[] TURN_OFF_ALL_COMMAND = new[] { (byte)'0' };
// // Battery Status Command: <b>
// private static readonly byte[] BATTERY_STATUS_COMMAND = new[] { (byte)'b' };
// // Motor Driving Commands: <+/-><000~255><a/b/c>
// private readonly byte[] _driveMotorsBuffer =
// new byte[] { 0x00, 0x00, 0x00, 0x00, (byte)'a', 0x00, 0x00, 0x00, 0x00, (byte)'b', 0x00, 0x00, 0x00, 0x00, (byte)'c' };

enum struct CircuitCubesCommandType
{
    BRAKEALL = '0',
    BATTERY = 'b',
};

enum struct CircuitCubesHubChannel
{
    A = 0x01,
    B = 0x02,
    C = 0x03,
};

#define CIRCUIT_CUBES_SPEED_MAX 255
#define CIRCUIT_CUBES_SPEED_MIN -255