#include <M5Cardputer.h>
#include "common.h"
#include "resources.h"

inline uint16_t interpolateColors(uint16_t color1, uint16_t color2, uint8_t percentage)
{
  if (percentage == 0)
    return color1;
  if (percentage >= 100)
    return color2;

  uint8_t red1 = (color1 >> 11) & 0x1F;  // 5 bits, max 31
  uint8_t green1 = (color1 >> 5) & 0x3F; // 6 bits, max 63
  uint8_t blue1 = color1 & 0x1F;         // 5 bits, max 31
  uint8_t red2 = (color2 >> 11) & 0x1F;
  uint8_t green2 = (color2 >> 5) & 0x3F;
  uint8_t blue2 = color2 & 0x1F;

  // max intermediate: 63 * 100 = 6300, fits in uint16_t (max 65535)
  uint8_t r = (red1 * (100 - percentage) + red2 * percentage) / 100;
  uint8_t g = (green1 * (100 - percentage) + green2 * percentage) / 100;
  uint8_t b = (blue1 * (100 - percentage) + blue2 * percentage) / 100;

  return (uint16_t)((r << 11) | (g << 5) | b);
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

inline void drawBatteryIndicator(M5Canvas *canvas, int x, int y, int batteryPct, int width = 24, int height = 11)
{
  int yc = y - height / 2;

  // determine battery color and charge width from charge level
  int chgw = (width - 2) * batteryPct / 100;
  uint16_t batColor = COLOR_TEAL;
  if (batteryPct < 100)
  {
    int r = ((100 - batteryPct) / 100.0) * 256;
    int g = (batteryPct / 100.0) * 256;
    batColor = canvas->color565(r, g, 0);
  }
  canvas->fillRoundRect(x, yc, width, height, 2, TFT_SILVER);
  if (height < 8)
  {
    canvas->fillRect(x - 1, y - 1, 1, 2, TFT_SILVER);
  }
  else
  {
    canvas->fillRect(x - 2, y - 2, 2, 4, TFT_SILVER);
  }
  canvas->fillRect(x + 1, yc + 1, width - 2 - chgw, height - 2, COLOR_DARKGRAY);  // 1px margin from outer battery
  canvas->fillRect(x + 1 + width - 2 - chgw, yc + 1, chgw, height - 2, batColor); // 1px margin from outer battery
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

inline void drawDirectionSymbol(M5Canvas *canvas, int x, int y, bool forward)
{
  int w = 9;
  int h = 6;
  if (forward)
    canvas->fillTriangle(x + w / 2, y, x - w / 2, y - h, x - w / 2, y + h, TFT_SILVER);
  else
    canvas->fillTriangle(x - w / 2, y, x + w / 2, y - h, x + w / 2, y + h, TFT_SILVER);
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

inline void drawSensorSymbol(M5Canvas *canvas, int x, int y, RemoteAction action, int8_t spdupFunction, int8_t stopFunction, int8_t spddnFunction)
{
  int gap = canvas->fontWidth();
  int8_t functionValue = action == RemoteAction::SpdUp   ? spdupFunction
                         : action == RemoteAction::SpdDn ? spddnFunction
                                                         : stopFunction;

  if (functionValue == 0)
  {
    drawMotorSymbol(canvas, x, y, action);
    return;
  }

  if (functionValue == -1)
    return; // disabled, draw nothing

  // symbol left, value right
  if (action == RemoteAction::Brake)
    stopFunction > 0 ? drawPauseSymbol(canvas, x - gap + 2, y)
                     : drawDirectionSymbol(canvas, x - gap + 2, y, false);
  else
    drawDirectionSymbol(canvas, x - gap + 2, y, true);

  canvas->setTextColor(TFT_SILVER);
  canvas->setTextDatum(middle_center);
  canvas->setTextSize(1);
  canvas->drawString(String(abs(functionValue)), x + gap + 1, y + 1);
}

inline void drawPulseSymbol(M5Canvas *canvas, int x, int y, bool highPulse, uint16_t color = TFT_SILVER)
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
    canvas->drawFastHLine(x1 - pulseW / 2 + 1, y2, pulseW / 2, color);
    canvas->drawFastVLine(x1, y1, pulseH, color);
    canvas->drawFastHLine(x1, y1, pulseW, color);
    canvas->drawFastVLine(x2, y1, pulseH, color);
    canvas->drawFastHLine(x2, y2, pulseW / 2, color);
  }
  else
  {
    canvas->drawFastHLine(x1 - pulseW / 2 + 1, y1, pulseW / 2, color);
    canvas->drawFastVLine(x1, y1, pulseH, color);
    canvas->drawFastHLine(x1, y2, pulseW, color);
    canvas->drawFastVLine(x2, y1, pulseH, color);
    canvas->drawFastHLine(x2, y1, pulseW / 2, color);
  }
}

inline void drawSwitchToggleSymbol(M5Canvas *canvas, int x, int y, uint8_t switchMode, uint16_t color = TFT_SILVER)
{
  switch (switchMode)
  {
  case 2:
    canvas->drawBitmap(
        x - switchSymbolWidth / 2,
        y - switchSymbolHeight / 2,
        switchAlternateSymbolBitmap,
        switchAlternateSymbolWidth,
        switchAlternateSymbolHeight,
        color);
    break;
  case 3:
    canvas->drawBitmap(
        x - switchSymbolWidth / 2,
        y - switchSymbolHeight / 2,
        switchRedirectSymbolBitmap,
        switchRedirectSymbolWidth,
        switchRedirectSymbolHeight,
        color);
    break;
  case 4:
    canvas->drawBitmap(
        x - switchSymbolWidth / 2,
        y - switchSymbolHeight / 2,
        switchRedirectLoopSymbolBitmap,
        switchRedirectLoopSymbolWidth,
        switchRedirectLoopSymbolHeight,
        color);
    break;
  default:
    canvas->drawBitmap(
        x - switchSymbolWidth / 2,
        y - switchSymbolHeight / 2,
        switchSymbolBitmap,
        switchSymbolWidth,
        switchSymbolHeight,
        color);
  }
}

inline void drawSwitchSymbol(M5Canvas *canvas, int x, int y, RemoteAction action, uint8_t switchMode, uint8_t switchDetection)
{
  const uint16_t detectionColor = interpolateColors(TFT_SILVER, TFT_YELLOW, 50);
  auto color = [&](uint8_t maskIdx) -> uint16_t
  {
    static constexpr uint8_t detectionMask[] = {
        0b00111100,  // spdup: fork direction: passing and exiting, merge direction: entering and passing => 2,3,4,5
        0b01101100,  // brake: fork direction: passing and exiting, merge direction: passing and exiting => 2,3,5,6
        0b01100110}; // spddn: fork direction: entering and passing, merge direction: passing and exiting => 1,2,5,6
    return (detectionMask[maskIdx] >> switchDetection) & 1 ? detectionColor : TFT_SILVER;
  };

  switch (action)
  {
  case RemoteAction::SpdUp:
    drawPulseSymbol(canvas, x, y, true, color(0));
    break;
  case RemoteAction::Brake:
    drawSwitchToggleSymbol(canvas, x, y, switchMode, color(1));
    break;
  case RemoteAction::SpdDn:
    drawPulseSymbol(canvas, x, y, false, color(2));
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
  else
  {
    canvas->drawTriangle(x, y + 5, x - 6, y + 9, x + 6, y + 9, TFT_SILVER);
  }

  if (irMode == 2 || irMode == 3)
  {
    canvas->fillArc(x, y, 4, 4, 140, 40, TFT_SILVER);
    canvas->fillArc(x, y, 7, 7, 140, 40, TFT_SILVER);
  }
}

inline void drawSBrickSensorReading(M5Canvas *canvas, int x, int y, RemoteAction action, bool isMotionSensor, float voltage, float neutralV)
{
  char deltaV[6];
  uint8_t gap = 4;
  int sx;
  y += 1;
  x += 3;

  canvas->setTextDatum(middle_center);
  String name = (isMotionSensor) ? "Motion" : "Tilt";

  switch (action)
  {
  case RemoteAction::SpdUp:
    canvas->drawString(name, x, y - gap, &fonts::TomThumb);
    canvas->drawString("Sensr", x, y + gap, &fonts::TomThumb);
    break;
  case RemoteAction::Brake:
    canvas->drawLine(x - 9, y - gap + 1, x - 4, y - gap - 3, TFT_SILVER);
    canvas->drawLine(x - 4, y - gap - 3, x, y - gap, TFT_SILVER);
    canvas->drawLine(x, y - gap - 1, x + 4, y - gap - 5, TFT_SILVER);
    canvas->drawString(String(voltage, 2) + "V", x, y + gap, &fonts::TomThumb);
    break;
  case RemoteAction::SpdDn:
    canvas->drawTriangle(x - 6, y - gap + 1, x + 1, y - gap + 1, x - 2, y - gap - 5, TFT_SILVER);
    sprintf(deltaV, "%d%%", (int)(((voltage - neutralV) / neutralV) * 100));
    canvas->drawString(String(deltaV), x, y + gap, &fonts::TomThumb);
    break;
  }
}

inline void drawButtonKeyMapping(M5Canvas *canvas, Button &button, int x, int y, uint8_t key)
{
  x += button.w / 2 + 1;
  y += button.h / 2;

  canvas->setTextDatum(middle_center);

  String keyStr;

  switch (key)
  {
  case KEY_TAB:
    keyStr = "tab";
    break;
  case KEY_BACKSPACE:
    keyStr = "del";
    break;
  case '`':
    keyStr = "esc";
    break;
  default:
    keyStr = String((char)key);
  }

  canvas->drawString(keyStr, x, y, &fonts::Font2);
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
      drawSensorSymbol(canvas, x, y, button.action, state.lpf2SensorSpdUpFunction, state.lpf2SensorStopFunction, state.lpf2SensorSpdDnFunction);
    else if (button.device == RemoteDevice::PowerFunctionsIR && state.irPortFunction[button.port])
      drawSwitchSymbol(canvas, x, y, button.action,
                       (button.port == (uint8_t)state.irSwitchPort) ? state.irSwitchMode : 0,
                       (button.port == (uint8_t)state.irSwitchPort) ? state.irSwitchDetection : 0);
    else if (button.device == RemoteDevice::SBrick && button.port == state.sbrickMotionSensorPort)
      drawSBrickSensorReading(canvas, x, y, button.action, true, state.sbrickMotionSensorV, state.sbrickMotionSensorNeutralV);
    else if (button.device == RemoteDevice::SBrick && button.port == state.sbrickTiltSensorPort)
      drawSBrickSensorReading(canvas, x, y, button.action, false, state.sbrickTiltSensorV, state.sbrickTiltSensorNeutralV);
    else
      drawMotorSymbol(canvas, x, y, button.action);
    break;
  default:
    break;
  }
}