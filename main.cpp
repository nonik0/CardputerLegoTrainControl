#include <Arduino.h>
#include <Lpf2Hub.h>
#include <M5Cardputer.h>
#include <M5StackUpdater.h>
#include <PowerFunctions.h>

#include "draw_helper.h"

#define IR_TX_PIN 44

M5Canvas canvas(&M5Cardputer.Display);

Lpf2Hub btTrainCtl;
byte btChA = (byte)PoweredUpHubPort::A;
byte btChB = (byte)PoweredUpHubPort::B;
short btChASpd = 0;
short btChBSpd = 0;
Color btColor = (Color)random(0, Color::NUM_COLORS);
// unsigned short BtColors[] = {TFT_RED, TFT_ORANGE, ...}; // RGB565 or 3x shorts?

PowerFunctions irTrainCtl(IR_TX_PIN, 0);
short irChannel = 0;
// short irRSpd;
// short irBSpd;

int batteryPct = M5Cardputer.Power.getBatteryLevel();
int batteryDelay = 0;

// define a bunch of display variables to make adjustments not a nightmare
int w = 240; // width
int h = 135; // height
int bw = 25; // button width
int om = 4;  // outer margin
int im = 2;  // inner margin

// header rectangle
int hx = om;
int hy = om;
int hw = w - 2 * om;
int hh = 20;
// main rectangle
int rx = om;
int ry = hy + hh + om;
int rw = hw;
int rh = h - hh - 3 * om;
// rows and cols in main rectangle
int r3 = h - 2 * om - im - bw;
int r2 = r3 - bw - im;
int r1 = r2 - bw - im;
int c3 = (w / 2) - 2 * om - bw;
int c2 = c3 - bw - im;
int c1 = c2 - bw - om;
int c4 = (w / 2) + 2 * om;
int c5 = c4 + bw + im;
int c6 = c5 + bw + om;
// bt rectangle
int btx = c1 - om;
int bty = ry + om;
int btw = 3 * bw + im + 3 * om;
int bth = rh - im - om;
// ir rectangle
int irx = c4 - om;
int iry = ry + om;
int irw = 3 * bw + im + 3 * om;
int irh = rh - im - om;

enum Channel
{
  BtA,
  BtB,
  IrR,
  IrB,
  BtCtl,
  IrCtl
};

struct Button
{
  char key;
  int x;
  int y;
  int w;
  int h;
  Channel channel;
  Action action;
  unsigned short color;
  bool pressed;
};

Button buttons[] = {
    {'`', c1, (r1 + r2) / 2, bw, bw, BtCtl, BtConnection, COLOR_LIGHTGRAY, false},
    {KEY_TAB, c1, (r2 + r3) / 2, bw, bw, BtCtl, BtColor, COLOR_LIGHTGRAY, false},
    {'e', c2, r1, bw, bw, BtA, SpeedUp, COLOR_LIGHTGRAY, false},
    {'s', c2, r2, bw, bw, BtA, Stop, COLOR_LIGHTGRAY, false},
    {'z', c2, r3, bw, bw, BtA, SpeedDn, COLOR_LIGHTGRAY, false},
    {'r', c3, r1, bw, bw, BtB, SpeedUp, COLOR_LIGHTGRAY, false},
    {'d', c3, r2, bw, bw, BtB, Stop, COLOR_LIGHTGRAY, false},
    {'x', c3, r3, bw, bw, BtB, SpeedDn, COLOR_LIGHTGRAY, false},
    {'i', c4, r1, bw, bw, IrR, SpeedUp, COLOR_LIGHTGRAY, false},
    {'j', c4, r2, bw, bw, IrR, Stop, COLOR_LIGHTGRAY, false},
    {'n', c4, r3, bw, bw, IrR, SpeedDn, COLOR_LIGHTGRAY, false},
    {'o', c5, r1, bw, bw, IrB, SpeedUp, COLOR_LIGHTGRAY, false},
    {'k', c5, r2, bw, bw, IrB, Stop, COLOR_LIGHTGRAY, false},
    {'m', c5, r3, bw, bw, IrB, SpeedDn, COLOR_LIGHTGRAY, false},
    {KEY_ENTER, c6, r2, bw, bw, IrCtl, IrChannel, COLOR_LIGHTGRAY, false},
};
uint8_t buttonCount = sizeof(buttons) / sizeof(Button);

SPIClass SPI2;
void checkForMenuBoot()
{
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isKeyPressed('a'))
  {
    SPI2.begin(M5.getPin(m5::pin_name_t::sd_spi_sclk),
               M5.getPin(m5::pin_name_t::sd_spi_miso),
               M5.getPin(m5::pin_name_t::sd_spi_mosi),
               M5.getPin(m5::pin_name_t::sd_spi_ss));
    while (!SD.begin(M5.getPin(m5::pin_name_t::sd_spi_ss), SPI2))
    {
      delay(500);
    }

    updateFromFS(SD, "/menu.bin");
    ESP.restart();
  }
}

void draw()
{
  canvas.fillSprite(m5gfx::ili9341_colors::BLACK);

  // draw background, sidebar, header graphics
  canvas.fillRoundRect(hx, hy, hw, hh, 8, COLOR_DARKGRAY);
  canvas.fillRoundRect(rx, ry, rw, rh, 8, COLOR_DARKGRAY);
  canvas.fillRoundRect(btx, bty, btw, bth, 6, COLOR_MEDGRAY);
  canvas.fillRoundRect(irx, iry, irw, irh, 6, COLOR_MEDGRAY);

  canvas.setTextColor(TFT_SILVER, COLOR_DARKGRAY);
  canvas.setTextDatum(middle_center);
  canvas.setTextSize(1.25);
  canvas.drawString("Lego Train Remote", w / 2, hy + hh / 2);

  canvas.setTextColor(TFT_SILVER, COLOR_MEDGRAY);
  canvas.setTextSize(1.8);
  canvas.drawString("BT", c1 + bw / 2, bty + bw / 2);
  canvas.drawString("IR", c6 + bw / 2, iry + bw / 2);

  draw_connected_indicator(&canvas, hx + om, hy + hh / 2, btTrainCtl.isConnected());
  draw_battery_indicator(&canvas, w - 34, hy + (hh / 2), batteryPct);

  // draw labels
  canvas.setTextColor(TFT_SILVER, COLOR_MEDGRAY);
  canvas.setTextDatum(bottom_center);
  canvas.setTextSize(1);
  canvas.drawString("CHA", c2 + bw / 2, r1 - 2);
  canvas.drawString("CHB", c3 + bw / 2, r1 - 2);
  canvas.drawString("RED", c4 + bw / 2, r1 - 2);
  canvas.drawString("BLUE", c5 + bw / 2, r1 - 2);

  // draw all layout for remote
  for (auto button : buttons)
  {
    unsigned short color = button.pressed ? TFT_ORANGE : button.color;
    canvas.fillRoundRect(button.x, button.y, button.w, button.h, 3, color);
    draw_button_symbol(&canvas, button.action, button.x + button.w / 2, button.y + button.h / 2);
  }

  // BT color button, TODO: move to draw_symbol??
  canvas.fillRoundRect(c1 + 4, (r2 + r3) / 2 + 4, bw - 8, bw - 8, 6, TFT_GREEN);

  // IR channel button, TODO: move to draw_symbol??
  canvas.setTextColor(TFT_SILVER, COLOR_LIGHTGRAY);
  canvas.setTextDatum(middle_center);
  canvas.setTextSize(1.5);
  canvas.drawString(String(irChannel + 1), c6 + bw / 2, r2 + bw / 2);

  canvas.pushSprite(0, 0);
}

void handle_button_press(Button *button)
{
  button->pressed = true;

  switch (button->action)
  {
  case BtConnection:
    if (!btTrainCtl.isConnected())
    {
      if (!btTrainCtl.isConnecting())
      {
        btTrainCtl.init();
      }
      if (btTrainCtl.isConnecting())
      {
        btTrainCtl.connectHub();
      }
    }
    else
    {
      btTrainCtl.shutDownHub();
      btChASpd = btChBSpd = 0;
    }
    break;
  case BtColor:
    if (!btTrainCtl.isConnected())
      break;
    btColor = (Color)((btColor + 1) % Color::NUM_COLORS);
    btTrainCtl.setLedColor(btColor);
    break;
  case IrChannel:
    irChannel = (irChannel + 1) % 4;
    break;
  case SpeedUp:
    switch (button->channel)
    {
    case BtA:
      if (!btTrainCtl.isConnected())
        break;
      btChASpd = min(100, btChASpd + 10);
      btTrainCtl.setBasicMotorSpeed(btChA, btChASpd);
      break;
    case BtB:
      if (!btTrainCtl.isConnected())
        break;
      btChBSpd = min(100, btChBSpd + 10);
      btTrainCtl.setBasicMotorSpeed(btChB, btChBSpd);
      break;
    case IrR:
      irTrainCtl.single_increment(PowerFunctionsPort::RED, irChannel);
      break;
    case IrB:
      irTrainCtl.single_increment(PowerFunctionsPort::BLUE, irChannel);
      break;
    }
    break;
  case Stop:
    switch (button->channel)
    {
    case BtA:
      if (!btTrainCtl.isConnected())
        break;
      btChASpd = 0;
      btTrainCtl.stopBasicMotor(btChA);
      break;
    case BtB:
      if (!btTrainCtl.isConnected())
        break;
      btChBSpd = 0;
      btTrainCtl.stopBasicMotor(btChB);
      break;
    case IrR:
      irTrainCtl.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::BRAKE, irChannel);
      break;
    case IrB:
      irTrainCtl.single_pwm(PowerFunctionsPort::BLUE, PowerFunctionsPwm::BRAKE, irChannel);
      break;
    }
    break;
  case SpeedDn:
    switch (button->channel)
    {
    case BtA:
      if (!btTrainCtl.isConnected())
        break;
      btChASpd = max(-100, btChASpd - 10);
      btTrainCtl.setBasicMotorSpeed(btChA, btChASpd);
      break;
    case BtB:
      if (!btTrainCtl.isConnected())
        break;
      btChBSpd = max(-100, btChBSpd - 10);
      btTrainCtl.setBasicMotorSpeed(btChB, btChBSpd);
      break;
    case IrR:
      irTrainCtl.single_decrement(PowerFunctionsPort::RED, irChannel);
      break;
    case IrB:
      irTrainCtl.single_decrement(PowerFunctionsPort::BLUE, irChannel);
      break;
    }
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);

  checkForMenuBoot();

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(100);
  canvas.createSprite(w, h);

  draw();
}

void loop()
{
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange())
  {
    for (int i = 0; i < buttonCount; i++)
    {
      if (M5Cardputer.Keyboard.isKeyPressed(buttons[i].key))
      {
        handle_button_press(&buttons[i]);
      }
    }

    draw();
    delay(60);
    for (int i = 0; i < buttonCount; i++)
      buttons[i].pressed = false;
    draw();
  }

  if (millis() > batteryDelay)
  {
    batteryDelay = millis() + 60000;
    batteryPct = M5Cardputer.Power.getBatteryLevel();
    draw();
  }
}