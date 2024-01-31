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

enum Port
{
  BtA = 0, // == PoweredUpHubPort::A // hacky but works for compare with PoweredUpHubPort
  BtB = 1, // == PoweredUpHubPort::B
  IrR,
  IrB,
  None
};

enum RemoteDevice
{
  PoweredUpHub = 0x00,
  SBrick = 0x01,
  PowerFunctionsIR = 0x02,
  CircuitCubes = 0x03
};

enum Action
{
  BtConnection,
  BtColor,
  IrChannel,
  IrTrackState,
  SpdUp,
  SpdDn,
  Brake,
  NoAction
};

struct Button
{
  char key;
  int x;
  int y;
  int w;
  int h;
  RemoteDevice device;
  byte port; // device-specific
  Action action;
  unsigned short color;
  bool pressed;
};

// used for conveying current state when drawing
// TODO: better way to do this...eventually pass pointers to object instance?
struct State
{
  bool btConnected;
  int btRssi;
  Color btLedColor;
  byte btSensorPort;
  Color btSensorSpdUpColor;
  Color btSensorStopColor;
  Color btSensorSpdDnColor;
  int8_t btSensorSpdUpFunction;
  int8_t btSensorStopFunction;
  int8_t btSensorSpdDnFunction;
  bool sBrickConnected;
  byte irChannel;
};