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
  Port port;
  Action action;
  unsigned short color;
  bool pressed;
};

// used for conveying current state when drawing
struct State
{
  bool btConnected;
  Color btLedColor;
  byte btSensorPort;
  Color btSensorSpdUpColor;
  Color btSensorStopColor;
  Color btSensorSpdDnColor;
  uint8_t btSensorStopFunction;
  byte irChannel;
};