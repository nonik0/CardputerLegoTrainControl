#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <Arduino.h>
#include <Lpf2Hub.h>
#include <M5Cardputer.h>
#include <M5StackUpdater.h>
#include <PowerFunctions.h>

#include "common.h"
#include "draw_helper.h"
#include "SBrickHub.h"

#define IR_TX_PIN 44
#define NO_SENSOR_FOUND 0xFF

unsigned short COLOR_GREYORANGEDIM = interpolateColors(COLOR_LIGHTGRAY, COLOR_ORANGE, 25);
unsigned short COLOR_GREYORANGEBRIGHT = interpolateColors(COLOR_LIGHTGRAY, COLOR_ORANGE, 75);

M5Canvas canvas(&M5Cardputer.Display);
uint8_t brightness = 100;
unsigned long lastKeyPressMillis = 0;
const unsigned long KeyboardDebounce = 200;

// bluetooth train control constants
const int BtMaxSpeed = 100;
const short BtSpdInc = 10; // TODO configurable
const short BtConnWait = 100;
const int BtInactiveTimeoutMs = 5 * 60 * 1000;
const short BtDistanceStopThreshold = 100; // when train is "picked up"

// hub state
Lpf2Hub lpfHub;
bool lpfInit = false;
volatile int lpfRssi = -1000;
short lpfPortSpeed[2] = {0, 0};
unsigned long lpfDisconnectDelay;    // debounce disconnects
unsigned long lpfButtonDebounce = 0; // debounce button presses
volatile Color lpfLedColor = Color::ORANGE;
unsigned short lpfLedColorDelay = 0;
unsigned long lpfLastAction = 0; // track for auto-disconnect
volatile RemoteAction lpfAutoAction = NoAction;

// color/distance sensor
bool lpfSensorInit = false;
byte lpfSensorPort = NO_SENSOR_FOUND; // set to A or B if detected
byte lpfMotorPort = NO_SENSOR_FOUND;  // set to opposite of sensor port if detected
short distanceMovingAverage = 0;
volatile Color lpfSensorColor = Color::NONE; // detected color by sensor
unsigned long lpfSensorDebounce = 0;         // debounce sensor color changes
Color lpfSensorIgnoreColors[] = {Color::BLACK, Color::BLUE};
Color lpfSensorSpdUpColor = Color::GREEN;
Color lpfSensorStopColor = Color::RED;
Color lpfSensorSpdDnColor = Color::YELLOW;
int8_t lpfSensorSpdUpFunction = 0; // TBD
int8_t btSensorStopFunction = 0;   // <0=disabled, 0=brake, >0=wait time in seconds
int8_t lpfSensorSpdDnFunction = 0; // TBD
unsigned long lpfSensorStopDelay = 0;
short lpfSensorStopSavedSpd = 0; // saved speed before stopping

volatile bool redraw = false;

// IR train control state
const int IrMaxSpeed = 105;
const short IrSpdInc = 15; // hacky but to match 7 levels
PowerFunctions irTrainCtl(IR_TX_PIN);
bool irTrackState = false;
byte irChannel = 0;
short irPortSpeed[2] = {0, 0};

// SBrick state
const short SBrickSpdInc = 35; // ~ 255 / 10
SBrickHub sbrickHub;
float sbrickBatteryV = 0;
bool sbrickInit = false;
volatile int sbrickRssi = -1000;
short sbrickPortSpeed[4] = {0, 0, 0, 0};
unsigned long sbrickDisconnectDelay; // debounce disconnects
unsigned long sbrickLastAction = 0;  // track for auto-disconnect
// volatile Action sbrickAutoAction = NoAction;

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
int r3_5 = r3 + bw / 2 + im; //(r3 + rh) / 2;
int r2 = r3 - bw - im;
int r2_5 = (r2 + r3) / 2;
int r1 = r2 - bw - im;
int r1_5 = (r1 + r2) / 2;
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

int bwhh = bw / 2 - im;

Button lpfHubButtons[] = {
    {AuxTop, AuxCol, Row1_5, bw, bw, RemoteDevice::PoweredUpHub, 0xFF, BtConnection, COLOR_LIGHTGRAY, false},
    {AuxMid, AuxCol, Row2_5, bw, bw, RemoteDevice::PoweredUpHub, 0xFF, BtColor, COLOR_LIGHTGRAY, false},
    {NoTouchy, AuxCol, Row3_5, bw, bwhh, RemoteDevice::PoweredUpHub, (byte)PoweredUpHubPort::LED, NoAction, COLOR_MEDGRAY, false},
    {LeftPortSpdUp, LeftPortCol, Row1, bw, bw, RemoteDevice::PoweredUpHub, (byte)PoweredUpHubPort::A, SpdUp, COLOR_LIGHTGRAY, false},
    {LeftPortBrake, LeftPortCol, Row2, bw, bw, RemoteDevice::PoweredUpHub, (byte)PoweredUpHubPort::A, Brake, COLOR_LIGHTGRAY, false},
    {LeftPortSpdDn, LeftPortCol, Row3, bw, bw, RemoteDevice::PoweredUpHub, (byte)PoweredUpHubPort::A, SpdDn, COLOR_LIGHTGRAY, false},
    {RightPortSpdUp, RightPortCol, Row1, bw, bw, RemoteDevice::PoweredUpHub, (byte)PoweredUpHubPort::B, SpdUp, COLOR_LIGHTGRAY, false},
    {RightPortBrake, RightPortCol, Row2, bw, bw, RemoteDevice::PoweredUpHub, (byte)PoweredUpHubPort::B, Brake, COLOR_LIGHTGRAY, false},
    {RightPortSpdDn, RightPortCol, Row3, bw, bw, RemoteDevice::PoweredUpHub, (byte)PoweredUpHubPort::B, SpdDn, COLOR_LIGHTGRAY, false}};
uint8_t lpfButtonCount = sizeof(lpfHubButtons) / sizeof(Button);

Button sbrickHubButtons[] = {
    {AuxTop, AuxCol, Row2, bw, bw, RemoteDevice::SBrick, 0xFF, BtConnection, COLOR_LIGHTGRAY, false},
    {LeftPortSpdUp, LeftPortCol, Row1, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubChannel::A, SpdUp, COLOR_LIGHTGRAY, false},
    {LeftPortBrake, LeftPortCol, Row2, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubChannel::A, Brake, COLOR_LIGHTGRAY, false},
    {LeftPortSpdDn, LeftPortCol, Row3, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubChannel::A, SpdDn, COLOR_LIGHTGRAY, false},
    {RightPortSpdUp, RightPortCol, Row1, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubChannel::B, SpdUp, COLOR_LIGHTGRAY, false},
    {RightPortBrake, RightPortCol, Row2, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubChannel::B, Brake, COLOR_LIGHTGRAY, false},
    {RightPortSpdDn, RightPortCol, Row3, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubChannel::B, SpdDn, COLOR_LIGHTGRAY, false},
};
uint8_t sbrickButtonCount = sizeof(sbrickHubButtons) / sizeof(Button);

Button powerFunctionsIrButtons[] = {
    {AuxTop, AuxCol, Row2, bw, bw, RemoteDevice::PowerFunctionsIR, 0xFF, IrChannel, COLOR_LIGHTGRAY, false},
    {AuxMid, HiddenCol, HiddenRow, 0, 0, RemoteDevice::PowerFunctionsIR, 0xFF, IrTrackState, COLOR_LIGHTGRAY, false},
    {LeftPortSpdUp, LeftPortCol, Row1, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::RED, SpdUp, COLOR_LIGHTGRAY, false},
    {LeftPortBrake, LeftPortCol, Row2, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::RED, Brake, COLOR_LIGHTGRAY, false},
    {LeftPortSpdDn, LeftPortCol, Row3, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::RED, SpdDn, COLOR_LIGHTGRAY, false},
    {RightPortSpdUp, RightPortCol, Row1, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::BLUE, SpdUp, COLOR_LIGHTGRAY, false},
    {RightPortBrake, RightPortCol, Row2, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::BLUE, Brake, COLOR_LIGHTGRAY, false},
    {RightPortSpdDn, RightPortCol, Row3, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::BLUE, SpdDn, COLOR_LIGHTGRAY, false},
};
uint8_t irButtonCount = sizeof(powerFunctionsIrButtons) / sizeof(Button);

Button circuitCubesButtons[] = {};
uint8_t circuitCubesButtonCount = sizeof(circuitCubesButtons) / sizeof(Button);

// must match order of RemoteDevice enum
Button *remoteButton[] = {lpfHubButtons, sbrickHubButtons, powerFunctionsIrButtons, circuitCubesButtons};
uint8_t remoteButtonCount[] = {lpfButtonCount, sbrickButtonCount, irButtonCount, circuitCubesButtonCount};
RemoteDevice activeRemoteLeft = RemoteDevice::PoweredUpHub;
RemoteDevice activeRemoteRight = RemoteDevice::SBrick;

bool isIgnoredColor(Color color)
{
  for (int i = 0; i < sizeof(lpfSensorIgnoreColors) / sizeof(Color); i++)
  {
    if (color == lpfSensorIgnoreColors[i])
      return true;
  }
  return false;
}

inline void resumeTrainMotion()
{
  lpfSensorStopDelay = 0;
  lpfAutoAction = lpfSensorStopSavedSpd > 0 ? SpdUp : SpdDn;
  short btSpdAdjust = lpfSensorStopSavedSpd > 0 ? -BtSpdInc : BtSpdInc;
  lpfPortSpeed[lpfMotorPort] = lpfSensorStopSavedSpd + btSpdAdjust;
}

void lpfButtonCallback(void *hub, HubPropertyReference hubProperty, uint8_t *pData)
{
  log_w("buttonCallback");

  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (hubProperty != HubPropertyReference::BUTTON || millis() < lpfButtonDebounce)
  {
    return;
  }

  lpfButtonDebounce = millis() + 200;
  ButtonState buttonState = trainCtl->parseHubButton(pData);

  if (buttonState == ButtonState::PRESSED)
  {
    if (lpfSensorStopDelay > 0)
    {
      log_w("bt button: resume");
      resumeTrainMotion();

      // also show press for led color button
      Button *lpfButton = remoteButton[RemoteDevice::PoweredUpHub];
      for (int i = 0; i < remoteButtonCount[RemoteDevice::PoweredUpHub]; i++)
      {
        if (lpfButton[i].action == BtColor)
        {
          lpfButton[i].pressed = true;
          break;
        }
      }
    }
    else
    {
      log_w("bt button: color");
      lpfAutoAction = BtColor;
    }
  }
}

void lpfRssiCallback(void *hub, HubPropertyReference hubProperty, uint8_t *pData)
{
  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (hubProperty != HubPropertyReference::RSSI)
  {
    return;
  }

  int rssi = trainCtl->parseRssi(pData);

  if (rssi == lpfRssi)
  {
    return;
  }

  log_w("rssiCallback: %d", rssi);
  lpfRssi = rssi;
  redraw = true;
}

void lpfSensorCallback(void *hub, byte sensorPort, DeviceType deviceType, uint8_t *pData)
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
  if (distanceMovingAverage > BtDistanceStopThreshold && lpfPortSpeed[lpfMotorPort] != 0)
  {
    log_w("off track, distance moving avg: %d", distance);

    lpfAutoAction = Brake;
    lpfSensorStopDelay = 0;
    lpfSensorStopSavedSpd = 0;
    return;
  }

  // filter noise
  if (color == lpfSensorColor || millis() < lpfSensorDebounce)
  {
    return;
  }

  lpfSensorColor = color;
  lpfSensorDebounce = millis() + 200;
  redraw = true;

  // ignore "ground" or noisy colors
  if (isIgnoredColor(color))
  {
    return;
  }

  lpfLedColor = color;
  trainCtl->setLedColor(color);

  // trigger actions for specific colors
  RemoteAction action;
  if (color == lpfSensorSpdUpColor)
  {
    log_i("bt action: spdup");
    lpfAutoAction = SpdUp;
  }
  else if (color == lpfSensorStopColor)
  {
    // resumed by calling resumeTrainMotion() after delay
    log_i("bt action: brake");
    lpfAutoAction = Brake;
    lpfSensorStopDelay = millis() + btSensorStopFunction * 1000;
    lpfSensorStopSavedSpd = lpfPortSpeed[lpfMotorPort];
  }
  else if (color == lpfSensorSpdDnColor)
  {
    log_i("bt action: spddn");
    lpfAutoAction = SpdDn;
  }
  else
  {
    return;
  }

  // also show press for sensor button
  Button *lpfButton = remoteButton[RemoteDevice::PoweredUpHub];
  for (int i = 0; i < remoteButtonCount[RemoteDevice::PoweredUpHub]; i++)
  {
    if (lpfButton[i].action == lpfAutoAction && lpfButton[i].port == lpfSensorPort)
    {
      lpfButton[i].pressed = true;
      break;
    }
  }
}

void lpfConnectionToggle()
{
  log_d("btConnectionToggle");

  if (!lpfInit)
  {
    if (!lpfHub.isConnecting())
    {
      lpfHub.init();
    }

    unsigned long exp = millis() + BtConnWait;
    while (!lpfHub.isConnecting() && exp > millis())
      ;

    if (lpfHub.isConnecting() && lpfHub.connectHub())
    {
      lpfInit = true;
      lpfLedColor = Color::NONE;
      lpfHub.activateHubPropertyUpdate(HubPropertyReference::BUTTON, lpfButtonCallback); // TODO: not working anymore??
      lpfHub.activateHubPropertyUpdate(HubPropertyReference::RSSI, lpfRssiCallback);
    }

    lpfDisconnectDelay = millis() + 500;
  }
  else if (lpfDisconnectDelay < millis())
  {
    lpfHub.shutDownHub();
    delay(200);
    lpfInit = false;
    lpfSensorInit = false;
    lpfPortSpeed[0] = lpfPortSpeed[1] = 0;
    lpfMotorPort = NO_SENSOR_FOUND;
    lpfSensorPort = NO_SENSOR_FOUND;
    lpfSensorSpdUpColor = Color::GREEN;
    lpfSensorStopColor = Color::RED;
    lpfSensorSpdDnColor = Color::YELLOW;
    btSensorStopFunction = 0;
    lpfSensorStopDelay = 0;
  }
}

void sbrickConnectionToggle()
{
  log_d("sbrickConnectionToggle");

  if (!sbrickInit)
  {
    if (!sbrickHub.isConnecting())
    {
      // sbrickHub.init();
      sbrickHub.init("88:6b:0f:80:35:b8"); // TODO: sbrick discovery
    }

    unsigned long exp = millis() + BtConnWait;
    while (!sbrickHub.isConnecting() && exp > millis())
      ;

    if (sbrickHub.isConnecting() && sbrickHub.connectHub())
    {
      sbrickInit = true;
    }

    sbrickDisconnectDelay = millis() + 500;
  }
  else if (sbrickDisconnectDelay < millis())
  {
    sbrickHub.disconnectHub();
    delay(200);
    sbrickInit = false;
    sbrickPortSpeed[0] = sbrickPortSpeed[1] = sbrickPortSpeed[2] = sbrickPortSpeed[3] = 0;
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

void handle_button_press(Button *button)
{
  button->pressed = true;

  switch (button->action)
  {
  case BtConnection:
    switch (button->device)
    {
    case RemoteDevice::PoweredUpHub:
      lpfConnectionToggle();
      break;
    case RemoteDevice::SBrick:
      sbrickConnectionToggle();
      break;
    }
    break;
  case BtColor:
    // function like hub button
    if (lpfSensorStopDelay > 0)
    {
      resumeTrainMotion();
    }
    else
    {
      // can change colors while not connected to choose initial color
      lpfLedColor = (lpfLedColor == Color(1))
                        ? Color(Color::NUM_COLORS - 1)
                        : (Color)(lpfLedColor - 1);
      log_i("bt color: %d", lpfLedColor);
      if (lpfHub.isConnected())
        lpfHub.setLedColor(lpfLedColor);
    }
    break;
  case IrChannel:
    irChannel = (irChannel + 1) % 4;
    break;
  case IrTrackState:
    irTrackState = !irTrackState;
    irPortSpeed[(byte)PowerFunctionsPort::RED] = 0;
    irPortSpeed[(byte)PowerFunctionsPort::BLUE] = 0;
    break;
  case SpdUp:
  case Brake:
  case SpdDn:
    switch (button->device)
    {
    case RemoteDevice::PoweredUpHub:
      if (!lpfHub.isConnected())
        break;

      if (button->port == lpfSensorPort)
      {
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN))
        {
          switch (button->action)
          {
          case SpdUp:
            do
            {
              lpfSensorSpdUpColor = (lpfSensorSpdUpColor == Color::BLACK)
                                        ? Color(Color::NUM_COLORS - 1)
                                        : (Color)(lpfSensorSpdUpColor - 1);
            } while (isIgnoredColor(lpfSensorSpdUpColor) || lpfSensorSpdUpColor == lpfSensorSpdDnColor || lpfSensorSpdUpColor == lpfSensorStopColor);
            break;
          case Brake:
            do
            {
              lpfSensorStopColor = (lpfSensorStopColor == Color::BLACK)
                                       ? Color(Color::NUM_COLORS - 1)
                                       : (Color)(lpfSensorStopColor - 1);
            } while (isIgnoredColor(lpfSensorStopColor) || lpfSensorStopColor == lpfSensorSpdUpColor || lpfSensorStopColor == lpfSensorSpdDnColor);
            break;
          case SpdDn:
            do
            {
              lpfSensorSpdDnColor = (lpfSensorSpdDnColor == Color::BLACK)
                                        ? Color(Color::NUM_COLORS - 1)
                                        : (Color)(lpfSensorSpdDnColor - 1);
            } while (isIgnoredColor(lpfSensorSpdDnColor) || lpfSensorSpdDnColor == lpfSensorSpdUpColor || lpfSensorSpdDnColor == lpfSensorStopColor);
            break;
          }
        }
        else if (button->action == Brake)
        {
          if (btSensorStopFunction == 0)
            btSensorStopFunction = 2;
          else if (btSensorStopFunction == 2)
            btSensorStopFunction = 5;
          else if (btSensorStopFunction == 5)
            btSensorStopFunction = 10;
          else if (btSensorStopFunction == 10)
            btSensorStopFunction = -1;
          else
            btSensorStopFunction = 0;
        }
      }
      else
      {
        switch (button->action)
        {
        case SpdUp:
          lpfPortSpeed[button->port] = min(BtMaxSpeed, lpfPortSpeed[button->port] + BtSpdInc);
          break;
        case Brake:
          lpfPortSpeed[button->port] = 0;
          break;
        case SpdDn:
          lpfPortSpeed[button->port] = max(-BtMaxSpeed, lpfPortSpeed[button->port] - BtSpdInc);
          break;
        }
        lpfHub.setBasicMotorSpeed(button->port, lpfPortSpeed[button->port]);
      }
    case RemoteDevice::PowerFunctionsIR:
      if (irTrackState)
      {
        switch (button->action)
        {
        case SpdUp:
          irPortSpeed[button->port] = min(IrMaxSpeed, irPortSpeed[button->port] + IrSpdInc);
          break;
        case Brake:
          irPortSpeed[button->port] = 0;
          break;
        case SpdDn:
          irPortSpeed[button->port] = max(-IrMaxSpeed, irPortSpeed[button->port] - IrSpdInc);
          break;
        }

        PowerFunctionsPwm pwm = irTrainCtl.speedToPwm(irPortSpeed[button->port]);
        irTrainCtl.single_pwm((PowerFunctionsPort)button->port, pwm, irChannel);
      }
      else
      {
        switch (button->action)
        {
        case SpdUp:
          irTrainCtl.single_increment((PowerFunctionsPort)button->port, irChannel);
          break;
        case Brake:
          irTrainCtl.single_pwm((PowerFunctionsPort)button->port, PowerFunctionsPwm::BRAKE, irChannel);
          break;
        case SpdDn:
          irTrainCtl.single_decrement((PowerFunctionsPort)button->port, irChannel);
          break;
        }
      }
      break;
    case RemoteDevice::SBrick:
      if (!sbrickHub.isConnected())
        break;

      switch (button->action)
      {
      case SpdUp:
        sbrickPortSpeed[button->port] = min(SBRICK_MAX_CHANNEL_SPEED, sbrickPortSpeed[button->port] + SBrickSpdInc);
        break;
      case Brake:
        sbrickPortSpeed[button->port] = 0;
        break;
      case SpdDn:
        sbrickPortSpeed[button->port] = max(SBRICK_MIN_CHANNEL_SPEED, sbrickPortSpeed[button->port] - SBrickSpdInc);
        break;
      }

      sbrickHub.setMotorSpeed(button->port, sbrickPortSpeed[button->port]);
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

  if (button->device == RemoteDevice::PoweredUpHub)
  {
    if (button->port == lpfSensorPort)
    {
      switch (button->action)
      {
      case SpdUp:
        return interpolateColors(COLOR_LIGHTGRAY, BtColors[lpfSensorSpdUpColor], 50);
      case Brake:
        return interpolateColors(COLOR_LIGHTGRAY, BtColors[lpfSensorStopColor], 50);
      case SpdDn:
        return interpolateColors(COLOR_LIGHTGRAY, BtColors[lpfSensorSpdDnColor], 50);
      default:
        return button->color;
      }
    }

    if (button->action == BtColor)
    {
      return interpolateColors(COLOR_LIGHTGRAY, BtColors[lpfLedColor], 50);
    }

    if (button->port == (byte)PoweredUpHubPort::LED)
    {
      if (lpfSensorInit)
        return BtColors[lpfSensorColor];
    }
  }

  if (button->action == Brake)
  {
    if (button->device == RemoteDevice::PoweredUpHub && lpfPortSpeed[button->port] == 0 ||
        button->device == RemoteDevice::SBrick && sbrickPortSpeed[button->port] == 0 ||
        button->device == RemoteDevice::PowerFunctionsIR && irTrackState && irPortSpeed[button->port] == 0)
    {
      return COLOR_GREYORANGEDIM;
    }
  }

  if (button->action == SpdUp || button->action == SpdDn)
  {
    int speed = 0;
    switch (button->device)
    {
    case RemoteDevice::PoweredUpHub:
      speed = lpfPortSpeed[button->port];
      break;
    case RemoteDevice::SBrick:
      speed = sbrickPortSpeed[button->port];
      break;
    case RemoteDevice::PowerFunctionsIR:
      speed = irTrackState ? irPortSpeed[button->port] : 0;
      break;
    }

    if (button->action == SpdUp && speed > 0 || button->action == SpdDn && speed < 0)
    {
      return interpolateColors(COLOR_GREYORANGEDIM, COLOR_GREYORANGEBRIGHT, abs(speed));
    }
  }

  return button->color;
}

bool getPressedRemoteKey(RemoteKey &pressedKey, bool &isLeftRemote)
{
  pressedKey = RemoteKey::NoTouchy;

  // aux
  if (M5Cardputer.Keyboard.isKeyPressed('`'))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::AuxTop;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::AuxTop;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::AuxMid;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('\\'))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::AuxMid;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::AuxBot;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::AuxBot;
  }

  // left port
  else if (M5Cardputer.Keyboard.isKeyPressed('e'))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::LeftPortSpdUp;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('i'))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::LeftPortSpdUp;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('s'))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::LeftPortBrake;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('j'))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::LeftPortBrake;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('z'))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::LeftPortSpdDn;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('n'))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::LeftPortSpdDn;
  }

  // right port
  else if (M5Cardputer.Keyboard.isKeyPressed('r'))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::RightPortSpdUp;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('o'))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::RightPortSpdUp;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('d'))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::RightPortBrake;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('k'))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::RightPortBrake;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('x'))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::RightPortSpdDn;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('m'))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::RightPortSpdDn;
  }

  return pressedKey != RemoteKey::NoTouchy;
}

String getRemoteString(RemoteDevice remote)
{
  switch (remote)
  {
  case RemoteDevice::PoweredUpHub:
    return "PU";
  case RemoteDevice::SBrick:
    return "SB";
  case RemoteDevice::PowerFunctionsIR:
    return "IR";
  case RemoteDevice::CircuitCubes:
    return "CC";
  default:
    return "??";
  }
}

String getRemoteLeftPortString(RemoteDevice remote)
{
  switch (remote)
  {
  case RemoteDevice::PoweredUpHub:
    return "A";
  case RemoteDevice::SBrick:
    return "A";
  case RemoteDevice::PowerFunctionsIR:
    return "RED";
  case RemoteDevice::CircuitCubes:
    return "A";
  default:
    return "?";
  }
}

String getRemoteRightPortString(RemoteDevice remote)
{
  switch (remote)
  {
  case RemoteDevice::PoweredUpHub:
    return "B";
  case RemoteDevice::SBrick:
    return "B";
  case RemoteDevice::PowerFunctionsIR:
    return "BLUE";
  case RemoteDevice::CircuitCubes:
    return "B";
  default:
    return "?";
  }
}

int getRemoteX(RemoteColumn col, bool isLeftRemote)
{
  switch (col)
  {
  case AuxCol:
    return isLeftRemote ? c1 : c6;
  case LeftPortCol:
    return isLeftRemote ? c2 : c4;
  case RightPortCol:
    return isLeftRemote ? c3 : c5;
  default:
    return -w;
  }
}

int getRemoteY(RemoteRow row)
{
  switch (row)
  {
  case Row1:
    return r1;
  case Row2:
    return r2;
  case Row3:
    return r3;
  case Row1_5:
    return r1_5;
  case Row2_5:
    return r2_5;
  case Row3_5:
    return r3_5;
  default:
    return -h;
  }
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
  canvas.drawString(getRemoteString(activeRemoteLeft), c1 + bw / 2, bty + bw / 2);
  canvas.drawString(getRemoteString(activeRemoteRight), c6 + bw / 2, iry + bw / 2);

  // TODO: indicator that shows active/inactive tabs, e.g. [] [x] [x] []
  draw_battery_indicator(&canvas, w - 34, hy + (hh / 2), batteryPct);

  // draw labels
  canvas.setTextColor(TFT_SILVER, COLOR_MEDGRAY);
  canvas.setTextDatum(bottom_center);
  canvas.setTextSize(1);
  canvas.drawString(getRemoteLeftPortString(activeRemoteLeft), c2 + bw / 2, r1 - 2);
  canvas.drawString(getRemoteRightPortString(activeRemoteLeft), c3 + bw / 2, r1 - 2);
  canvas.drawString(getRemoteLeftPortString(activeRemoteRight), c4 + bw / 2, r1 - 2);
  canvas.drawString(getRemoteRightPortString(activeRemoteRight), c5 + bw / 2, r1 - 2);

  // draw layout for both active remotes
  State state = {lpfInit, lpfRssi, lpfLedColor, lpfSensorPort,
                 lpfSensorSpdUpColor, lpfSensorStopColor, lpfSensorSpdDnColor,
                 lpfSensorSpdUpFunction, btSensorStopFunction, lpfSensorSpdDnFunction,
                 sbrickInit,
                 irChannel};

  Button *leftRemoteButton = remoteButton[activeRemoteLeft];
  for (int i = 0; i < remoteButtonCount[activeRemoteLeft]; i++)
  {
    Button button = leftRemoteButton[i];
    unsigned short color = get_button_color(&button);
    int x = getRemoteX(button.col, true);
    int y = getRemoteY(button.row);
    canvas.fillRoundRect(x, y, button.w, button.h, 3, color);
    draw_button_symbol(&canvas, button, x, y, state);
  }

  Button *rightRemoteButton = remoteButton[activeRemoteRight];
  for (int i = 0; i < remoteButtonCount[activeRemoteRight]; i++)
  {
    Button button = rightRemoteButton[i];
    unsigned short color = get_button_color(&button);
    int x = getRemoteX(button.col, false);
    int y = getRemoteY(button.row);
    canvas.fillRoundRect(x, y, button.w, button.h, 3, color);
    draw_button_symbol(&canvas, button, x, y, state);
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

    bool isLeftRemote;
    RemoteKey remoteKeyPressed;
    if (getPressedRemoteKey(remoteKeyPressed, isLeftRemote))
    {
      int activeRemote = isLeftRemote ? activeRemoteLeft : activeRemoteRight;

      for (int i = 0; i < remoteButtonCount[activeRemote]; i++)
      {
        if (remoteButton[activeRemote][i].key == remoteKeyPressed)
        {
          handle_button_press(&remoteButton[activeRemote][i]);
          actionTaken = true;
          break;
        }
      }
    }

    // toggle active left remote
    if (M5Cardputer.Keyboard.isKeyPressed(KEY_LEFT_CTRL))
    {
      do
      {
        activeRemoteLeft = (RemoteDevice)((activeRemoteLeft + 1) % RemoteDevice::NUM_DEVICES);
      } while (activeRemoteLeft == activeRemoteRight);
      redraw = true;
    }

    // toggle active right remote
    if (M5Cardputer.Keyboard.isKeyPressed(' '))
    {
      do
      {
        activeRemoteRight = (RemoteDevice)((activeRemoteRight + 1) % RemoteDevice::NUM_DEVICES);
      } while (activeRemoteRight == activeRemoteLeft);
      redraw = true;
    }

    // adjust brightness
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
  if (lpfAutoAction != NoAction)
  {
    log_w("auto action: %d", lpfAutoAction);

    Button *lpfButton = remoteButton[RemoteDevice::PoweredUpHub];
    for (int i = 0; i < remoteButtonCount[RemoteDevice::PoweredUpHub]; i++)
    {
      if (lpfButton[i].action == lpfAutoAction && (lpfButton[i].port == 0xFF || lpfButton[i].port == lpfMotorPort))
      {
        handle_button_press(&lpfButton[i]);
        lpfAutoAction = NoAction;
        actionTaken = true;
      }
    }
  }

  // draw button being pressed
  if (actionTaken)
  {
    draw();
    delay(60);
    for (int i = 0; i < remoteButtonCount[activeRemoteLeft]; i++)
      remoteButton[activeRemoteLeft][i].pressed = false;
    for (int i = 0; i < remoteButtonCount[activeRemoteRight]; i++)
      remoteButton[activeRemoteRight][i].pressed = false;
    redraw = true;

    // TODO
    lpfLastAction = millis();
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

    if (lpfInit)
    {
      if (!lpfHub.isConnected())
      {
        lpfInit = false;
        redraw = true;
      }
      else
      {
        if (lpfLedColor == Color::NONE)
        {
          byte btLedPort = lpfHub.getPortForDeviceType((byte)DeviceType::HUB_LED);
          if (btLedPort != NO_SENSOR_FOUND)
          {
            lpfLedColor = (Color)(1 + (millis() % (Color::NUM_COLORS - 1))); // "random" color
            lpfHub.setLedColor(lpfLedColor);
          }
        }

        if (!lpfSensorInit) // TODO: timeout for checking for sensor?
        {
          lpfSensorPort = lpfHub.getPortForDeviceType((byte)DeviceType::COLOR_DISTANCE_SENSOR);
          if (lpfSensorPort == (byte)PoweredUpHubPort::A || lpfSensorPort == (byte)PoweredUpHubPort::B)
          {
            lpfHub.activatePortDevice(lpfSensorPort, lpfSensorCallback);
            lpfMotorPort = lpfSensorPort == (byte)PoweredUpHubPort::A ? (byte)PoweredUpHubPort::B : (byte)PoweredUpHubPort::A;
            lpfSensorColor = Color::NONE;
            lpfSensorInit = true;
            redraw = true;
          }
        }
        // bt sensor delayed start after stop
        if (btSensorStopFunction > 0 && lpfSensorStopDelay > 0 && millis() > lpfSensorStopDelay)
        {
          resumeTrainMotion();

          // also show press for sensor button
          Button *lpfButton = remoteButton[RemoteDevice::PoweredUpHub];
          for (int i = 0; i < remoteButtonCount[RemoteDevice::PoweredUpHub]; i++)
          {
            if (lpfButton[i].action == Brake && lpfButton[i].port == lpfSensorPort)
            {
              lpfButton[i].pressed = true;
              break;
            }
          }
          return; // TODO: make sure OK
        }

        if (millis() - lpfLastAction > BtInactiveTimeoutMs && lpfPortSpeed[lpfMotorPort] == 0) // TODO: check both, only works with sensor?
        {
          lpfConnectionToggle();
          redraw = true;
        }
      }
    }

    if (sbrickInit)
    {
      if (!sbrickHub.isConnected())
      {
        sbrickInit = false;
        redraw = true;
      }
      else
      {
        int newBatteryV = M5Cardputer.Power.getBatteryLevel();
        if (newBatteryV != sbrickBatteryV)
        {
          log_i("sbrick battery %d", newBatteryV);
          sbrickBatteryV = newBatteryV;
          redraw = true;
        }

        // disable for now
        // if (sbrickHub.getWatchdogTimeout()) {
        //   log_i("disabling sbrick watchdog");
        //   USBSerial.println("OVER SERIAL disabling sbrick watchdog");
        //   sbrickHub.setWatchdogTimeout(0);
        // }

        // TODO remote action specific or any action of cardputer resets timer?
        // if (millis() - sbrickLastAction > BtInactiveTimeoutMs)
        // {
        //   sbrickConnectionToggle();
        //   redraw = true;
        // }
      }
    }
  }

  if (redraw)
  {
    draw();
    redraw = false;
  }
}