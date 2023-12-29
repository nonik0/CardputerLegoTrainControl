#include <Arduino.h>
#include <Lpf2Hub.h>
#include <M5Cardputer.h>
#include <M5StackUpdater.h>
#include <PowerFunctions.h>

#include "draw_helper.h"

#define IR_TX_PIN 44

M5Canvas canvas(&M5Cardputer.Display);
Lpf2Hub btTrainCtl;
PowerFunctions irTrainCtl(IR_TX_PIN, 0);
byte portA = (byte)PoweredUpHubPort::A;
byte portB = (byte)PoweredUpHubPort::B;
int portASpeed = 0;
int portBSpeed = 0;
unsigned int long nextUpdate = 0;
bool isLedRed = false;

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
int rh = h - hh - 3 * om; // 3 * bw + 2 * bm; + 2 * m;
// rows and cols in main rectangle
// int r1 = hy + hh + 2 * m;
// int r2 = r1 + bw + bm;
// int r3 = r2 + bw + bm;
int r3 = h - 2 * om - im - bw;
int r2 = r3 - bw - im;
int r1 = r2 - bw - im;
// int c1 = hx + 2 * m;
// int c2 = c1 + bw + bm;
// int c3 = c2 + bw + bm + 4 * m;
// int c4 = c3 + bw + bm;
int c2 = (w / 2) - 2 * om - bw;
int c3 = (w / 2) + 2 * om;
int c1 = c2 - bw - im;
int c4 = c3 + bw + im;

// bt rectangle
int btx = c1 - om;
int bty = ry + om;
int btw = 2 * bw + im + 2 * om;
int bth = rh - im - om;

// ir rectangle
int irx = c3 - om;
int iry = ry + om;
int irw = 2 * bw + im + 2 * om;
int irh = rh - im - om;

enum Channel
{
  BtA,
  BtB,
  IrR,
  IrB
};

struct Button
{
  char key;
  int x;
  int y;
  int w;
  int h;
  Channel channel;
  Symbol symbol;
  unsigned short color;
  bool pressed;
};

Button buttons[] = {
    {'e', c1, r1, bw, bw, BtA, SpeedUp, COLOR_LIGHTGRAY, false},
    {'s', c1, r2, bw, bw, BtA, Stop, COLOR_LIGHTGRAY, false},
    {'z', c1, r3, bw, bw, BtA, SpeedDn, COLOR_LIGHTGRAY, false},
    {'r', c2, r1, bw, bw, BtB, SpeedUp, COLOR_LIGHTGRAY, false},
    {'d', c2, r2, bw, bw, BtB, Stop, COLOR_LIGHTGRAY, false},
    {'x', c2, r3, bw, bw, BtB, SpeedDn, COLOR_LIGHTGRAY, false},
    {'i', c3, r1, bw, bw, IrR, SpeedUp, COLOR_LIGHTGRAY, false},
    {'j', c3, r2, bw, bw, IrR, Stop, COLOR_LIGHTGRAY, false},
    {'n', c3, r3, bw, bw, IrR, SpeedDn, COLOR_LIGHTGRAY, false},
    {'o', c4, r1, bw, bw, IrB, SpeedUp, COLOR_LIGHTGRAY, false},
    {'k', c4, r2, bw, bw, IrB, Stop, COLOR_LIGHTGRAY, false},
    {'m', c4, r3, bw, bw, IrB, SpeedDn, COLOR_LIGHTGRAY, false},
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
  canvas.drawString("BT", btx - 20, rx + rh / 2);
  canvas.drawString("IR", irx + irw + 20, rx + rh / 2);
  canvas.drawString("Lego Train Remote", w / 2, hy + hh / 2);

  draw_connected_indicator(&canvas, hx + om, hy + hh / 2, btTrainCtl.isConnected());
  draw_battery_indicator(&canvas, w - 34, hy + (hh / 2), batteryPct);

  // draw labels
  canvas.setTextColor(TFT_SILVER, COLOR_MEDGRAY);
  canvas.setTextDatum(bottom_center);
  canvas.setTextSize(1);
  canvas.drawString("CHA", c1 + bw / 2, r1 - 2);
  canvas.drawString("CHB", c2 + bw / 2, r1 - 2);
  canvas.drawString("RED", c3 + bw / 2, r1 - 2);
  canvas.drawString("BLUE", c4 + bw / 2, r1 - 2);

  // draw all layout for remote
  for (auto button : buttons)
  {
    unsigned short color = button.pressed ? TFT_ORANGE : button.color;
    canvas.fillRoundRect(button.x, button.y, button.w, button.h, 3, color);
    draw_button_symbol(&canvas, button.symbol, button.x + button.w / 2, button.y + button.h / 2);
  }

  canvas.pushSprite(0, 0);
}

void handle_button_press(Button* button)
{
  button->pressed = true;

  switch (button->channel)
  {
  case BtA:
    if (btTrainCtl.isConnected())
    {
      switch (button->symbol)
      {
      case SpeedUp:
        portASpeed = min(100, portASpeed + 10);
        btTrainCtl.setBasicMotorSpeed(portA, portASpeed);
        break;
      case Stop:
        portASpeed = 0;
        btTrainCtl.stopBasicMotor(portA);
        break;
      case SpeedDn:
        portASpeed = max(-100, portASpeed - 10);
        btTrainCtl.setBasicMotorSpeed(portA, portASpeed);
        break;
      }
    }
    break;
  case BtB:
    if (btTrainCtl.isConnected())
    {
      switch (button->symbol)
      {
      case SpeedUp:
        portBSpeed = min(100, portBSpeed + 10);
        btTrainCtl.setBasicMotorSpeed(portB, portBSpeed);
        break;
      case Stop:
        portBSpeed = 0;
        btTrainCtl.stopBasicMotor(portB);
        break;
      case SpeedDn:
        portBSpeed = max(-100, portBSpeed - 10);
        btTrainCtl.setBasicMotorSpeed(portB, portBSpeed);
        break;
      }
    }
    break;
  case IrR:
    switch (button->symbol)
    {
    case SpeedUp:
      irTrainCtl.single_increment(PowerFunctionsPort::RED);
      break;
    case Stop:
      irTrainCtl.single_pwm(PowerFunctionsPort::RED, PowerFunctionsPwm::BRAKE, 0);
      break;
    case SpeedDn:
      irTrainCtl.single_decrement(PowerFunctionsPort::RED);
      break;
    }
    break;
  case IrB:
    switch (button->symbol)
    {
    case SpeedUp:
      irTrainCtl.single_increment(PowerFunctionsPort::BLUE);
      break;
    case Stop:
      irTrainCtl.single_pwm(PowerFunctionsPort::BLUE, PowerFunctionsPwm::BRAKE, 0);
      break;
    case SpeedDn:
      irTrainCtl.single_decrement(PowerFunctionsPort::BLUE);
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
  // try to connect to BT train every second
  if (millis() > nextUpdate && !btTrainCtl.isConnected() && !btTrainCtl.isConnecting())
  {
    btTrainCtl.init();
    nextUpdate = millis() + 1000;
  }

  if (btTrainCtl.isConnecting())
  {
    btTrainCtl.connectHub();
    btTrainCtl.setLedColor(Color::GREEN);
    draw();
    // if (trainHub.isConnected())
    // {
    //   M5Cardputer.Display.printf("Connected to HUB\n%s\n%s",
    //                              trainHub.getHubAddress().toString().c_str(),
    //                              trainHub.getHubName().c_str());
    // }
    // else
    // {
    //   M5Cardputer.Display.printf("Failed to connect to HUB");
    // }
  }

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