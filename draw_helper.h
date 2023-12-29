#include <M5Cardputer.h>

// RBG565 colors
const unsigned short COLOR_BLACK = 0x18E3;
const unsigned short COLOR_DARKGRAY = 0x0861;
const unsigned short COLOR_MEDGRAY = 0x2104;
const unsigned short COLOR_LIGHTGRAY = 0x4208;
const unsigned short COLOR_DARKRED = 0x5800;
const unsigned short COLOR_ORANGE = 0xC401;
const unsigned short COLOR_TEAL = 0x07CC;
const unsigned short COLOR_BLUEGRAY = 0x0B0C;
const unsigned short COLOR_BLUE = 0x026E;
const unsigned short COLOR_PURPLE = 0x7075;

enum Symbol
{
  SpeedUp,
  SpeedDn,
  Stop
};

inline void draw_connected_indicator(M5Canvas* canvas, int x, int y, bool connected) {
  int w = 32;
  int h = 14;
  y -= h / 2;
  unsigned short color = connected ? TFT_GREEN : TFT_RED;
  String text = connected ? "CON" : "DC";
  canvas->fillRoundRect(x, y, w, h, 6, color);
  canvas->setTextColor(TFT_SILVER, COLOR_MEDGRAY);
  canvas->setTextSize(1.0);
  canvas->setTextColor(TFT_BLACK, color);
  canvas->setTextDatum(middle_center);
  canvas->drawString(text, x + w / 2, y + h / 2);
}

inline void draw_battery_indicator(M5Canvas* canvas, int x, int y, int batteryPct) {
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
  canvas->fillRect(x + 1, ya + 1, battw - 2 - chgw, batth - 2,
                  COLOR_DARKGRAY); // 1px margin from outer battery
  canvas->fillRect(x + 1 + battw - 2 - chgw, ya + 1, chgw, batth - 2,
                  batColor); // 1px margin from outer battery
}

inline void draw_speedup_symbol(M5Canvas* canvas, int x, int y)
{ 
  int w = 6;
  int h = 9;
  canvas->fillTriangle(x, y - h / 2, x - w, y + h / 2, x + w, y + h / 2, TFT_SILVER);
}

inline void draw_stop_symbol(M5Canvas* canvas, int x, int y)
{
  int w = 9;
  canvas->fillRect(x - w / 2, y - w / 2, w, w, TFT_SILVER);
}

inline void draw_speeddn_symbol(M5Canvas* canvas, int x, int y)
{
  int w = 6;
  int h = 9;
  canvas->fillTriangle(x, y + h / 2, x + w, y - h / 2, x - w, y - h / 2, TFT_SILVER);
}

inline void draw_button_symbol(M5Canvas* canvas, Symbol symbol, int x, int y)
{
  switch (symbol) {
    case Symbol::SpeedUp:
      draw_speedup_symbol(canvas, x, y);
      break;
    case Symbol::Stop:
      draw_stop_symbol(canvas, x, y);
      break;
    case Symbol::SpeedDn:
      draw_speeddn_symbol(canvas, x, y);
      break;
    default:
      break;
  }
}