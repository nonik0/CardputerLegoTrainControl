#include <M5Cardputer.h>
#include "common.h"
#include "resources.h"

inline unsigned short interpolateColors(unsigned short color1, unsigned short color2, int percentage)
{
  if (percentage <= 0)
  {
    return color1;
  }
  else if (percentage >= 100)
  {
    return color2;
  }

  int red1 = (color1 >> 11) & 0x1F;
  int green1 = (color1 >> 5) & 0x3F;
  int blue1 = color1 & 0x1F;
  int red2 = (color2 >> 11) & 0x1F;
  int green2 = (color2 >> 5) & 0x3F;
  int blue2 = color2 & 0x1F;
  int interpolatedRed = (red1 * (100 - percentage) + red2 * percentage) / 100;
  int interpolatedGreen = (green1 * (100 - percentage) + green2 * percentage) / 100;
  int interpolatedBlue = (blue1 * (100 - percentage) + blue2 * percentage) / 100;
  return (unsigned short)((interpolatedRed << 11) | (interpolatedGreen << 5) | interpolatedBlue);
}

inline void drawRemoteTitle(M5Canvas *canvas, bool isLeftRemote, RemoteDevice activeRemote, int x, int y)
{
  const uint16_t *titleData;
  uint16_t titleWidth, titleHeight;

  switch (activeRemote)
  {
  case RemoteDevice::PoweredUp:
    titleData = (isLeftRemote) ? lpf2LeftTitle : lpf2RightTitle;
    titleWidth = (isLeftRemote) ? lpf2LeftTitleWidth : lpf2RightTitleWidth;
    titleHeight = (isLeftRemote) ? lpf2LeftTitleHeight : lpf2RightTitleHeight;
    break;
  case RemoteDevice::SBrick:
    titleData = (isLeftRemote) ? sbrickLeftTitle : sbrickRightTitle;
    titleWidth = (isLeftRemote) ? sbrickLeftTitleWidth : sbrickRightTitleWidth;
    titleHeight = (isLeftRemote) ? sbrickLeftTitleHeight : sbrickRightTitleHeight;
    break;
  case RemoteDevice::CircuitCubes:
    titleData = (isLeftRemote) ? circuitCubesLeftTitle : circuitCubesRightTitle;
    titleWidth = (isLeftRemote) ? circuitCubesLeftTitleWidth : circuitCubesRightTitleWidth;
    titleHeight = (isLeftRemote) ? circuitCubesLeftTitleHeight : circuitCubesRightTitleHeight;
    break;
  case RemoteDevice::PowerFunctionsIR:
    titleData = (isLeftRemote) ? powerFunctionsIrLeftTitle : powerFunctionsIrRightTitle;
    titleWidth = (isLeftRemote) ? powerFunctionsIrLeftTitleWidth : powerFunctionsIrRightTitleWidth;
    titleHeight = (isLeftRemote) ? powerFunctionsIrLeftTitleHeight : powerFunctionsIrRightTitleHeight;
    break;
  }

  canvas->pushImage(x - titleWidth / 2, y - titleHeight / 2, titleWidth, titleHeight, (uint16_t *)titleData, transparencyColor);
}

inline void drawActiveRemoteIndicator(M5Canvas *canvas, int x, int y, uint8_t activeRemoteLeft, uint8_t activeRemoteRight)
{
  // alignment middle_left
  int w = 8;
  int h = 12;
  y = y - h / 2;

  for (int i = 0; i < 4; i++)
  {
    if (i == activeRemoteLeft)
    {
      // canvas->fillRoundRect(x, y, w, h, 1, TFT_RED);
      canvas->drawRoundRect(x, y, w, h, 1, TFT_SILVER);
      canvas->floodFill(x + 1, y + 1, TFT_RED);
    }
    else if (i == activeRemoteRight)
    {
      // canvas->fillRoundRect(x, y, w, h, 1, TFT_BLUE);
      canvas->drawRoundRect(x, y, w, h, 1, TFT_SILVER);
      canvas->floodFill(x + 1, y + 1, TFT_BLUE);
    }
    else
    {
      canvas->drawRoundRect(x, y, w, h, 1, COLOR_LIGHTGRAY);
    }
    x += w + 2;
  }
}

inline void drawBatteryIndicator(M5Canvas *canvas, int x, int y, int batteryPct)
{
  int battw = 24;
  int batth = 11;

  int ya = y - batth / 2;

  // determine battery color and charge width from charge level
  int chgw = (battw - 2) * batteryPct / 100;
  uint16_t batColor = COLOR_TEAL;
  if (batteryPct < 100)
  {
    int r = ((100 - batteryPct) / 100.0) * 256;
    int g = (batteryPct / 100.0) * 256;
    batColor = canvas->color565(r, g, 0);
  }
  canvas->fillRoundRect(x, ya, battw, batth, 2, TFT_SILVER);
  canvas->fillRect(x - 2, y - 2, 2, 4, TFT_SILVER);
  canvas->fillRect(x + 1, ya + 1, battw - 2 - chgw, batth - 2, COLOR_DARKGRAY);  // 1px margin from outer battery
  canvas->fillRect(x + 1 + battw - 2 - chgw, ya + 1, chgw, batth - 2, batColor); // 1px margin from outer battery
}

inline void drawRssiSymbol(M5Canvas *canvas, int x, int y, bool init, int rssi)
{
  const uint8_t bar1 = 3, bar2 = 8, bar3 = 13;
  const uint8_t barW = 3;
  const uint8_t barY = y - bar3 / 2;
  const uint8_t barSpace = 2;

  if (!init)
    rssi = -1000;

  // easy way to "center" coords
  x -= 7;

  canvas->drawLine(x, barY, x, barY + bar3 - 1, TFT_SILVER);
  canvas->drawTriangle(x - 3, barY, x + 3, barY, x, barY + 3, TFT_SILVER);

  uint8_t barX = x + 4;
  (rssi > -100)
      ? canvas->fillRect(barX, barY + (bar3 - bar1), barW, bar1, COLOR_ORANGE)
      : canvas->drawRect(barX, barY + (bar3 - bar1), barW, bar1, TFT_SILVER);

  barX += barW + barSpace;
  (rssi > -80)
      ? canvas->fillRect(barX, barY + (bar3 - bar2), barW, bar2, COLOR_ORANGE)
      : canvas->drawRect(barX, barY + (bar3 - bar2), barW, bar2, TFT_SILVER);

  barX += barW + barSpace;
  (rssi > -60)
      ? canvas->fillRect(barX, barY + (bar3 - bar3), barW, bar3, COLOR_ORANGE)
      : canvas->drawRect(barX, barY + (bar3 - bar3), barW, bar3, TFT_SILVER);

  if (!init)
  {
    int x1 = x, x2 = x + 3 * (barSpace + barW);
    int y1 = barY, y2 = barY + bar3;

    for (int i = 0; i < barW; i++)
    {
      canvas->drawLine(x1 + i, y1, x2 - (barW - i), y2, TFT_RED);
      canvas->drawLine(x2 - (barW - i), y1, x1 + i, y2, TFT_RED);
    }
  }
}

inline void drawSensorColor(M5Canvas *canvas, int x, int y, Color color)
{
  const uint16_t FADE_RED = interpolateColors(COLOR_LIGHTGRAY, TFT_RED, 50);
  const uint16_t FADE_YELLOW = interpolateColors(COLOR_LIGHTGRAY, TFT_YELLOW, 50);
  const uint16_t FADE_GREEN = interpolateColors(COLOR_LIGHTGRAY, TFT_GREEN, 50);
  const uint16_t FADE_BLUE = interpolateColors(COLOR_LIGHTGRAY, TFT_BLUE, 50);

  int r = 5;
  int t = 2;
  canvas->fillArc(x, y, r, r + t, 180, 270, FADE_RED);
  canvas->fillArc(x, y, r, r + t, 270, 360, FADE_YELLOW);
  canvas->fillArc(x, y, r, r + t, 0, 90, FADE_GREEN);
  canvas->fillArc(x, y, r, r + t, 90, 180, FADE_BLUE);
  canvas->drawCircle(x, y, r - 1, TFT_SILVER);
  canvas->drawCircle(x, y, r + t + 1, TFT_SILVER);

  // show raw sensor color in middle if on
  if (color != Color::NONE)
  {
    canvas->floodFill(x, y, BtColors[color]);
  }
}

inline void drawPauseSymbol(M5Canvas *canvas, int x, int y)
{
  int w = 9;
  canvas->fillRect(x - w / 2, y - w / 2, w / 3, w, TFT_SILVER);
  canvas->fillRect(x - w / 2 + 2 * (w / 3), y - w / 2, w / 3, w, TFT_SILVER);
}

inline void drawMotorSymbol(M5Canvas *canvas, int x, int y, RemoteAction action)
{
  int w, h;
  switch (action)
  {
  case RemoteAction::SpdUp:
    w = 6;
    h = 9;
    canvas->fillTriangle(x, y - h / 2, x - w, y + h / 2, x + w, y + h / 2, TFT_SILVER);
    break;
  case RemoteAction::Brake:
    w = 9;
    canvas->fillRect(x - w / 2, y - w / 2, w, w, TFT_SILVER);
    break;
  case RemoteAction::SpdDn:
    w = 6;
    h = 9;
    canvas->fillTriangle(x, y + h / 2, x + w, y - h / 2, x - w, y - h / 2, TFT_SILVER);
    break;
  default:
    break;
  }
}

inline void drawSensorSymbol(M5Canvas *canvas, int x, int y, Color color, RemoteAction action, int spdupFunction, int8_t stopFunction, int8_t spddnFunction)
{
  switch (action)
  {
  case RemoteAction::SpdUp:
    if (spdupFunction >= 0)
      drawMotorSymbol(canvas, x, y, action);
    break;
  case RemoteAction::Brake:
    if (stopFunction == 0)
    {
      drawMotorSymbol(canvas, x, y, action);
    }
    else if (stopFunction > 0)
    {
      int gap = canvas->fontWidth();

      drawPauseSymbol(canvas, x - gap, y);

      // match muted background color of button
      int adjustedColor = interpolateColors(COLOR_LIGHTGRAY, BtColors[color], 50);

      canvas->setTextColor(TFT_SILVER);
      canvas->setTextDatum(middle_center);
      canvas->setTextSize(1);
      canvas->drawString(String(stopFunction), x + gap, y + 1);
    }
    break;
  case RemoteAction::SpdDn:
    if (spddnFunction >= 0)
      drawMotorSymbol(canvas, x, y, action);
    break;
  }
}

inline void drawPulseSymbol(M5Canvas *canvas, int x, int y, bool highPulse)
{
  int pulseW = 10;
  int pulseH = 10;

  int x1, x2, y1, y2;
  x1 = x - pulseW / 2;
  y1 = y - pulseH / 2;
  x2 = x + pulseW / 2;
  y2 = y + pulseH / 2;
  if (highPulse)
  {
    canvas->drawFastHLine(x1 - pulseW / 2 + 1, y2, pulseW / 2, TFT_SILVER);
    canvas->drawFastVLine(x1, y1, pulseH, TFT_SILVER);
    canvas->drawFastHLine(x1, y1, pulseW, TFT_SILVER);
    canvas->drawFastVLine(x2, y1, pulseH, TFT_SILVER);
    canvas->drawFastHLine(x2, y2, pulseW / 2, TFT_SILVER);
  }
  else
  {
    canvas->drawFastHLine(x1 - pulseW / 2 + 1, y1, pulseW / 2, TFT_SILVER);
    canvas->drawFastVLine(x1, y1, pulseH, TFT_SILVER);
    canvas->drawFastHLine(x1, y2, pulseW, TFT_SILVER);
    canvas->drawFastVLine(x2, y1, pulseH, TFT_SILVER);
    canvas->drawFastHLine(x2, y1, pulseW / 2, TFT_SILVER);
  }
}

inline void drawSwitchToggleSymbol(M5Canvas *canvas, int x, int y)
{
  canvas->pushImage(x - switchsymbolWidth / 2, y - switchSymbolHeight / 2, switchsymbolWidth, switchSymbolHeight, (uint16_t *)switchSymbol, transparencyColor);
}

inline void drawSwitchSymbol(M5Canvas *canvas, int x, int y, RemoteAction action, bool *portFunction)
{
  switch (action)
  {
  case RemoteAction::SpdUp:
    drawPulseSymbol(canvas, x, y, true);
    break;
  case RemoteAction::Brake:
    drawSwitchToggleSymbol(canvas, x, y);
    break;
  case RemoteAction::SpdDn:
    drawPulseSymbol(canvas, x, y, false);
    break;
  }
}

inline void drawIrChannelSymbol(M5Canvas *canvas, int x, int y, byte irChannel)
{
  canvas->setTextColor(TFT_SILVER);
  canvas->setTextDatum(middle_center);
  canvas->drawString(String(irChannel + 1), x + 1, y, &fonts::Font2);
}

inline void drawIrModeSymbol(M5Canvas *canvas, int x, int y, uint8_t irMode)
{
  canvas->fillCircle(x, y, 1, TFT_SILVER);

  if (irMode == 1 || irMode == 2)
  {
    canvas->fillTriangle(x, y + 5, x - 6, y + 9, x + 6, y + 9, TFT_SILVER);
  }
  else {
    canvas->drawTriangle(x, y + 5, x - 6, y + 9, x + 6, y + 9, TFT_SILVER);
  }

  if (irMode == 2 || irMode == 3)
  {
    canvas->fillArc(x, y, 4, 4, 140, 40, TFT_SILVER);
    canvas->fillArc(x, y, 7, 7, 140, 40, TFT_SILVER);
  }
}

inline void drawButtonSymbol(M5Canvas *canvas, Button &button, int x, int y, State &state)
{
  x += button.w / 2;
  y += button.h / 2;

  switch (button.action)
  {
  case RemoteAction::BtConnection:
    switch (button.device)
    {
    case RemoteDevice::PoweredUp:
      drawRssiSymbol(canvas, x, y, state.lpf2Connected, state.lpf2Rssi);
      break;
    case RemoteDevice::SBrick:
      drawRssiSymbol(canvas, x, y, state.sBrickConnected, state.sbrickRssi);
      break;
    case RemoteDevice::CircuitCubes:
      drawRssiSymbol(canvas, x, y, state.circuitCubesConnected, state.circuitCubesRssi);
      break;
    }
    break;
    
  case RemoteAction::Lpf2Color:
    drawSensorColor(canvas, x, y, state.lpf2SensorColor);
    break;
  case RemoteAction::IrChannel:
    drawIrChannelSymbol(canvas, x, y, state.irChannel);
    break;
  case RemoteAction::IrMode:
    drawIrModeSymbol(canvas, x, y, state.irMode);
    break;
  case RemoteAction::SpdUp:
  case RemoteAction::Brake:
  case RemoteAction::SpdDn:
    if (button.device == RemoteDevice::PoweredUp && button.port == state.lpf2SensorPort)
      drawSensorSymbol(canvas, x, y, state.lpf2SensorSpdUpColor, button.action, state.lpf2SensorSpdUpFunction, state.lpf2SensorStopFunction, state.lpf2SensorSpdDnFunction);
    else if (button.device == RemoteDevice::PowerFunctionsIR && state.irPortFunction[button.port])
      drawSwitchSymbol(canvas, x, y, button.action, state.irPortFunction);
    else
      drawMotorSymbol(canvas, x, y, button.action);
    break;
  default:
    break;
  }
}