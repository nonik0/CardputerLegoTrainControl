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

struct ColorMapping
{
  Color color;
  unsigned short rgb565;
};

// TODO: investigate colors
const ColorMapping BtColors[] = {
    {Color::RED, TFT_RED},
    {Color::ORANGE, COLOR_ORANGE},
    {Color::YELLOW, TFT_YELLOW},
    {Color::GREEN, TFT_GREEN},
    {Color::CYAN, TFT_CYAN},
    {Color::LIGHTBLUE, 0x9E7F},
    {Color::BLUE, TFT_BLUE},
    {Color::PURPLE, TFT_PURPLE},
    {Color::PINK, TFT_PINK},
    {Color::WHITE, TFT_WHITE},
    {Color::BLACK, TFT_BLACK},
};
const unsigned short BtNumColors = sizeof(BtColors) / sizeof(ColorMapping);

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
  uint8_t btColorIndex;
  byte btSensorPort;
  uint8_t btSensorStopFunction;
  byte irChannel;
};