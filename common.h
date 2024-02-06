#pragma once

#include <Lpf2Hub.h>
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

// will map to keys in either left or right remote layouts
enum RemoteKey {
  AuxOne, // '`' or KEY_BACKSPACE
  AuxTwo, // KEY_TAB or '\'
  AuxFunction, // KEY_FN or KEY_ENTER
  LeftPortFunction, // '3' or '8'
  LeftPortSpdUp, // 'e' or 'i'
  LeftPortBrake, // 's' or 'j'
  LeftPortSpdDn, // 'z' or 'n'
  RightPortFunction, // '4' or '9'
  RightPortSpdUp, // 'r' or 'o'
  RightPortBrake, // 'd' or 'k'
  RightPortSpdDn, // 'x' or 'm'
  NoTouchy,
};

enum RemoteColumn
{
  LeftPortCol, // c2 or c4
  RightPortCol, // c3 or c5
  AuxCol, // c1 or c6
  HiddenCol,
};

enum RemoteRow
{
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
  SBrick = 0x01,
  CircuitCubes = 0x02,
  PowerFunctionsIR = 0x03,
  NUM_DEVICES = 4
};

enum RemoteAction
{
  NoAction,
  SpdDn,
  Brake,
  SpdUp,
  PortFunction,
  BtConnection,
  Lpf2Color,
  IrChannel,
  IrMode
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
// TODO: better way to do this...eventually pass pointers to object instance?
struct State
{
  bool lpf2Connected;
  int lpf2Rssi;
  Color lpf2LedColor;
  byte lpf2SensorPort;
  Color lpf2SensorSpdUpColor;
  Color lpf2SensorStopColor;
  Color lpf2SensorSpdDnColor;
  int8_t lpf2SensorSpdUpFunction;
  int8_t lpf2SensorStopFunction;
  int8_t lpf2SensorSpdDnFunction;
  bool sBrickConnected;
  int sbrickRssi;
  bool circuitCubesConnected;
  int circuitCubesRssi;
  byte irChannel;
  uint8_t irMode;
  bool *irPortFunction;
};