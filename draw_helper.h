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

inline void draw_connected_indicator(M5Canvas *canvas, int x, int y, bool connected)
{
  int w = 32;
  int h = 14;
  y -= h / 2;
  unsigned short color = connected ? TFT_GREEN : TFT_RED;
  String text = connected ? "CON" : "D/C";
  canvas->fillRoundRect(x, y, w, h, 6, color);
  canvas->setTextColor(TFT_SILVER, COLOR_MEDGRAY);
  canvas->setTextSize(1.0);
  canvas->setTextColor(TFT_BLACK, color);
  canvas->setTextDatum(middle_center);
  canvas->drawString(text, x + w / 2, y + h / 2);
}

inline void draw_sensor_indicator(M5Canvas *canvas, int x, int y, Color color)
{
  // TODO distance
  int w = 32;
  int h = 14;
  y -= h / 2;

  if (color == Color::NONE)
    return;

  canvas->fillRoundRect(x, y, w, h, 6, color);
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
  canvas->fillRect(x + 1, ya + 1, battw - 2 - chgw, batth - 2, COLOR_DARKGRAY); // 1px margin from outer battery
  canvas->fillRect(x + 1 + battw - 2 - chgw, ya + 1, chgw, batth - 2, batColor); // 1px margin from outer battery
}

inline void draw_power_symbol(M5Canvas *canvas, int x, int y,
                              bool isConnected)
{
  unsigned short color = isConnected ? TFT_BLUE : TFT_RED;
  canvas->fillArc(x, y, 8, 6, 0, 230, color);
  canvas->fillArc(x, y, 8, 6, 310, 359, color);
  canvas->fillRect(x - 1, y - 9, 3, 9, color);
}

inline void draw_bt_color_indicator(M5Canvas *canvas, int x, int y, int btColorIndex)
{
  int w = 17;
  canvas->fillRoundRect(x - w / 2, y - w / 2, w, w, 6, BtColors[btColorIndex].rgb565);
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

inline void draw_speeddn_symbol(M5Canvas *canvas, int x, int y)
{
  int w = 6;
  int h = 9;
  canvas->fillTriangle(x, y + h / 2, x + w, y - h / 2, x - w, y - h / 2, TFT_SILVER);
}

inline void draw_color_symbol(M5Canvas *canvas, int x, int y, const int &color)
{
  int w = 17;
  canvas->fillRoundRect(x - w / 2, y - w / 2, w, w, 6, color);
}

inline void draw_ir_channel_indicator(M5Canvas *canvas, int x, int y, byte irChannel)
{
  canvas->setTextColor(TFT_SILVER, COLOR_LIGHTGRAY);
  canvas->setTextDatum(middle_center);
  canvas->setTextSize(1.5);
  canvas->drawString(String(irChannel + 1), x, y);
}

// inline void draw_button_symbol(M5Canvas *canvas, Action action, int x, int y,
//                                State *state)
inline void draw_button_symbol(M5Canvas *canvas, Button &button, State &state)
{
  int x = button.x + button.w / 2;
  int y = button.y + button.h / 2;

  switch (button.action)
  {
  case Action::BtConnection:
    draw_power_symbol(canvas, x, y, state.btConnected);
    break;
  case Action::BtColor:
    draw_bt_color_indicator(canvas, x, y, state.btColorIndex);
    break;
  case Action::IrChannel:
    draw_ir_channel_indicator(canvas, x, y, state.irChannel);
    break;
  case Action::SpdUp:
    button.port == state.btSensorPort
        ? draw_color_symbol(canvas, x, y, TFT_GREEN)
        : draw_speedup_symbol(canvas, x, y);
    break;
  case Action::Brake:
    button.port == state.btSensorPort
        ? draw_color_symbol(canvas, x, y, TFT_RED)
        : draw_stop_symbol(canvas, x, y);
    break;
  case Action::SpdDn:
    button.port == state.btSensorPort
        ? draw_color_symbol(canvas, x, y, TFT_BLUE)
        : draw_speeddn_symbol(canvas, x, y);
    break;
  default:
    break;
  }
}