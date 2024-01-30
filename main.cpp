#include <Arduino.h>
#include <Lpf2Hub.h>
#include <M5Cardputer.h>
#include <M5StackUpdater.h>
#include <PowerFunctions.h>

#include "common.h"
#include "draw_helper.h"

#define IR_TX_PIN 44
#define NO_SENSOR_FOUND 0xFF

unsigned short COLOR_GREYORANGEDIM = interpolateColors(COLOR_LIGHTGRAY, COLOR_ORANGE, 25);
unsigned short COLOR_GREYORANGEBRIGHT = interpolateColors(COLOR_LIGHTGRAY, COLOR_ORANGE, 75);

M5Canvas canvas(&M5Cardputer.Display);
uint8_t brightness = 100;
unsigned long lastKeyPressMillis = 0;
const unsigned long KeyboardDebounce = 200;

// bluetooth train control constants
const byte BtPortA = (byte)PoweredUpHubPort::A;
const byte BtPortB = (byte)PoweredUpHubPort::B;
const int BtMaxSpeed = 100;
const short BtSpdInc = 10; // TODO configurable
const short BtConnWait = 100;
const int BtInactiveTimeoutMs = 5 * 60 * 1000;
const short BtDistanceStopThreshold = 100; // when train is "picked up"

// hub state
Lpf2Hub btTrainCtl;
bool btInit = false;
volatile int btRssi = -1000;
short btPortASpd = 0;
short btPortBSpd = 0;
unsigned long btDisconnectDelay;    // debounce disconnects
unsigned long btButtonDebounce = 0; // debounce button presses
volatile Color btLedColor = Color::ORANGE;
unsigned short btLedColorDelay = 0;
unsigned long btLastAction = 0; // track for auto-disconnect
volatile Action btAutoAction = NoAction;

// color/distance sensor
bool btSensorInit = false;
byte btSensorPort = NO_SENSOR_FOUND; // set to A or B if detected
byte btMotorPort = NO_SENSOR_FOUND;  // set to opposite of sensor port if detected
short distanceMovingAverage = 0;
volatile Color btSensorColor = Color::NONE; // detected color by sensor
unsigned long btSensorDebounce = 0;         // debounce sensor color changes
Color btSensorIgnoreColors[] = {Color::BLACK, Color::BLUE};
Color btSensorSpdUpColor = Color::GREEN;
Color btSensorStopColor = Color::RED;
Color btSensorSpdDnColor = Color::YELLOW;
uint8_t btSensorSpdUpFunction = 0; // TBD
uint8_t btSensorStopFunction = 0;  // 0=infinite, >0=wait time in seconds
uint8_t btSensorSpdDnFunction = 0; // TBD
unsigned long btSensorStopDelay = 0;
short btSensorStopSavedSpd = 0; // saved speed before stopping
volatile bool redraw = false;

// IR train control state
const PowerFunctionsPort IrPortR = PowerFunctionsPort::RED;
const PowerFunctionsPort IrPortB = PowerFunctionsPort::BLUE;
const int IrMaxSpeed = 105;
const short IrSpdInc = 15; // hacky but to match 7 levels
PowerFunctions irTrainCtl(IR_TX_PIN);
bool irTrackState = false;
byte irChannel = 0;
short irPortRSpd = 0;
short irPortBSpd = 0;

// system bar state
int batteryPct = M5Cardputer.Power.getBatteryLevel();
unsigned long updateDelay = 0;

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

Button buttons[] = {
    {'`', c1, (r1 + r2) / 2, bw, bw, None, BtConnection, COLOR_LIGHTGRAY, false},
    {KEY_TAB, c1, (r2 + r3) / 2, bw, bw, None, BtColor, COLOR_LIGHTGRAY, false},
    {'e', c2, r1, bw, bw, BtA, SpdUp, COLOR_LIGHTGRAY, false},
    {'s', c2, r2, bw, bw, BtA, Brake, COLOR_LIGHTGRAY, false},
    {'z', c2, r3, bw, bw, BtA, SpdDn, COLOR_LIGHTGRAY, false},
    {'r', c3, r1, bw, bw, BtB, SpdUp, COLOR_LIGHTGRAY, false},
    {'d', c3, r2, bw, bw, BtB, Brake, COLOR_LIGHTGRAY, false},
    {'x', c3, r3, bw, bw, BtB, SpdDn, COLOR_LIGHTGRAY, false},
    {'i', c4, r1, bw, bw, IrR, SpdUp, COLOR_LIGHTGRAY, false},
    {'j', c4, r2, bw, bw, IrR, Brake, COLOR_LIGHTGRAY, false},
    {'n', c4, r3, bw, bw, IrR, SpdDn, COLOR_LIGHTGRAY, false},
    {'o', c5, r1, bw, bw, IrB, SpdUp, COLOR_LIGHTGRAY, false},
    {'k', c5, r2, bw, bw, IrB, Brake, COLOR_LIGHTGRAY, false},
    {'m', c5, r3, bw, bw, IrB, SpdDn, COLOR_LIGHTGRAY, false},
    {KEY_BACKSPACE, 0, 0, 0, 0, None, IrTrackState, COLOR_LIGHTGRAY, false},
    {KEY_ENTER, c6, r2, bw, bw, None, IrChannel, COLOR_LIGHTGRAY, false},
};
uint8_t buttonCount = sizeof(buttons) / sizeof(Button);

bool isIgnoredColor(Color color)
{
  for (int i = 0; i < sizeof(btSensorIgnoreColors) / sizeof(Color); i++)
  {
    if (color == btSensorIgnoreColors[i])
      return true;
  }
  return false;
}

inline void resumeTrainMotion()
{
  btSensorStopDelay = 0;
  btAutoAction = btSensorStopSavedSpd > 0 ? SpdUp : SpdDn;
  short btSpdAdjust = btSensorStopSavedSpd > 0 ? -BtSpdInc : BtSpdInc;
  if (btMotorPort == BtPortA)
    btPortASpd = btSensorStopSavedSpd + btSpdAdjust;
  else
    btPortBSpd = btSensorStopSavedSpd + btSpdAdjust;
}

void buttonCallback(void *hub, HubPropertyReference hubProperty, uint8_t *pData)
{
  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (hubProperty != HubPropertyReference::BUTTON || millis() < btButtonDebounce)
  {
    return;
  }

  btButtonDebounce = millis() + 200;
  ButtonState buttonState = trainCtl->parseHubButton(pData);

  if (buttonState == ButtonState::PRESSED)
  {
    if (btSensorStopDelay > 0)
    {
      resumeTrainMotion();

      for (int i = 0; i < buttonCount; i++)
      {
        if (buttons[i].action == BtColor)
        {
          buttons[i].pressed = true;
          break;
        }
      }
    }
    else
    {
      btAutoAction = BtColor;
    }
  }
}

void rssiCallback(void *hub, HubPropertyReference hubProperty, uint8_t *pData)
{
  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (hubProperty != HubPropertyReference::RSSI)
  {
    return;
  }

  int rssi = trainCtl->parseRssi(pData);

  if (rssi == btRssi)
  {
    return;
  }

  btRssi = rssi;
  redraw = true;
}

void sensorCallback(void *hub, byte sensorPort, DeviceType deviceType, uint8_t *pData)
{
  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (deviceType != DeviceType::COLOR_DISTANCE_SENSOR)
  {
    return;
  }

  int distance = trainCtl->parseDistance(pData);
  Color color = static_cast<Color>(trainCtl->parseColor(pData));

  // track moving average of distance to random noisy spikes, turn off motor if not on track
  distanceMovingAverage = (distanceMovingAverage * 3 + distance) >> 2; // I bet compiler does this anyway for dividing by 4
  if (distanceMovingAverage > BtDistanceStopThreshold &&
      (btMotorPort == BtPortA && btPortASpd != 0 ||
       btMotorPort == BtPortB && btPortBSpd != 0))
  {
    log_w("off track, distance moving avg: %d", distance);

    btAutoAction = Brake;
    btSensorStopDelay = 0;
    btSensorStopSavedSpd = 0;
    return;
  }

  // filter noise
  if (color == btSensorColor || millis() < btSensorDebounce)
  {
    return;
  }

  btSensorColor = color;
  btSensorDebounce = millis() + 200;
  redraw = true;

  // ignore "ground" or noisy colors
  if (isIgnoredColor(color))
  {
    return;
  }

  btLedColor = color;
  trainCtl->setLedColor(color);

  // trigger actions for specific colors
  Action action;
  if (color == btSensorSpdUpColor)
  {
    log_i("bt action: spdup");
    btAutoAction = SpdUp;
  }
  else if (color == btSensorStopColor)
  {
    // resumed by calling resumeTrainMotion() after delay
    log_i("bt action: brake");
    btAutoAction = Brake;
    btSensorStopDelay = millis() + btSensorStopFunction * 1000;
    btSensorStopSavedSpd = btMotorPort == BtPortA ? btPortASpd : btPortBSpd;
  }
  else if (color == btSensorSpdDnColor)
  {
    log_i("bt action: spddn");
    btAutoAction = SpdDn;
  }
  else
  {
    return;
  }

  // also show press for sensor button
  for (int i = 0; i < buttonCount; i++)
  {
    if (buttons[i].action == btAutoAction && buttons[i].port == btSensorPort)
    {
      buttons[i].pressed = true;
      break;
    }
  }
}

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

void btConnectionToggle()
{
  if (!btInit && !btTrainCtl.isConnected())
  {
    if (!btTrainCtl.isConnecting())
    {
      btTrainCtl.init();
    }

    unsigned long exp = millis() + BtConnWait;
    while (!btTrainCtl.isConnecting() && exp > millis())
      ;

    if (btTrainCtl.isConnecting() && btTrainCtl.connectHub())
    {
      btInit = true;
      btLedColor = Color::NONE;
      btTrainCtl.activateHubPropertyUpdate(HubPropertyReference::BUTTON, buttonCallback);
      btTrainCtl.activateHubPropertyUpdate(HubPropertyReference::RSSI, rssiCallback);
    }

    btDisconnectDelay = millis() + 500;
  }
  else if (btDisconnectDelay < millis())
  {
    btTrainCtl.shutDownHub();
    delay(200);
    btInit = false;
    btSensorInit = false;
    btPortASpd = btPortBSpd = 0;
    btMotorPort = NO_SENSOR_FOUND;
    btSensorPort = NO_SENSOR_FOUND;
    btSensorSpdUpColor = Color::GREEN;
    btSensorStopColor = Color::RED;
    btSensorSpdDnColor = Color::YELLOW;
    btSensorStopFunction = 0;
    btSensorStopDelay = 0;
  }
}

void handle_button_press(Button *button)
{
  button->pressed = true;

  PowerFunctionsPwm pwm;
  switch (button->action)
  {
  case BtConnection:
    btConnectionToggle();
    break;
  case BtColor:
    // function like hub button
    if (btSensorStopDelay > 0)
    {
      resumeTrainMotion();
    }
    else
    {
      // can change colors while not connected to choose initial color
      btLedColor = (btLedColor == Color(1))
                       ? Color(Color::NUM_COLORS - 1)
                       : (Color)(btLedColor - 1);
      log_i("bt color: %d", btLedColor);
      if (btTrainCtl.isConnected())
        btTrainCtl.setLedColor(btLedColor);
    }
    break;
  case IrChannel:
    irChannel = (irChannel + 1) % 4;
    break;
  case IrTrackState:
    irTrackState = !irTrackState;
    irPortRSpd = irPortBSpd = 0;
    break;
  case SpdUp:
    switch (button->port)
    {
    case BtA:
      if (!btTrainCtl.isConnected())
        break;
      if (btSensorPort == BtPortA)
      {
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN))
        {
          do
          {
            btSensorSpdUpColor = (btSensorSpdUpColor == Color::BLACK)
                                     ? Color(Color::NUM_COLORS - 1)
                                     : (Color)(btSensorSpdUpColor - 1);
          } while (isIgnoredColor(btSensorSpdUpColor) || btSensorSpdUpColor == btSensorSpdDnColor || btSensorSpdUpColor == btSensorStopColor);
        }
      }
      else
      {
        btPortASpd = min(BtMaxSpeed, btPortASpd + BtSpdInc);
        btTrainCtl.setBasicMotorSpeed(BtPortA, btPortASpd);
      }
      break;
    case BtB:
      if (!btTrainCtl.isConnected())
        break;
      if (btSensorPort == BtPortB)
      {
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN))
        {
          do
          {
            btSensorSpdUpColor = (btSensorSpdUpColor == Color::BLACK)
                                     ? Color(Color::NUM_COLORS - 1)
                                     : (Color)(btSensorSpdUpColor - 1);
          } while (isIgnoredColor(btSensorSpdUpColor) || btSensorSpdUpColor == btSensorSpdDnColor || btSensorSpdUpColor == btSensorStopColor);
        }
      }
      else
      {
        btPortBSpd = min(BtMaxSpeed, btPortBSpd + BtSpdInc);
        btTrainCtl.setBasicMotorSpeed(BtPortB, btPortBSpd);
      }
      break;
    case IrR:
      irPortRSpd = min(IrMaxSpeed, irPortRSpd + IrSpdInc);
      if (irTrackState)
      {
        pwm = irTrainCtl.speedToPwm(irPortRSpd);
        irTrainCtl.single_pwm(IrPortR, pwm, irChannel);
      }
      else
      {
        irTrainCtl.single_increment(IrPortR, irChannel);
      }
      break;
    case IrB:
      irPortBSpd = min(IrMaxSpeed, irPortBSpd + IrSpdInc);
      if (irTrackState)
      {
        pwm = irTrainCtl.speedToPwm(irPortBSpd);
        irTrainCtl.single_pwm(IrPortB, pwm, irChannel);
      }
      else
      {
        irTrainCtl.single_increment(IrPortB, irChannel);
      }
      break;
    }
    break;
  case Brake:
    switch (button->port)
    {
    case BtA:
      if (!btTrainCtl.isConnected())
        break;
      if (btSensorPort == BtPortA)
      {
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN))
        {
          do
          {
            btSensorStopColor = (btSensorStopColor == Color::BLACK)
                                    ? Color(Color::NUM_COLORS - 1)
                                    : (Color)(btSensorStopColor - 1);
          } while (isIgnoredColor(btSensorStopColor) || btSensorStopColor == btSensorSpdUpColor || btSensorStopColor == btSensorSpdDnColor);
        }
        else
        {
          if (btSensorStopFunction == 0)
            btSensorStopFunction = 2;
          else if (btSensorStopFunction == 2)
            btSensorStopFunction = 5;
          else if (btSensorStopFunction == 5)
            btSensorStopFunction = 10;
          else
            btSensorStopFunction = 0;
        }
      }
      else
      {
        btPortASpd = 0;
        btTrainCtl.stopBasicMotor(BtPortA);
      }
      break;
    case BtB:
      if (!btTrainCtl.isConnected())
        break;
      if (btSensorPort == BtPortB)
      {
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN))
        {
          do
          {
            btSensorStopColor = (btSensorStopColor == Color::BLACK)
                                    ? Color(Color::NUM_COLORS - 1)
                                    : (Color)(btSensorStopColor - 1);
          } while (isIgnoredColor(btSensorStopColor) || btSensorStopColor == btSensorSpdUpColor || btSensorStopColor == btSensorSpdDnColor);
        }
        else
        {
          if (btSensorStopFunction == 0)
            btSensorStopFunction = 2;
          else if (btSensorStopFunction == 2)
            btSensorStopFunction = 5;
          else if (btSensorStopFunction == 5)
            btSensorStopFunction = 10;
          else
            btSensorStopFunction = 0;
        }
      }
      else
      {
        btPortBSpd = 0;
        btTrainCtl.stopBasicMotor(BtPortB);
      }
      break;
    case IrR:
      irPortRSpd = 0;
      irTrainCtl.single_pwm(IrPortR, PowerFunctionsPwm::BRAKE, irChannel);
      break;
    case IrB:
      irPortBSpd = 0;
      irTrainCtl.single_pwm(IrPortB, PowerFunctionsPwm::BRAKE, irChannel);
      break;
    }
    break;
  case SpdDn:
    switch (button->port)
    {
    case BtA:
      if (!btTrainCtl.isConnected())
        break;
      if (btSensorPort == BtPortA)
      {
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN))
        {
          do
          {
            btSensorSpdDnColor = (btSensorSpdDnColor == Color::BLACK)
                                     ? Color(Color::NUM_COLORS - 1)
                                     : (Color)(btSensorSpdDnColor - 1);
          } while (isIgnoredColor(btSensorSpdDnColor) || btSensorSpdDnColor == btSensorSpdUpColor || btSensorSpdDnColor == btSensorStopColor);
        }
      }
      else
      {
        btPortASpd = max(-BtMaxSpeed, btPortASpd - BtSpdInc);
        btTrainCtl.setBasicMotorSpeed(BtPortA, btPortASpd);
      }
      break;
    case BtB:
      if (!btTrainCtl.isConnected())
        break;
      if (btSensorPort == BtPortB)
      {
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN))
        {
          do
          {
            btSensorSpdDnColor = (btSensorSpdDnColor == Color::BLACK)
                                     ? Color(Color::NUM_COLORS - 1)
                                     : (Color)(btSensorSpdDnColor - 1);
          } while (isIgnoredColor(btSensorSpdDnColor) || btSensorSpdDnColor == btSensorSpdUpColor || btSensorSpdDnColor == btSensorStopColor);
        }
      }
      else
      {
        btPortBSpd = max(-BtMaxSpeed, btPortBSpd - BtSpdInc);
        btTrainCtl.setBasicMotorSpeed(BtPortB, btPortBSpd);
      }
      break;
    case IrR:
      if (irTrackState)
      {
        irPortRSpd = max(-IrMaxSpeed, irPortRSpd - IrSpdInc);
        pwm = irTrainCtl.speedToPwm(irPortRSpd);
        irTrainCtl.single_pwm(IrPortR, pwm, irChannel);
      }
      else
      {
        irTrainCtl.single_decrement(IrPortR, irChannel);
      }
      break;
    case IrB:
      if (irTrackState)
      {
        irPortBSpd = max(-IrMaxSpeed, irPortBSpd - IrSpdInc);
        pwm = irTrainCtl.speedToPwm(irPortBSpd);
        irTrainCtl.single_pwm(IrPortB, pwm, irChannel);
      }
      else
      {
        irTrainCtl.single_decrement(IrPortB, irChannel);
      }
      break;
    }
    break;
  }
}

unsigned short get_button_color(Button *button)
{
  if (button->pressed)
  {
    return COLOR_ORANGE;
  }

  // TODO: improve
  // if (btSensorPort == button->port)
  // {
  //   switch (button->action)
  //   {
  //   case SpdUp:
  //     return interpolateColors(COLOR_LIGHTGRAY, BtColors[btSensorSpdUpColor], 25);
  //   case Brake:
  //     return interpolateColors(COLOR_LIGHTGRAY, BtColors[btSensorStopColor], 25);
  //   case SpdDn:
  //     return interpolateColors(COLOR_LIGHTGRAY, BtColors[btSensorSpdDnColor], 25);
  //   default:
  //     return button->color;
  //   }
  // }

  // if (button->action == BtColor)
  // {
  //   return interpolateColors(COLOR_LIGHTGRAY, BtColors[btLedColor], 25);
  // }

  if (button->action == Brake)
  {
    if ((button->port == BtA && btPortASpd == 0) ||
        (button->port == BtB && btPortBSpd == 0) ||
        (button->port == IrR && irTrackState && irPortRSpd == 0) ||
        (button->port == IrB && irTrackState && irPortBSpd == 0))
    {
      return COLOR_GREYORANGEDIM;
    }
  }

  if (button->action == SpdUp || button->action == SpdDn)
  {
    int speed = 0;
    switch (button->port)
    {
    case BtA:
      speed = btPortASpd;
      break;
    case BtB:
      speed = btPortBSpd;
      break;
    case IrR:
      speed = irTrackState ? irPortRSpd : 0;
      break;
    case IrB:
      speed = irTrackState ? irPortBSpd : 0;
      break;
    }

    if ((button->action == SpdUp && speed > 0) ||
        (button->action == SpdDn && speed < 0))
    {
      return interpolateColors(COLOR_GREYORANGEDIM, COLOR_GREYORANGEBRIGHT,
                               abs(speed));
    }
  }

  return button->color;
}

void draw()
{
  canvas.fillSprite(m5gfx::ili9341_colors::BLACK);

  // draw background, sidebar, header graphics
  canvas.fillRoundRect(hx, hy, hw, hh, 6, COLOR_DARKGRAY);
  canvas.fillRoundRect(rx, ry, rw, rh, 6, COLOR_DARKGRAY);
  canvas.fillRoundRect(btx, bty, btw, bth, 6, COLOR_MEDGRAY);
  canvas.fillRoundRect(irx, iry, irw, irh, 6, COLOR_MEDGRAY);

  canvas.setTextColor(TFT_SILVER, COLOR_DARKGRAY);
  canvas.setTextDatum(middle_center);
  canvas.setTextSize(1.25);
  canvas.drawString("Lego Train Control", w / 2, hy + hh / 2);

  canvas.setTextColor(TFT_SILVER, COLOR_MEDGRAY);
  canvas.setTextSize(1.8);
  canvas.drawString("BT", c1 + bw / 2, bty + bw / 2);
  canvas.drawString("IR", c6 + bw / 2, iry + bw / 2);

  draw_rssi_indicator(&canvas, hx + om, hy + hh / 2, btInit, btRssi);
  if (btSensorPort != NO_SENSOR_FOUND)
    draw_sensor_indicator(&canvas, hx + om + om + 21, hy + hh / 2, btSensorColor);
  draw_battery_indicator(&canvas, w - 34, hy + (hh / 2), batteryPct);

  // draw labels
  canvas.setTextColor(TFT_SILVER, COLOR_MEDGRAY);
  canvas.setTextDatum(bottom_center);
  canvas.setTextSize(1);
  canvas.drawString("A", c2 + bw / 2, r1 - 2);
  canvas.drawString("B", c3 + bw / 2, r1 - 2);
  canvas.drawString("RED", c4 + bw / 2, r1 - 2);
  canvas.drawString("BLUE", c5 + bw / 2, r1 - 2);
  canvas.drawString("CH", c6 + bw / 2, r2 - 2);

  // draw all layout for remotes
  State state = {btInit, btLedColor, btSensorPort,
                 btSensorSpdUpColor, btSensorStopColor, btSensorSpdDnColor,
                 btSensorSpdUpFunction, btSensorStopFunction, btSensorSpdDnFunction,
                 irChannel};
  for (auto button : buttons)
  {
    unsigned short color = get_button_color(&button);
    canvas.fillRoundRect(button.x, button.y, button.w, button.h, 3, color);
    draw_button_symbol(&canvas, button, state);
  }

  canvas.pushSprite(0, 0);
}

void setup()
{
  USBSerial.begin(115200);
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);

  checkForMenuBoot();

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(brightness);
  canvas.createSprite(w, h);

  // workaround for Legoino PowerFunctions ctor (don't want to put on heap)
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);

  draw();
}

void loop()
{
  M5Cardputer.update();

  bool actionTaken = false;

  // keyboard triggered action
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed() && millis() - lastKeyPressMillis > KeyboardDebounce)
  {
    lastKeyPressMillis = millis();

    for (int i = 0; i < buttonCount; i++)
    {
      if (M5Cardputer.Keyboard.isKeyPressed(buttons[i].key))
      {
        handle_button_press(&buttons[i]);
        actionTaken = true;
      }
    }

    if (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed(';'))
    {
      brightness = M5Cardputer.Keyboard.isKeyPressed('.')
                       ? max(0, brightness - 10)
                       : min(100, brightness + 10);
      log_i("brightness: %d", brightness);
      M5Cardputer.Display.setBrightness(brightness);
    }
  }

  // sensor or button triggered action
  if (btAutoAction != NoAction)
  {
    for (int i = 0; i < buttonCount; i++)
    {
      if (buttons[i].action == btAutoAction && (buttons[i].port == None || buttons[i].port == btMotorPort))
      {
        handle_button_press(&buttons[i]);
        btAutoAction = NoAction;
        actionTaken = true;
      }
    }
  }

  // draw button being pressed
  if (actionTaken)
  {
    draw();
    delay(60);
    for (int i = 0; i < buttonCount; i++)
      buttons[i].pressed = false;
    redraw = true;
    btLastAction = millis();
  }

  // update loop for system bar redraws, etc
  if (millis() > updateDelay)
  {
    updateDelay = millis() + 1000;
    int newBatteryPct = M5Cardputer.Power.getBatteryLevel();
    if (newBatteryPct != batteryPct)
    {
      batteryPct = newBatteryPct;
      redraw = true;
    }

    if (btInit)
    {
      if (!btTrainCtl.isConnected())
      {
        btInit = false;
        redraw = true;
      }
      else
      {
        if (btLedColor == Color::NONE)
        {
          byte btLedPort = btTrainCtl.getPortForDeviceType((byte)DeviceType::HUB_LED);
          if (btLedPort != NO_SENSOR_FOUND)
          {
            btLedColor = (Color)(1 + (millis() % (Color::NUM_COLORS - 1))); // "random" color
            btTrainCtl.setLedColor(btLedColor);
          }
        }

        if (!btSensorInit) // TODO: timeout for checking for sensor?
        {
          btSensorPort = btTrainCtl.getPortForDeviceType((byte)DeviceType::COLOR_DISTANCE_SENSOR);
          if (btSensorPort == BtPortA || btSensorPort == BtPortB)
          {
            btTrainCtl.activatePortDevice(btSensorPort, sensorCallback);
            btMotorPort = btSensorPort == BtPortA ? BtPortB : BtPortA;
            btSensorColor = Color::NONE;
            btSensorInit = true;
            redraw = true;
          }
        }
        // bt sensor delayed start after stop
        if (btSensorStopFunction > 0 && btSensorStopDelay > 0 && millis() > btSensorStopDelay)
        {
          resumeTrainMotion();

          // also show press for sensor button
          for (int i = 0; i < buttonCount; i++)
          {
            if (buttons[i].action == Brake && buttons[i].port == btSensorPort)
            {
              buttons[i].pressed = true;
              break;
            }
          }
          return; // TODO: make sure OK
        }

        if (millis() - btLastAction > BtInactiveTimeoutMs &&
            (btMotorPort == BtPortA && btPortASpd == 0 ||
             btMotorPort == BtPortB && btPortBSpd == 0))
        {
          btConnectionToggle();
          redraw = true;
        }
      }
    }
  }

  if (redraw)
  {
    draw();
    redraw = false;
  }
}