#include <Arduino.h>
#include <Lpf2Hub.h>
#include <M5Cardputer.h>
#include <M5StackUpdater.h>
#include <PowerFunctions.h>

#include "common.h"
#include "draw_helper.h"

#define IR_TX_PIN 44
#define NO_SENSOR_FOUND 0xFF

unsigned short COLOR_GREYORANGEDIM =
    interpolateColors(COLOR_LIGHTGRAY, COLOR_ORANGE, 25);
unsigned short COLOR_GREYORANGEBRIGHT =
    interpolateColors(COLOR_LIGHTGRAY, COLOR_ORANGE, 75);

M5Canvas canvas(&M5Cardputer.Display);

const byte BtPortA = (byte)PoweredUpHubPort::A;
const byte BtPortB = (byte)PoweredUpHubPort::B;
const int BtMaxSpeed = 100;
const short BtSpdInc = 10; // TODO configurable
const short BtConnWait = 100;
const short BtColorInitWait = 500;
Lpf2Hub btTrainCtl;
short btPortASpd = 0;
short btPortBSpd = 0;

byte btSensorPort = NO_SENSOR_FOUND; // set to A or B if found
Color btSensorLastColorSeen = Color::NONE;
bool btSensorFunction = false; // false=color, true=distance

unsigned short btColorIndex = 0;
unsigned short btColorDelay = 0;
unsigned short btDisconnectDelay; // prevent jamming connect and then
                                  // disconnecting accidentally once connected

const PowerFunctionsPort IrPortR = PowerFunctionsPort::RED;
const PowerFunctionsPort IrPortB = PowerFunctionsPort::BLUE;
const int IrMaxSpeed = 105;
const short IrSpdInc = 15;
PowerFunctions irTrainCtl(IR_TX_PIN);
bool irTrackState = false;
byte irChannel = 0;
short irPortRSpd = 0;
short irPortBSpd = 0;

int batteryPct = M5Cardputer.Power.getBatteryLevel();
int updateDelay = 0;

// define a bunch of display variables to make adjutments not a nightmare
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

void sensorCallback(void *hub, byte sensorPort, DeviceType deviceType, uint8_t *pData);

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

void btConnectionInit()
{
  if (!btTrainCtl.isConnected())
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
      // detect if color/distance sensor is connected and set up for use if so
      btSensorPort = btTrainCtl.getPortForDeviceType((byte)DeviceType::COLOR_DISTANCE_SENSOR);
      if (btSensorPort == NO_SENSOR_FOUND)
      {
        // if first time connecting, wait a bit otherwise won't work
        if (btColorDelay == 0)
        {
          btColorDelay = millis() + BtColorInitWait;
        }
        else
        {
          btTrainCtl.setLedColor(BtColors[btColorIndex].color);
        }
      }
      else if (btSensorPort == BtPortA || btSensorPort == BtPortB)
      {
        // TODO: check if sensor port == to btChA or btChB?
        btTrainCtl.activatePortDevice(btSensorPort, sensorCallback);
      }
    }

    btDisconnectDelay = millis() + 500;
  }
  else if (btDisconnectDelay < millis())
  {
    btTrainCtl.shutDownHub();
    btPortASpd = btPortBSpd = 0;
  }
}

void handle_button_press(Button *button)
{
  button->pressed = true;

  PowerFunctionsPwm pwm;
  switch (button->action)
  {
  case BtConnection:
    btConnectionInit();
    break;
  case BtColor:
    // can change colors while not connected to choose initial color
    btColorIndex = (btColorIndex + 1) % BtNumColors;
    if (btTrainCtl.isConnected())
      btTrainCtl.setLedColor(BtColors[btColorIndex].color);
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
        // TODO
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
        // TODO
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
        // TODO
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
        // TODO
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
        // TODO
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
        // TODO
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

void sensorCallback(void *hub, byte sensorPort, DeviceType deviceType, uint8_t *pData)
{
  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (deviceType == DeviceType::COLOR_DISTANCE_SENSOR)
  {
    // distance function: TODO
    if (btSensorFunction)
    {
      int distance = trainCtl->parseDistance(pData);
      // TODO: stop if blocked, save speed and continue moving when unblocked
    }
    // color function
    else
    {
      Action action;
      int color = trainCtl->parseColor(pData);
      switch (color)
      {
      case (byte)Color::GREEN:
        action = SpdUp;
        break;
      case (byte)Color::RED:
        action = Brake;
        break;
      case (byte)Color::BLUE:
        action = SpdDn;
        break;
      default:
        return;
      }

      // "press" the corresponding button on the opposite port (i.e. the motor)
      byte btMotorPort = (sensorPort == (byte)PoweredUpHubPort::A) ? BtPortB : BtPortA;
      for (int i = 0; i < buttonCount; i++)
      {
        if (buttons[i].action == action && buttons[i].port == btMotorPort)
        {
          handle_button_press(&buttons[i]);
        }
      }
    }
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

  draw_connected_indicator(&canvas, hx + om, hy + hh / 2,
                           btTrainCtl.isConnected());
  if (btSensorPort != NO_SENSOR_FOUND)
    draw_sensor_indicator(&canvas, hx + om + hy + hh / 2 + om, hy + hh / 2, btSensorLastColorSeen);
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
  State state = {btColorIndex, btTrainCtl.isConnected(), btSensorPort, irChannel};
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
  M5Cardputer.Display.setBrightness(100);
  canvas.createSprite(w, h);

  // workaround for Legoino PowerFunctions ctor (don't want to put on heap)
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);

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

  // redraw occcasionally for battery and bt connection status updates
  if (millis() > updateDelay)
  {
    updateDelay = millis() + 2000;
    int newBatteryPct = M5Cardputer.Power.getBatteryLevel();
    if (newBatteryPct != batteryPct)
    {
      batteryPct = newBatteryPct;
      draw();
    }

    if (btColorDelay > 0 && millis() > btColorDelay && btTrainCtl.isConnected())
    {
      btColorDelay = -1;
      btTrainCtl.setLedColor(BtColors[btColorIndex].color);
    }
  }
}