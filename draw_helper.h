#include <M5Cardputer.h>
#include "common.h"

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

inline void draw_rssi_indicator(M5Canvas *canvas, int x, int y, bool init, int rssi)
{
  const uint8_t bar1 = 2, bar2 = 5, bar3 = 8, bar4 = 11;
  const uint8_t barW = 3;
  const uint8_t barY = y - bar4 / 2;
  const uint8_t barSpace = 2;

  if (!init)
    rssi = -1000;

  canvas->drawLine(x, barY, x, barY + bar4 - 1, TFT_SILVER);
  canvas->drawTriangle(x - 3, barY, x + 3, barY, x, barY + 3, TFT_SILVER);

  uint8_t barX = x + 4;
  (rssi > -90)
      ? canvas->fillRect(barX, barY + (bar4 - bar1), barW, bar1, COLOR_ORANGE)
      : canvas->drawRect(barX, barY + (bar4 - bar1), barW, bar1, TFT_SILVER);

  barX += barW + barSpace;
  (rssi > -70)
      ? canvas->fillRect(barX, barY + (bar4 - bar2), barW, bar2, COLOR_ORANGE)
      : canvas->drawRect(barX, barY + (bar4 - bar2), barW, bar2, TFT_SILVER);

  barX += barW + barSpace;
  (rssi > -50)
      ? canvas->fillRect(barX, barY + (bar4 - bar3), barW, bar3, COLOR_ORANGE)
      : canvas->drawRect(barX, barY + (bar4 - bar3), barW, bar3, TFT_SILVER);

  barX += barW + barSpace;
  (rssi > -30)
      ? canvas->fillRect(barX, barY + (bar4 - bar4), barW, bar4, COLOR_ORANGE)
      : canvas->drawRect(barX, barY + (bar4 - bar4), barW, bar4, TFT_SILVER);

  if (!init)
  {
    int x1 = x + barW, x2 = x + 4 * (barSpace + barW) + barW;
    int y1 = barY, y2 = barY + bar4;

    for (int i = 0; i < barW; i++)
    {
      canvas->drawLine(x1 + i, y1, x2 - (barW - i), y2, TFT_RED);
      canvas->drawLine(x2 - (barW - i), y1, x1 + i, y2, TFT_RED);
    }
  }
}

inline void draw_rssi_symbol(M5Canvas *canvas, int x, int y, bool init, int rssi)
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
  (rssi > -90)
      ? canvas->fillRect(barX, barY + (bar3 - bar1), barW, bar1, COLOR_ORANGE)
      : canvas->drawRect(barX, barY + (bar3 - bar1), barW, bar1, TFT_SILVER);

  barX += barW + barSpace;
  (rssi > -70)
      ? canvas->fillRect(barX, barY + (bar3 - bar2), barW, bar2, COLOR_ORANGE)
      : canvas->drawRect(barX, barY + (bar3 - bar2), barW, bar2, TFT_SILVER);

  barX += barW + barSpace;
  (rssi > -50)
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

inline void draw_sensor_indicator(M5Canvas *canvas, int x, int y, Color color)
{
  // TODO distance
  int w = 18;
  int h = 11;
  y -= h / 2;

  if (color != Color::NONE)
  {
    canvas->fillRoundRect(x, y, w, h, 3, BtColors[color]);
    // canvas->setTextColor(TFT_SILVER, BtColors[i].rgb565);
    // canvas->setTextSize(1.0);
    // canvas->setTextDatum(middle_center);
    // canvas->drawString(LegoinoCommon::ColorStringFromColor(BtColors[i].color).c_str(), x + w / 2, y + h / 2);
  }

  canvas->drawRoundRect(x, y, w, h, 3, TFT_SILVER);
}

inline void draw_battery_indicator(M5Canvas *canvas, int x, int y, int batteryPct)
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

inline void draw_power_symbol(M5Canvas *canvas, int x, int y, bool isConnected)
{
  unsigned short color = isConnected ? TFT_BLUE : TFT_RED;
  canvas->fillArc(x, y, 8, 6, 0, 230, color);
  canvas->fillArc(x, y, 8, 6, 310, 359, color);
  canvas->fillRect(x - 1, y - 9, 3, 9, color);
}

inline void draw_bt_color_indicator(M5Canvas *canvas, int x, int y, Color color)
{
  int w = 17;
  if (color != Color::NONE)
    canvas->fillRoundRect(x - w / 2, y - w / 2, w, w, 6, BtColors[color]);
}

inline void draw_speedup_symbol(M5Canvas *canvas, int x, int y)
{
  int w = 6;
  int h = 9;
  canvas->fillTriangle(x, y - h / 2, x - w, y + h / 2, x + w, y + h / 2, TFT_SILVER);
}

inline void draw_stop_symbol(M5Canvas *canvas, int x, int y)
{
  int w = 9;
  canvas->fillRect(x - w / 2, y - w / 2, w, w, TFT_SILVER);
}

inline void draw_pause_symbol(M5Canvas *canvas, int x, int y)
{
  int w = 9;
  canvas->fillRect(x - w / 2, y - w / 2, w / 3, w, TFT_SILVER);
  canvas->fillRect(x - w / 2 + 2 * (w / 3), y - w / 2, w / 3, w, TFT_SILVER);
}

inline void draw_speeddn_symbol(M5Canvas *canvas, int x, int y)
{
  int w = 6;
  int h = 9;
  canvas->fillTriangle(x, y + h / 2, x + w, y - h / 2, x - w, y - h / 2, TFT_SILVER);
}

inline void draw_sensor_spdup_symbol(M5Canvas *canvas, int x, int y, Color color, int8_t spdupFunction)
{
  int w = 17;
  if (color != Color::NONE)
    draw_speedup_symbol(canvas, x, y);
  // canvas->fillRoundRect(x - w / 2, y - w / 2, w, w, 6, BtColors[color]);
}

inline void draw_sensor_stop_symbol(M5Canvas *canvas, int x, int y, Color color, int8_t stopFunction)
{
  int w = 17;
  if (color != Color::NONE)
  {
    if (stopFunction == 0)
    {
      draw_stop_symbol(canvas, x, y);
    }
    else if (stopFunction > 0)
    {
      int gap = canvas->fontWidth();

      draw_pause_symbol(canvas, x - gap, y);

      // match muted background color of button
      int adjustedColor = interpolateColors(COLOR_LIGHTGRAY, BtColors[color], 50);

      canvas->setTextColor(TFT_SILVER, adjustedColor);
      canvas->setTextDatum(middle_center);
      canvas->setTextSize(1);
      canvas->drawString(String(stopFunction), x + gap, y + 1);
    }
    else
    {
      // // NOOP/off symbol?
      // int x1 = x - w / 2, x2 = x + w / 2;
      // int y1 = y - w / 2, y2 = y + w / 2;
      // for (int i = 0; i < 2; i++)
      // {
      //   canvas->drawLine(x1 + i, y1 + i, x2 , y2, TFT_RED);
      //   canvas->drawLine(x2 - (barW - i), y1, x1 + i, y2, TFT_RED);
      // }
    }
  }
}

inline void draw_sensor_spddn_symbol(M5Canvas *canvas, int x, int y, Color color, int8_t spddnFunction)
{
  int w = 17;
  if (color != Color::NONE)
    draw_speeddn_symbol(canvas, x, y);
  // canvas->fillRoundRect(x - w / 2, y - w / 2, w, w, 6, BtColors[color]);
}

inline void draw_ir_channel_indicator(M5Canvas *canvas, int x, int y, byte irChannel, bool isPressed)
{
  canvas->setTextColor(TFT_SILVER, isPressed ? COLOR_ORANGE : COLOR_LIGHTGRAY);
  canvas->setTextDatum(middle_center);
  canvas->setTextSize(1.5);
  canvas->drawString(String(irChannel + 1), x, y);
}

inline void draw_button_symbol(M5Canvas *canvas, Button &button, int x, int y, State &state)
{
  x += button.w / 2;
  y += button.h / 2;

  switch (button.action)
  {
  case RemoteAction::BtConnection:
    switch (button.device)
    {
    case RemoteDevice::PoweredUpHub:
      //draw_power_symbol(canvas, x, y, state.btConnected);
      draw_rssi_symbol(canvas, x, y, state.lpfConnected, state.lpfRssi);
      break;
    case RemoteDevice::SBrick:
      draw_power_symbol(canvas, x, y, state.sBrickConnected);
      break;
    }
    break;
  case RemoteAction::BtColor:
    //draw_bt_color_indicator(canvas, x, y, state.btLedColor);
    break;
  case RemoteAction::IrChannel:
    draw_ir_channel_indicator(canvas, x, y, state.irChannel, button.pressed);
    break;
  case RemoteAction::SpdUp:
    button.device == RemoteDevice::PoweredUpHub &&button.port == state.lpfSensorPort
        ? draw_sensor_spdup_symbol(canvas, x, y, state.lpfSensorSpdUpColor, state.lpfSensorSpdUpFunction)
        : draw_speedup_symbol(canvas, x, y);
    break;
  case RemoteAction::Brake:
    button.device == RemoteDevice::PoweredUpHub &&button.port == state.lpfSensorPort
        ? draw_sensor_stop_symbol(canvas, x, y, state.lpfSensorStopColor, state.lpfSensorStopFunction)
        : draw_stop_symbol(canvas, x, y);
    break;
  case RemoteAction::SpdDn:
    button.device == RemoteDevice::PoweredUpHub &&button.port == state.lpfSensorPort
        ? draw_sensor_spddn_symbol(canvas, x, y, state.lpfSensorSpdDnColor, state.lpfSensorSpdDnFunction)
        : draw_speeddn_symbol(canvas, x, y);
    break;
  default:
    break;
  }
}