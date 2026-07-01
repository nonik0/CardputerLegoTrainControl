#pragma once

#include <Lpf2Hub.h>
#include <PowerFunctions.h>
#include <M5Cardputer.h>

// RBG565 colors
const unsigned short COLOR_BLACK = 0x18E3;
const unsigned short COLOR_DARKGRAY = 0x0861;
const unsigned short COLOR_MEDGRAY = 0x2104;
const unsigned short COLOR_LIGHTGRAY = 0x4208;
const unsigned short COLOR_DARKRED = 0x5800;
const unsigned short COLOR_ORANGE = 0xEBC3;
const unsigned short COLOR_TEAL = 0x07CC;
const unsigned short COLOR_BLUEGRAY = 0x0B0C;
const unsigned short COLOR_BLUE = 0x026E;
const unsigned short COLOR_PURPLE = 0x7075;

// index maps/casts to Legoino Color enum
const unsigned short BtColors[] = {TFT_BLACK, TFT_PINK, TFT_PURPLE, TFT_BLUE, 0x9E7F, TFT_CYAN, TFT_GREEN, TFT_YELLOW, COLOR_ORANGE, TFT_RED, TFT_WHITE};

template <typename T>
struct KeyMapping
{
  uint8_t keyboardKey;
  T functionKey;
  bool isLeftRemote; // ignored for aux buttons
};

// will map to keys in either left or right remote layouts
enum RemoteKey
{
  AuxOne,            // '`' or KEY_BACKSPACE
  AuxTwo,            // KEY_TAB or '\'
  AuxFunction,       // KEY_FN or KEY_ENTER
  LeftPortFunction,  // '3' or '8'
  LeftPortSpdUp,     // 'e' or 'i'
  LeftPortBrake,     // 's' or 'j'
  LeftPortSpdDn,     // 'z' or 'n'
  RightPortFunction, // '4' or '9'
  RightPortSpdUp,    // 'r' or 'o'
  RightPortBrake,    // 'd' or 'k'
  RightPortSpdDn,    // 'x' or 'm'
  RemoteNoOp,
};

enum AuxKey
{
  BrightnessUp, // ';'
  BrightnessDn, // '.'
  SaveSettings, // 'v'
  ToggleLeft,   // KEY_LEFT_CTRL
  ToggleRight,  // ' '
  AuxNoOp,
};

static constexpr KeyMapping<RemoteKey> RemoteKeyMappings[] = {
    // left remote
    {'`', RemoteKey::AuxOne, true},
    {KEY_TAB, RemoteKey::AuxTwo, true},
    {KEY_FN, RemoteKey::AuxFunction, true},

    {'3', RemoteKey::LeftPortFunction, true},
    {'e', RemoteKey::LeftPortSpdUp, true},
    {'s', RemoteKey::LeftPortBrake, true},
    {'z', RemoteKey::LeftPortSpdDn, true},

    {'4', RemoteKey::RightPortFunction, true},
    {'r', RemoteKey::RightPortSpdUp, true},
    {'d', RemoteKey::RightPortBrake, true},
    {'x', RemoteKey::RightPortSpdDn, true},

    // right remote
    {'8', RemoteKey::LeftPortFunction, false},
    {'i', RemoteKey::LeftPortSpdUp, false},
    {'j', RemoteKey::LeftPortBrake, false},
    {'n', RemoteKey::LeftPortSpdDn, false},

    {'9', RemoteKey::RightPortFunction, false},
    {'o', RemoteKey::RightPortSpdUp, false},
    {'k', RemoteKey::RightPortBrake, false},
    {'m', RemoteKey::RightPortSpdDn, false},

    {KEY_BACKSPACE, RemoteKey::AuxOne, false},
    {'\\', RemoteKey::AuxTwo, false},
    {KEY_ENTER, RemoteKey::AuxFunction, false},
};

static constexpr KeyMapping<AuxKey> AuxKeyMappings[] = {
    {'v', AuxKey::SaveSettings, false},
    {';', AuxKey::BrightnessUp, false},
    {'.', AuxKey::BrightnessDn, false},
    {KEY_LEFT_CTRL, AuxKey::ToggleLeft, false},
    {' ', AuxKey::ToggleRight, false},
};

enum RemoteColumn
{
  LeftPortCol,  // c2 or c4
  RightPortCol, // c3 or c5
  AuxCol,       // c1 or c6
  HiddenCol,
};

enum RemoteRow
{
  Row0,
  Row1,
  Row1_5,
  Row2,
  Row2_5,
  Row3,
  Row3_5,
  HiddenRow,
};

enum RemoteDevice
{
  PoweredUp = 0x00,
  PowerFunctionsIR = 0x01,
  SBrick = 0x02,
  CircuitCubes = 0x03,
  NUM_DEVICES = 4
};

enum RemoteAction
{
  NoAction = 0,
  SpdDn = 1,
  Brake = 2,
  SpdUp = 3,
  PortFunction = 4,
  BtConnection = 5,
  Lpf2Color = 6,
  IrChannel = 7,
  IrMode = 8
};

struct Button
{
  RemoteKey key;
  RemoteColumn col;
  RemoteRow row;
  int w;
  int h;
  RemoteDevice device;
  byte port; // device-specific
  RemoteAction action;
  unsigned short color;
  bool pressed;
};

// used for conveying current state when drawing
// TODO: eventually refactor devices into remotes and draw off their state
// TODO: fix uint8_t and byte type usage inconsistency
struct State
{
  bool lpf2Connected;
  int lpf2Rssi;
  Color lpf2SensorColor;
  byte lpf2SensorPort;
  int8_t lpf2SensorSpdUpFunction;
  int8_t lpf2SensorStopFunction;
  int8_t lpf2SensorSpdDnFunction;
  bool sBrickConnected;
  int sbrickRssi;
  byte sbrickMotionSensorPort;
  float sbrickMotionSensorV;
  float sbrickMotionSensorNeutralV;
  byte sbrickTiltSensorPort;
  float sbrickTiltSensorV;
  float sbrickTiltSensorNeutralV;
  bool circuitCubesConnected;
  int circuitCubesRssi;
  byte irChannel;
  uint8_t irMode;
  bool *irPortFunction;
  PowerFunctionsPort irSwitchPort;
  uint8_t irSwitchMode;
  uint8_t irSwitchDetection;
};