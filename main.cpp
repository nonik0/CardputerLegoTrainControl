#include <Arduino.h>
#include <Lpf2Hub.h>
#include <M5Cardputer.h>
#include <SD.h>

#include "common.h"
#include "draw_helper.h"
#include "IrBroadcast.h"
#include "settings.h"

#include "CircuitCubes/CircuitCubesHub.h"
#include "SBrick/SBrickHub.h"

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
const short BtConnWait = 500;
const int BtInactiveTimeoutMs = 5 * 60 * 1000;
const short BtDistanceStopThreshold = 100; // when train is "picked up"

// lpf2 hub state
Lpf2Hub lpf2Hub;
bool lpf2Init = false;
volatile int lpf2Rssi = -1000;
short lpf2PortSpeed[2] = {0, 0};
unsigned long lpf2DisconnectDelay;    // debounce disconnects
unsigned long lpf2ButtonDebounce = 0; // debounce button presses
volatile Color lpf2LedColor = Color::NONE;
unsigned short lpf2LedColorDelay = 0;
unsigned long lpf2LastAction = 0; // track for auto-disconnect
volatile RemoteAction lpf2AutoAction = NoAction;

// lpf2 color/distance sensor
bool lpf2SensorInit = false;
byte lpf2SensorPort = NO_SENSOR_FOUND; // set to A or B if detected
byte lpf2MotorPort = NO_SENSOR_FOUND;  // set to opposite of sensor port if detected
short lpf2SensorDistanceMovingAverage = 0;
volatile Color lpf2SensorColor = Color::NONE; // detected color by sensor
unsigned long lpf2SensorDebounce = 0;         // debounce sensor color changes
short lpf2SensorStopSavedSpd = 0; // saved speed before stopping
Color lpf2SensorIgnoreColors[] = {Color::BLACK, Color::BLUE};
// saved settings
Color lpf2SensorSpdUpColor = Color::GREEN;
Color lpf2SensorStopColor = Color::RED;
Color lpf2SensorSpdDnColor = Color::YELLOW;
int8_t lpf2SensorSpdUpFunction = 0; // <0=disabled, >0 speed up
int8_t lpf2SensorStopFunction = 0;  // <0=disabled, 0=brake, >0=wait time in seconds, // TODO? 0xFF=pause and reverse
int8_t lpf2SensorSpdDnFunction = 0; // <0=disabled, >0 speed down
unsigned long lpf2SensorStopDelay = 0;

// IR train control state
const int IrMaxSpeed = 105;
const short IrSpdInc = 15; // hacky but to match 7 levels
PowerFunctionsIrBroadcast irTrainCtl;
short irPortSpeed[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
unsigned long irSwitchDelay[2] = {0, 0};
volatile RemoteAction irAutoAction = NoAction;
volatile byte irAutoActionPort = 0xFF;
volatile bool irActionBroadcastRecv = false; // reflect action on display, but don't do
// saved settings
uint8_t irMode = 0; // 0=off, 1=track, 2=track, broadcast, 3=broadcast
byte irChannel = 0;
bool irPortFunction[4][2] = {{false, false}, {false, false}, {false, false}, {false, false}}; // false=motor, true=switch

// SBrick state
const short SBrickSpdInc = 35; // ~ 255 / 10
SBrickHub sbrickHub;
bool sbrickInit = false;
float sbrickBatteryV = 0;
float sbrickTempF = 0;
volatile int sbrickRssi = -1000;
short sbrickPortSpeed[4] = {0, 0, 0, 0};
unsigned long sbrickDisconnectDelay; // debounce disconnects
unsigned long sbrickLastAction = 0;  // track for auto-disconnect
// volatile Action sbrickAutoAction = NoAction;
volatile RemoteAction sbrickAutoAction = NoAction;
// saved settings
byte sbrickLeftPort = (byte)SBrickHubPort::A;
byte sbrickRightPort = (byte)SBrickHubPort::B;

// SBrick sensor state
bool sbrickMotionSensorInit = false;
byte sbrickMotionSensorPort = NO_SENSOR_FOUND;
float sbrickMotionSensorNeutralV = -1.0f;
bool sbrickTiltSensorInit = false;
byte sbrickTiltSensorPort = NO_SENSOR_FOUND;
float sbrickTiltSensorNeutralV = -1.0f;
float sbrickMotionSensorV;
float sbrickTiltSensorV;
WedoTilt sbrickSensorTilt;
bool sbrickSensorMotion;

// CircuitCubes state
const short circuitCubesSpdInc = 35; // ~ 255 / 10
CircuitCubesHub circuitCubesHub;
float circuitCubesBatteryV = 0;
bool circuitCubesInit = false;
short circuitCubesPortSpeed[3] = {0, 0, 0};
volatile int circuitCubesRssi = -1000;
unsigned long circuitCubesDisconnectDelay; // debounce disconnects
unsigned long circuitCubesLastAction = 0;  // track for auto-disconnect
// volatile Action circuitCubesAutoAction = NoAction;
// saved settings 
byte circuitCubesLeftPort = (byte)CircuitCubesHubPort::A;
byte circuitCubesRightPort = (byte)CircuitCubesHubPort::B;

// system bar state
int batteryPct = M5Cardputer.Power.getBatteryLevel();
unsigned long updateDelay = 0;

SPIClass SPI2;
bool sdInit = false;
volatile bool redraw = false;

// define a bunch of display variables to make adjustments not a nightmare
int w = 240; // width
int h = 135; // height
int bw = 25; // button width
int om = 4;  // outer margin
int im = 2;  // inner margin
int bwhh = bw / 2 - im;

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
// left remote rectangle
int lrx = c1 - om;
int lry = ry + om;
int lrw = 3 * bw + im + 3 * om;
int lrh = rh - im - om;
int lrtx = (rx + lrx) / 2;
int lrty = lry + (lrh / 2);
// right remote rectangle
int rrx = c4 - om;
int rry = lry;
int rrw = lrw;
int rrh = lrh;
int rrtx = (rx + rw + rrx + rrw) / 2;
int rrty = rry + (rrh / 2);

Button lpf2HubButtons[] = {
    {AuxOne, AuxCol, Row1_5, bw, bw, RemoteDevice::PoweredUp, 0xFF, BtConnection, COLOR_LIGHTGRAY, false},
    {AuxTwo, AuxCol, Row2_5, bw, bw, RemoteDevice::PoweredUp, 0xFF, Lpf2Color, COLOR_LIGHTGRAY, false},
    {LeftPortSpdUp, LeftPortCol, Row1, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::A, SpdUp, COLOR_LIGHTGRAY, false},
    {LeftPortBrake, LeftPortCol, Row2, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::A, Brake, COLOR_LIGHTGRAY, false},
    {LeftPortSpdDn, LeftPortCol, Row3, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::A, SpdDn, COLOR_LIGHTGRAY, false},
    {RightPortSpdUp, RightPortCol, Row1, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::B, SpdUp, COLOR_LIGHTGRAY, false},
    {RightPortBrake, RightPortCol, Row2, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::B, Brake, COLOR_LIGHTGRAY, false},
    {RightPortSpdDn, RightPortCol, Row3, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::B, SpdDn, COLOR_LIGHTGRAY, false}};
uint8_t lpf2ButtonCount = sizeof(lpf2HubButtons) / sizeof(Button);

Button sbrickHubButtons[] = {
    {AuxOne, AuxCol, Row2, bw, bw, RemoteDevice::SBrick, 0xFF, BtConnection, COLOR_LIGHTGRAY, false},
    {LeftPortFunction, HiddenCol, HiddenRow, 0, 0, RemoteDevice::SBrick, (byte)SBrickHubPort::A, PortFunction, COLOR_LIGHTGRAY, false},
    {LeftPortSpdUp, LeftPortCol, Row1, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::A, SpdUp, COLOR_LIGHTGRAY, false},
    {LeftPortBrake, LeftPortCol, Row2, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::A, Brake, COLOR_LIGHTGRAY, false},
    {LeftPortSpdDn, LeftPortCol, Row3, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::A, SpdDn, COLOR_LIGHTGRAY, false},
    {RightPortFunction, HiddenCol, HiddenRow, 0, 0, RemoteDevice::SBrick, (byte)SBrickHubPort::B, PortFunction, COLOR_LIGHTGRAY, false},
    {RightPortSpdUp, RightPortCol, Row1, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::B, SpdUp, COLOR_LIGHTGRAY, false},
    {RightPortBrake, RightPortCol, Row2, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::B, Brake, COLOR_LIGHTGRAY, false},
    {RightPortSpdDn, RightPortCol, Row3, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::B, SpdDn, COLOR_LIGHTGRAY, false},
};
uint8_t sbrickButtonCount = sizeof(sbrickHubButtons) / sizeof(Button);

Button circuitCubesButtons[] = {
    {AuxOne, AuxCol, Row2, bw, bw, RemoteDevice::CircuitCubes, 0xFF, BtConnection, COLOR_LIGHTGRAY, false},
    {LeftPortFunction, HiddenCol, HiddenRow, 0, 0, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::A, PortFunction, COLOR_LIGHTGRAY, false},
    {LeftPortSpdUp, LeftPortCol, Row1, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::A, SpdUp, COLOR_LIGHTGRAY, false},
    {LeftPortBrake, LeftPortCol, Row2, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::A, Brake, COLOR_LIGHTGRAY, false},
    {LeftPortSpdDn, LeftPortCol, Row3, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::A, SpdDn, COLOR_LIGHTGRAY, false},
    {RightPortFunction, HiddenCol, HiddenRow, 0, 0, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::B, PortFunction, COLOR_LIGHTGRAY, false},
    {RightPortSpdUp, RightPortCol, Row1, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::B, SpdUp, COLOR_LIGHTGRAY, false},
    {RightPortBrake, RightPortCol, Row2, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::B, Brake, COLOR_LIGHTGRAY, false},
    {RightPortSpdDn, RightPortCol, Row3, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::B, SpdDn, COLOR_LIGHTGRAY, false},
};
uint8_t circuitCubesButtonCount = sizeof(circuitCubesButtons) / sizeof(Button);

Button powerFunctionsIrButtons[] = {
    {AuxOne, AuxCol, Row1_5, bw, bw, RemoteDevice::PowerFunctionsIR, 0xFF, IrChannel, COLOR_LIGHTGRAY, false},
    {AuxTwo, AuxCol, Row2_5, bw, bw, RemoteDevice::PowerFunctionsIR, 0xFF, IrMode, COLOR_LIGHTGRAY, false},
    {LeftPortFunction, HiddenCol, HiddenRow, 0, 0, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::RED, PortFunction, COLOR_LIGHTGRAY, false},
    {LeftPortSpdUp, LeftPortCol, Row1, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::RED, SpdUp, COLOR_LIGHTGRAY, false},
    {LeftPortBrake, LeftPortCol, Row2, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::RED, Brake, COLOR_LIGHTGRAY, false},
    {LeftPortSpdDn, LeftPortCol, Row3, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::RED, SpdDn, COLOR_LIGHTGRAY, false},
    {RightPortFunction, HiddenCol, HiddenRow, 0, 0, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::BLUE, PortFunction, COLOR_LIGHTGRAY, false},
    {RightPortSpdUp, RightPortCol, Row1, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::BLUE, SpdUp, COLOR_LIGHTGRAY, false},
    {RightPortBrake, RightPortCol, Row2, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::BLUE, Brake, COLOR_LIGHTGRAY, false},
    {RightPortSpdDn, RightPortCol, Row3, bw, bw, RemoteDevice::PowerFunctionsIR, (byte)PowerFunctionsPort::BLUE, SpdDn, COLOR_LIGHTGRAY, false},
};
uint8_t powerFunctionsIrButtonCount = sizeof(powerFunctionsIrButtons) / sizeof(Button);

// must match order of RemoteDevice enum
Button *remoteButton[] = {lpf2HubButtons, powerFunctionsIrButtons, sbrickHubButtons, circuitCubesButtons};
uint8_t remoteButtonCount[] = {lpf2ButtonCount, powerFunctionsIrButtonCount, sbrickButtonCount, circuitCubesButtonCount};
RemoteDevice activeRemoteLeft = RemoteDevice::PoweredUp;
RemoteDevice activeRemoteRight = RemoteDevice::PowerFunctionsIR;

bool isIgnoredColor(Color color)
{
  for (int i = 0; i < sizeof(lpf2SensorIgnoreColors) / sizeof(Color); i++)
  {
    if (color == lpf2SensorIgnoreColors[i])
      return true;
  }
  return false;
}

void lpf2ResumeTrainMotion()
{
  lpf2SensorStopDelay = 0;
  lpf2AutoAction = lpf2SensorStopSavedSpd > 0 ? SpdUp : SpdDn;
  short btSpdAdjust = lpf2SensorStopSavedSpd > 0 ? -BtSpdInc : BtSpdInc;
  lpf2PortSpeed[lpf2MotorPort] = lpf2SensorStopSavedSpd + btSpdAdjust;
  log_w("resuming train motion: %d", lpf2PortSpeed[lpf2MotorPort]);
}

void lpf2HubCallback(void *hub, HubPropertyReference hubProperty, uint8_t *pData)
{
  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (hubProperty == HubPropertyReference::RSSI)
  {
    int rssi = trainCtl->parseRssi(pData);

    if (rssi == lpf2Rssi)
    {
      return;
    }

    log_d("rssiCallback: %d", rssi);
    lpf2Rssi = rssi;
    redraw = true;
    return;
  }
  else if (hubProperty == HubPropertyReference::BUTTON && millis() > lpf2ButtonDebounce)
  {
    lpf2ButtonDebounce = millis() + 200;
    ButtonState buttonState = trainCtl->parseHubButton(pData);

    log_d("buttonState: %d", buttonState);
    if (buttonState == ButtonState::PRESSED)
    {
      if (lpf2SensorStopDelay > 0)
      {
        lpf2ResumeTrainMotion();

        // also show press for led color button
        Button *lpf2Button = remoteButton[RemoteDevice::PoweredUp];
        for (int i = 0; i < remoteButtonCount[RemoteDevice::PoweredUp]; i++)
        {
          if (lpf2Button[i].action == Lpf2Color)
          {
            lpf2Button[i].pressed = true;
            break;
          }
        }
      }
      else
      {
        lpf2AutoAction = Lpf2Color;
      }
    }
  }
}

void lpf2DeviceCallback(void *hub, byte sensorPort, DeviceType deviceType, uint8_t *pData)
{
  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (deviceType != DeviceType::COLOR_DISTANCE_SENSOR)
  {
    return;
  }

  int distance = trainCtl->parseDistance(pData);
  Color color = (Color)(trainCtl->parseColor(pData));

  // track moving average of distance to random noisy spikes, turn off motor if not on track
  lpf2SensorDistanceMovingAverage = (lpf2SensorDistanceMovingAverage * 3 + distance) >> 2; // I bet compiler does this anyway for dividing by 4
  if (lpf2SensorDistanceMovingAverage > BtDistanceStopThreshold && lpf2PortSpeed[lpf2MotorPort] != 0)
  {
    log_w("off track, distance moving avg: %d", distance);

    lpf2AutoAction = Brake;
    lpf2SensorStopDelay = 0;
    lpf2SensorStopSavedSpd = 0;
    return;
  }

  // filter noise
  if (color == lpf2SensorColor || millis() < lpf2SensorDebounce)
  {
    return;
  }

  lpf2SensorColor = color;
  lpf2SensorDebounce = millis() + 200;
  redraw = true;

  // ignore "ground" or noisy colors
  if (isIgnoredColor(color))
  {
    return;
  }

  lpf2LedColor = color;
  trainCtl->setLedColor(color);

  // trigger actions for specific colors
  RemoteAction action;
  if (color == lpf2SensorSpdUpColor && lpf2SensorSpdUpFunction >= 0)
  {
    log_w("lpf2 action: spdup");
    lpf2AutoAction = SpdUp;
  }
  else if (color == lpf2SensorStopColor && lpf2SensorStopFunction >= 0)
  {
    // resumed by calling resumeTrainMotion() after delay
    log_w("lpf2 action: brake");
    lpf2AutoAction = Brake;
    lpf2SensorStopDelay = millis() + lpf2SensorStopFunction * 1000;
    lpf2SensorStopSavedSpd = lpf2PortSpeed[lpf2MotorPort];
  }
  else if (color == lpf2SensorSpdDnColor && lpf2SensorSpdDnFunction >= 0)
  {
    log_w("lpf2 action: spddn");
    lpf2AutoAction = SpdDn;
  }
  else
  {
    return;
  }

  // also show press for sensor button
  Button *lpf2Button = remoteButton[RemoteDevice::PoweredUp];
  for (int i = 0; i < remoteButtonCount[RemoteDevice::PoweredUp]; i++)
  {
    if (lpf2Button[i].action == lpf2AutoAction && lpf2Button[i].port == lpf2SensorPort)
    {
      lpf2Button[i].pressed = true;
      break;
    }
  }
}

void lpf2ConnectionToggle()
{
  log_d("btConnectionToggle");

  if (!lpf2Init)
  {
    if (!lpf2Hub.isConnecting())
    {
      lpf2Hub.init();
    }

    unsigned long exp = millis() + BtConnWait;
    while (!lpf2Hub.isConnecting() && exp > millis())
      ;

    if (lpf2Hub.isConnecting() && lpf2Hub.connectHub())
    {
      delay(200);

      lpf2Init = true;
      lpf2LedColor = Color::NONE;
      lpf2Hub.activateHubPropertyUpdate(HubPropertyReference::BUTTON, lpf2HubCallback);
      lpf2Hub.activateHubPropertyUpdate(HubPropertyReference::RSSI, lpf2HubCallback);
    }

    lpf2DisconnectDelay = millis() + 500;
  }
  else if (lpf2DisconnectDelay < millis())
  {
    lpf2Hub.shutDownHub();

    delay(200);

    lpf2Init = false;
    lpf2SensorInit = false;
    lpf2PortSpeed[0] = lpf2PortSpeed[1] = 0;
    lpf2MotorPort = NO_SENSOR_FOUND;
    lpf2SensorPort = NO_SENSOR_FOUND;
    lpf2LedColor = Color::NONE;
    lpf2SensorColor = Color::NONE;
    lpf2SensorStopDelay = 0;
  }
}

void lpf2HandlePortAction(Button *button)
{
  log_w("[d:%d][p:%d][a:%d]", button->device, button->port, button->action);

  if (!lpf2Hub.isConnected())
    return;

  if (button->port == lpf2SensorPort)
  {
    if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN) || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER))
    {
      switch (button->action)
      {
      case SpdUp:
        do
        {
          lpf2SensorSpdUpColor = (lpf2SensorSpdUpColor == Color::BLACK)
                                     ? Color(Color::NUM_COLORS - 1)
                                     : (Color)(lpf2SensorSpdUpColor - 1);
        } while (isIgnoredColor(lpf2SensorSpdUpColor) || lpf2SensorSpdUpColor == lpf2SensorSpdDnColor || lpf2SensorSpdUpColor == lpf2SensorStopColor);
        break;
      case Brake:
        do
        {
          lpf2SensorStopColor = (lpf2SensorStopColor == Color::BLACK)
                                    ? Color(Color::NUM_COLORS - 1)
                                    : (Color)(lpf2SensorStopColor - 1);
        } while (isIgnoredColor(lpf2SensorStopColor) || lpf2SensorStopColor == lpf2SensorSpdUpColor || lpf2SensorStopColor == lpf2SensorSpdDnColor);
        break;
      case SpdDn:
        do
        {
          lpf2SensorSpdDnColor = (lpf2SensorSpdDnColor == Color::BLACK)
                                     ? Color(Color::NUM_COLORS - 1)
                                     : (Color)(lpf2SensorSpdDnColor - 1);
        } while (isIgnoredColor(lpf2SensorSpdDnColor) || lpf2SensorSpdDnColor == lpf2SensorSpdUpColor || lpf2SensorSpdDnColor == lpf2SensorStopColor);
        break;
      }
    }
    else
    {
      switch (button->action)
      {
      case SpdUp:
        lpf2SensorSpdUpFunction = (lpf2SensorSpdUpFunction == 0) ? -1 : 0;
        break;
      case Brake:
        if (button->action == Brake)
        {
          if (lpf2SensorStopFunction == 0)
            lpf2SensorStopFunction = 2;
          else if (lpf2SensorStopFunction == 2)
            lpf2SensorStopFunction = 5;
          else if (lpf2SensorStopFunction == 5)
            lpf2SensorStopFunction = 9;
          else if (lpf2SensorStopFunction == 9)
            lpf2SensorStopFunction = -1;
          else
            lpf2SensorStopFunction = 0;

          // if currently waiting, reset delay to new setting or clear pause state
          if (lpf2SensorStopDelay > 0)
          {
            lpf2SensorStopDelay = lpf2SensorStopFunction > 0 ? millis() + (lpf2SensorStopFunction * 1000) : 0;
          }
        }
        break;
      case SpdDn:
        lpf2SensorSpdDnFunction = (lpf2SensorSpdDnFunction == 0) ? -1 : 0;
        break;
      }
    }
  }
  else
  {
    // if stopped and waiting and manual action, clear wait state
    if (lpf2SensorStopDelay > 0 && lpf2AutoAction == NoAction)
    {
      lpf2SensorStopDelay = 0;
      lpf2PortSpeed[button->port] = 0;
    }

    switch (button->action)
    {
    case SpdUp:
      lpf2PortSpeed[button->port] = min(BtMaxSpeed, lpf2PortSpeed[button->port] + BtSpdInc);
      break;
    case Brake:
      lpf2PortSpeed[button->port] = 0;
      break;
    case SpdDn:
      lpf2PortSpeed[button->port] = max(-BtMaxSpeed, lpf2PortSpeed[button->port] - BtSpdInc);
      break;
    }

    lpf2Hub.setBasicMotorSpeed(button->port, lpf2PortSpeed[button->port]);
  }
}

void lpf2Update()
{
  if (!lpf2Hub.isConnected())
  {
    lpf2Init = false;
    lpf2SensorInit = false;
    lpf2PortSpeed[0] = lpf2PortSpeed[1] = 0;
    lpf2MotorPort = NO_SENSOR_FOUND;
    lpf2SensorPort = NO_SENSOR_FOUND;
    lpf2SensorSpdUpColor = Color::GREEN;
    lpf2SensorStopColor = Color::RED;
    lpf2SensorSpdDnColor = Color::YELLOW;
    lpf2SensorStopFunction = 0;
    lpf2SensorStopDelay = 0;
    redraw = true;
  }
  else
  {
    if (lpf2LedColor == Color::NONE)
    {
      byte btLedPort = lpf2Hub.getPortForDeviceType((byte)DeviceType::HUB_LED);
      if (btLedPort != NO_SENSOR_FOUND)
      {
        lpf2LedColor = (Color)(1 + (millis() % (Color::NUM_COLORS - 1))); // "random" color
        lpf2Hub.setLedColor(lpf2LedColor);
      }
    }

    if (!lpf2SensorInit)
    {
      lpf2SensorPort = lpf2Hub.getPortForDeviceType((byte)DeviceType::COLOR_DISTANCE_SENSOR);
      if (lpf2SensorPort == (byte)PoweredUpHubPort::A || lpf2SensorPort == (byte)PoweredUpHubPort::B)
      {
        lpf2Hub.activatePortDevice(lpf2SensorPort, lpf2DeviceCallback);
        lpf2MotorPort = (lpf2SensorPort == (byte)PoweredUpHubPort::A) ? (byte)PoweredUpHubPort::B : (byte)PoweredUpHubPort::A;
        lpf2SensorColor = Color::NONE;
        lpf2SensorInit = true;
        redraw = true;
      }
    }
    // bt sensor delayed start after stop
    if (lpf2SensorStopFunction > 0 && lpf2SensorStopDelay > 0 && millis() > lpf2SensorStopDelay)
    {
      lpf2ResumeTrainMotion();

      // also show press for sensor button
      Button *lpf2Button = remoteButton[RemoteDevice::PoweredUp];
      for (int i = 0; i < remoteButtonCount[RemoteDevice::PoweredUp]; i++)
      {
        if (lpf2Button[i].action == Brake && lpf2Button[i].port == lpf2SensorPort)
        {
          lpf2Button[i].pressed = true;
          break;
        }
      }
    }

    // disconnect from hub if no activity (motor off with sensor or both motors off)
    if (millis() - lpf2LastAction > BtInactiveTimeoutMs && ((lpf2SensorInit && lpf2PortSpeed[lpf2MotorPort] == 0) || (lpf2PortSpeed[0] == 0 && lpf2PortSpeed[1] == 0)))
    {
      lpf2ConnectionToggle();
      redraw = true;
    }
  }
}

void powerFunctionsHandlePortAction(Button *button)
{
  if (button->action == PortFunction || M5Cardputer.Keyboard.isKeyPressed(KEY_FN) || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER))
  {
    irPortFunction[irChannel][button->port] = !irPortFunction[irChannel][button->port];
  }
  else
  {
    // switch functionality for IR port
    if (irPortFunction[irChannel][button->port])
    {
      // this detects when a stop action is triggered by the delay
      if (irAutoAction)
      {
        // we brake but don't update the speed to keep the "state" of the switch shown
        irTrainCtl.single_pwm((PowerFunctionsPort)button->port, PowerFunctionsPwm::BRAKE, irChannel);
        return;
      }

      // turn on motor to full and then set timer to turn off after some time
      switch (button->action)
      {
      case SpdUp:
        irPortSpeed[irChannel][button->port] = IrMaxSpeed;
        break;
      case Brake:
        irPortSpeed[irChannel][button->port] = irPortSpeed[irChannel][button->port] > 0 ? -IrMaxSpeed : IrMaxSpeed;
        ;
        break;
      case SpdDn:
        irPortSpeed[irChannel][button->port] = -IrMaxSpeed;
        break;
      }

      PowerFunctionsPwm pwm = irTrainCtl.speedToPwm(irPortSpeed[irChannel][button->port]);
      irTrainCtl.single_pwm((PowerFunctionsPort)button->port, pwm, irChannel);

      irSwitchDelay[button->port] = millis() + 1000;
    }
    else if (irMode == 1 || irMode == 2)
    {
      switch (button->action)
      {
      case SpdUp:
        irPortSpeed[irChannel][button->port] = min(IrMaxSpeed, irPortSpeed[irChannel][button->port] + IrSpdInc);
        break;
      case Brake:
        irPortSpeed[irChannel][button->port] = 0;
        break;
      case SpdDn:
        irPortSpeed[irChannel][button->port] = max(-IrMaxSpeed, irPortSpeed[irChannel][button->port] - IrSpdInc);
        break;
      }

      PowerFunctionsPwm pwm = irTrainCtl.speedToPwm(irPortSpeed[irChannel][button->port]);
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
  }
}

void powerFunctionsHandleBroadcastAction(byte recvChannel, RemoteAction action, byte port)
{
  if (irPortFunction[recvChannel][port])
  {
    switch (action)
    {
    case SpdUp:
      irPortSpeed[recvChannel][port] = IrMaxSpeed;
      break;
    case Brake:
      irPortSpeed[recvChannel][port] = irPortSpeed[recvChannel][port] > 0 ? IrMaxSpeed : -IrMaxSpeed;
      break;
    case SpdDn:
      irPortSpeed[recvChannel][port] = -IrMaxSpeed;
      break;
    }
  }
  else
  {
    switch (action)
    {
    case SpdUp:
      irPortSpeed[recvChannel][port] = min(IrMaxSpeed, irPortSpeed[recvChannel][port] + IrSpdInc);
      break;
    case Brake:
      irPortSpeed[recvChannel][port] = 0;
      break;
    case SpdDn:
      irPortSpeed[recvChannel][port] = max(-IrMaxSpeed, irPortSpeed[recvChannel][port] - IrSpdInc);
      break;
    }
  }
}

void powerFunctionsHandleBroadcastAction(Button *button)
{
  button->pressed = true;
  powerFunctionsHandleBroadcastAction(irChannel, button->action, button->port);
}

void powerFunctionsRecvCallback(PowerFunctionsIrMessage receivedMessage)
{
  RemoteAction buttonAction;
  switch (receivedMessage.call)
  {
  case PowerFunctionsCall::SingleDecrement:
    buttonAction = SpdDn;
    break;
  case PowerFunctionsCall::SingleIncrement:
    buttonAction = SpdUp;
    break;
  case PowerFunctionsCall::SinglePwm:
    switch (receivedMessage.pwm)
    {
    case PowerFunctionsPwm::FLOAT:
      // switchState = existing state
      break;
    case PowerFunctionsPwm::BRAKE:
      buttonAction = Brake;
      break;
    case PowerFunctionsPwm::FORWARD1:
    case PowerFunctionsPwm::FORWARD2:
    case PowerFunctionsPwm::FORWARD3:
    case PowerFunctionsPwm::FORWARD4:
    case PowerFunctionsPwm::FORWARD5:
    case PowerFunctionsPwm::FORWARD6:
    case PowerFunctionsPwm::FORWARD7:
      buttonAction = SpdUp;
      break;
    case PowerFunctionsPwm::REVERSE1:
    case PowerFunctionsPwm::REVERSE2:
    case PowerFunctionsPwm::REVERSE3:
    case PowerFunctionsPwm::REVERSE4:
    case PowerFunctionsPwm::REVERSE5:
    case PowerFunctionsPwm::REVERSE6:
    case PowerFunctionsPwm::REVERSE7:
      buttonAction = SpdDn;
      break;
    }
    break;
  }

  // if receiving on channel different than active, update state in the background otherwise send as action to show on screen
  if (receivedMessage.channel != irChannel)
  {
    powerFunctionsHandleBroadcastAction(receivedMessage.channel, buttonAction, (byte)receivedMessage.port);
    return;
  }
  else
  {
    irAutoAction = buttonAction;
    irAutoActionPort = (byte)receivedMessage.port;
    irActionBroadcastRecv = true;
  }
}

void powerFunctionsUpdate()
{
  for (auto irPort : {(byte)PowerFunctionsPort::RED, (byte)PowerFunctionsPort::BLUE})
  {
    if (irSwitchDelay[irPort] > 0 && irSwitchDelay[irPort] < millis())
    {
      irSwitchDelay[irPort] = 0;
      irAutoAction = Brake;
      irAutoActionPort = irPort;
      break;
    }
  }
}

void sbrickMotionSensorCallback(void *hub, byte channel, float voltage)
{
  SBrickHub *sbrickHub = (SBrickHub *)hub;

  byte port = SBrickAdcChannelToPort[channel];
  if (port != sbrickMotionSensorPort)
  {
    log_w("port %d not motion sensor", port);
    return;
  }

  if (voltage < 0.01)
  {
    return;
  }

  if (sbrickMotionSensorNeutralV < 0.0)
  {
    sbrickMotionSensorNeutralV = voltage;
    return;
  }

  sbrickMotionSensorV = voltage;
  byte motion = sbrickHub->interpretSensorMotion(voltage, sbrickMotionSensorNeutralV);

  // filter noise
  if (motion == sbrickSensorMotion)
  {
    return;
  }

  sbrickSensorMotion = motion;
  redraw = true;

  if (motion == 0)
  {
    return;
  }

  log_d("motion: %d", motion);
  sbrickAutoAction = Brake;
}

void sbrickTiltSensorCallback(void *hub, byte channel, float voltage)
{
  SBrickHub *sbrickHub = (SBrickHub *)hub;

  byte port = SBrickAdcChannelToPort[channel];
  if (port != sbrickTiltSensorPort)
  {
    log_w("port %d not tilt sensor", port);
    return;
  }

  if (voltage < 0.01)
  {
    return;
  }

  if (sbrickTiltSensorNeutralV < 0.0)
  {
    sbrickTiltSensorNeutralV = voltage;
    return;
  }

  sbrickTiltSensorV = voltage;
  WedoTilt tilt = (WedoTilt)sbrickHub->interpretSensorTilt(voltage, sbrickTiltSensorNeutralV);

  // filter noise
  if (tilt == sbrickSensorTilt)
  {
    return;
  }

  sbrickSensorTilt = tilt;
  redraw = true;

  if (tilt == WedoTilt::Neutral)
  {
    return;
  }

  log_d("tilt: %d", tilt);
  sbrickAutoAction = Brake;
}

void sbrickConnectionToggle()
{
  if (!sbrickInit)
  {
    if (!sbrickHub.isConnecting())
    {
      log_w("init sbrick");
      sbrickHub.init();
    }

    unsigned long exp = millis() + BtConnWait;
    while (!sbrickHub.isConnecting() && exp > millis())
      ;

    if (!sbrickHub.isConnecting())
    {
      log_w("sbrick not connecting");
      return;
    }

    if (sbrickHub.isConnecting() && sbrickHub.connectHub())
    {
      log_w("connected to sbrick");
      sbrickInit = true;

      // detect sensors and subscribe to them
      for (byte port = 0; port < 4; port++)
      {
        byte detectedSensor = sbrickHub.detectPortSensor(port);

        if (detectedSensor == (byte)SBrickDevice::MotionSensor && !sbrickMotionSensorInit)
        {
          log_w("sbrick port %d detected as motion sensor %d", port, detectedSensor);
          sbrickHub.subscribeSensor(port, sbrickMotionSensorCallback);
          sbrickMotionSensorInit = true;
          sbrickMotionSensorPort = port;
          sbrickSensorMotion = false;
        }
        else if (detectedSensor == (byte)SBrickDevice::TiltSensor && !sbrickTiltSensorInit)
        {
          log_w("sbrick port %d detected as tilt sensor %d", port, detectedSensor);
          sbrickHub.subscribeSensor(port, sbrickTiltSensorCallback);
          sbrickTiltSensorInit = true;
          sbrickTiltSensorPort = port;
          sbrickSensorTilt = WedoTilt::Neutral;
        }
      }

      if (sbrickHub.getWatchdogTimeout() != 11)
      {
        log_w("setting sbrick watchdog to 1.1s");
        sbrickHub.setWatchdogTimeout(11); // we read at least 1/s so 1.1s should be safe
      }

      sbrickDisconnectDelay = millis() + 500;
    }
  }
  else if (sbrickDisconnectDelay < millis())
  {
    log_w("disconnecting sbrick");
    sbrickHub.disconnectHub();
    delay(200);
    sbrickInit = false;
    sbrickPortSpeed[0] = sbrickPortSpeed[1] = sbrickPortSpeed[2] = sbrickPortSpeed[3] = 0;
    sbrickMotionSensorInit = false;
    sbrickMotionSensorPort = NO_SENSOR_FOUND;
    sbrickTiltSensorInit = false;
    sbrickTiltSensorPort = NO_SENSOR_FOUND;
  }
}

void sbrickHandlePortAction(Button *button)
{
  if (!sbrickHub.isConnected())
    return;

  if (button->action == PortFunction || M5Cardputer.Keyboard.isKeyPressed(KEY_FN) || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER))
  {
    // determine which port to move to foreground
    byte portToBg = button->port;
    byte portToFg = button->port;
    do
    {
      portToFg = (portToFg + 1) % 4;
    } while (portToFg == sbrickLeftPort || portToFg == sbrickRightPort);

    // update corresponding port
    if (portToBg == sbrickLeftPort)
    {
      sbrickLeftPort = portToFg;
    }
    else
    {
      sbrickRightPort = portToFg;
    }

    // then update all buttons to reflect new state
    for (int i = 0; i < sbrickButtonCount; i++)
    {
      if (sbrickHubButtons[i].port == portToBg)
      {
        sbrickHubButtons[i].port = portToFg;
      }
    }
  }
  else
  {
    // can recalibrate/reset neutral V by pressing spddn button for port with sensor
    if (button->port == sbrickMotionSensorPort)
    {
      if (button->action == SpdDn)
        sbrickMotionSensorNeutralV = sbrickMotionSensorV;
      return;
    }
    else if (button->port == sbrickTiltSensorPort)
    {
      if (button->action == SpdDn)
        sbrickTiltSensorNeutralV = sbrickTiltSensorV;
      return;
    }

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
}

void sbrickUpdate()
{
  if (!sbrickHub.isConnected())
  {
    sbrickInit = false;
    sbrickPortSpeed[0] = sbrickPortSpeed[1] = sbrickPortSpeed[2] = sbrickPortSpeed[3] = 0;
    sbrickMotionSensorInit = false;
    sbrickMotionSensorPort = NO_SENSOR_FOUND;
    sbrickTiltSensorInit = false;
    sbrickTiltSensorPort = NO_SENSOR_FOUND;
    redraw = true;
  }
  else
  {
    int newRssi = sbrickHub.getRssi();
    if (newRssi != sbrickRssi)
    {
      sbrickRssi = newRssi;
      redraw = true;
    }

    float newBatteryV = sbrickHub.getBatteryLevel();
    if (newBatteryV != sbrickBatteryV)
    {
      sbrickBatteryV = newBatteryV;
      redraw = true;
    }

    float newTempC = sbrickHub.getTemperature();
    if (newTempC != sbrickTempF)
    {
      sbrickTempF = newTempC;
      redraw = true;
    }
  }
}

void circuitCubesConnectionToggle()
{
  log_d("circuitCubesConnectionToggle");

  if (!circuitCubesInit)
  {
    if (!circuitCubesHub.isConnecting())
    {
      circuitCubesHub.init();
    }

    unsigned long exp = millis() + BtConnWait;
    while (!circuitCubesHub.isConnecting() && exp > millis())
      ;

    if (circuitCubesHub.isConnecting() && circuitCubesHub.connectHub())
    {
      circuitCubesInit = true;
    }

    circuitCubesDisconnectDelay = millis() + 500;
  }
  else if (circuitCubesDisconnectDelay < millis())
  {
    circuitCubesHub.disconnectHub();
    delay(200);
    circuitCubesInit = false;
    circuitCubesPortSpeed[0] = circuitCubesPortSpeed[1] = circuitCubesPortSpeed[2] = 0;
  }
}

void circuitCubesHandlePortAction(Button *button)
{
  if (!circuitCubesHub.isConnected())
    return;

  if (button->action == PortFunction || M5Cardputer.Keyboard.isKeyPressed(KEY_FN) || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER))
  {
    // determine which port to move to foreground
    byte portToBg = button->port;
    byte portToFg = (byte)CircuitCubesHubPort::A;
    for (byte port : {(byte)CircuitCubesHubPort::B, (byte)CircuitCubesHubPort::C})
    {
      if (port != circuitCubesLeftPort && port != circuitCubesRightPort)
      {
        portToFg = port;
        break;
      }
    }

    // update corresponding port
    if (portToBg == circuitCubesLeftPort)
    {
      circuitCubesLeftPort = portToFg;
    }
    else
    {
      circuitCubesRightPort = portToFg;
    }

    // then update all buttons to reflect new state
    for (int i = 0; i < circuitCubesButtonCount; i++)
    {
      if (circuitCubesButtons[i].port == portToBg)
      {
        circuitCubesButtons[i].port = portToFg;
      }
    }
  }
  else
  {
    switch (button->action)
    {
    case SpdUp:
      circuitCubesPortSpeed[button->port] = min(CIRCUIT_CUBES_SPEED_MAX, circuitCubesPortSpeed[button->port] + circuitCubesSpdInc);
      break;
    case Brake:
      circuitCubesPortSpeed[button->port] = 0;
      break;
    case SpdDn:
      circuitCubesPortSpeed[button->port] = max(CIRCUIT_CUBES_SPEED_MIN, circuitCubesPortSpeed[button->port] - circuitCubesSpdInc);
      break;
    }

    circuitCubesHub.setMotorSpeed(button->port, circuitCubesPortSpeed[button->port]);
  }
}

void circuitCubesUpdate()
{
  if (!circuitCubesHub.isConnected())
  {
    circuitCubesInit = false;
    circuitCubesPortSpeed[0] = circuitCubesPortSpeed[1] = circuitCubesPortSpeed[2] = 0;
    redraw = true;
  }
  else
  {
    int newRssi = circuitCubesHub.getRssi();
    if (newRssi != circuitCubesRssi)
    {
      circuitCubesRssi = newRssi;
      redraw = true;
    }

    float newBatteryV = circuitCubesHub.getBatteryLevel();
    if (newBatteryV != circuitCubesBatteryV)
    {
      circuitCubesBatteryV = newBatteryV;
      redraw = true;
    }
  }
}

bool sdCardInit()
{
  if (sdInit)
  {
    return true;
  }

  uint8_t retries = 3;
  SPI2.begin(M5.getPin(m5::pin_name_t::sd_spi_sclk),
             M5.getPin(m5::pin_name_t::sd_spi_miso),
             M5.getPin(m5::pin_name_t::sd_spi_mosi),
             M5.getPin(m5::pin_name_t::sd_spi_ss));
  while (!(sdInit = SD.begin(M5.getPin(m5::pin_name_t::sd_spi_ss), SPI2)) && retries-- > 0)
  {
    delay(100);
  }

  if (!sdInit)
  {
    log_w("SD card init failed");
  }

  return sdInit;
}

void saveScreenshot()
{
  if (!sdCardInit())
  {
    return;
  }

  size_t pngLen;
  // TODO: memory issue? can't take whole screnshot
  uint8_t *pngBytes = (uint8_t *)M5Cardputer.Display.createPng(&pngLen, 0, 0, 240, 135);

  delay(5000);

  if (pngLen == 0)
  {
    log_w("screenshot has 0 bytes, trying half screen");
    pngBytes = (uint8_t *)M5Cardputer.Display.createPng(&pngLen, 0, 0, 120, 135);

    if (pngLen == 0)
    {
      log_w("screenshot has 0 bytes, giving up");
      return;
    }
  }

  int i = 0;
  String filename;
  do
  {
    filename = "/screenshot." + String(i++) + ".png";
  } while (SD.exists(filename));

  File file = SD.open(filename, FILE_WRITE);
  if (file)
  {
    file.write(pngBytes, pngLen);
    file.flush();
    file.close();
    log_w("saved screenshot to %s, %d bytes", filename.c_str(), pngLen);
  }
  else
  {
    log_w("cannot save screenshot to file %s, %d bytes", filename.c_str(), pngLen);
  }

  free(pngBytes);
}

void handleRemoteButtonPress(Button *button)
{
  log_w("[d:%d][p:%d][a:%d]", button->device, button->port, button->action);

  button->pressed = true;

  switch (button->action)
  {
  case BtConnection:
    switch (button->device)
    {
    case RemoteDevice::PoweredUp:
      lpf2ConnectionToggle();
      break;
    case RemoteDevice::SBrick:
      sbrickConnectionToggle();
      break;
    case RemoteDevice::CircuitCubes:
      circuitCubesConnectionToggle();
      break;
    }
    break;
  case Lpf2Color:
    // function like hub button
    if (lpf2SensorStopDelay > 0)
    {
      lpf2ResumeTrainMotion();
    }
    else
    {
      // can change colors while not connected to choose initial color
      lpf2LedColor = (lpf2LedColor == Color(1))
                         ? Color(Color::NUM_COLORS - 1)
                         : (Color)(lpf2LedColor - 1);
      log_w("bt color: %d", lpf2LedColor);
      if (lpf2Hub.isConnected())
        lpf2Hub.setLedColor(lpf2LedColor);
    }
    break;
  case IrChannel:
    irChannel = (irChannel + 1) % 4;
    break;
  case IrMode:
    irMode = (irMode + 1) % 4;

    if (irMode == 1 || irMode == 3) // reset after on and off if not in switch mode
    {
      for (byte i = 0; i < 4; i++)
      {
        if (!irPortFunction[i][0])
        {
          irPortSpeed[i][0] = 0;
        }

        if (!irPortFunction[i][1])
        {
          irPortSpeed[i][1] = 0;
        }
      }
    }

    if (irMode == 2)
    {
      irTrainCtl.enableBroadcast();
      irTrainCtl.registerRecvCallback(powerFunctionsRecvCallback);
    }
    else if (irMode == 0)
    {
      irTrainCtl.unregisterRecvCallback();
      irTrainCtl.disableBroadcast();
    }
    break;
  case PortFunction:
  case SpdUp:
  case Brake:
  case SpdDn:
    switch (button->device)
    {
    case RemoteDevice::PoweredUp:
      lpf2HandlePortAction(button);
      break;
    case RemoteDevice::PowerFunctionsIR:
      powerFunctionsHandlePortAction(button);
      break;
    case RemoteDevice::SBrick:
      sbrickHandlePortAction(button);
      break;
    case RemoteDevice::CircuitCubes:
      circuitCubesHandlePortAction(button);
      break;
    }
    break;
  }
}

unsigned short getButtonColor(Button *button)
{
  if (button->pressed)
  {
    return COLOR_ORANGE;
  }

  if (button->device == RemoteDevice::PoweredUp)
  {
    if (button->port == lpf2SensorPort)
    {
      switch (button->action)
      {
      case SpdUp:
        return interpolateColors(COLOR_LIGHTGRAY, BtColors[lpf2SensorSpdUpColor], 50);
      case Brake:
        return interpolateColors(COLOR_LIGHTGRAY, BtColors[lpf2SensorStopColor], 50);
      case SpdDn:
        return interpolateColors(COLOR_LIGHTGRAY, BtColors[lpf2SensorSpdDnColor], 50);
      default:
        return button->color;
      }
    }

    if (button->action == Lpf2Color)
    {
      return interpolateColors(COLOR_LIGHTGRAY, BtColors[lpf2LedColor], 50);
    }

    if (button->port == (byte)PoweredUpHubPort::LED)
    {
      if (lpf2SensorInit)
        return BtColors[lpf2SensorColor];
    }
  }

  if (button->device == (byte)RemoteDevice::PowerFunctionsIR)
  {
    if (button->action == IrMode && irMode > 0)
    {
      return COLOR_GREYORANGEDIM;
    }

    if (irPortFunction[irChannel][button->port])
    {
      if (button->action == SpdUp && irPortSpeed[irChannel][button->port] > 0 ||
          button->action == SpdDn && irPortSpeed[irChannel][button->port] < 0)
      {
        return irSwitchDelay[button->port] > 0 ? COLOR_GREYORANGEBRIGHT : COLOR_GREYORANGEDIM;
      }

      if (button->action == Brake)
      {
        return button->color;
      }
    }
  }

  if (button->device == RemoteDevice::SBrick)
  {
    if (button->port == sbrickMotionSensorPort)
    {
      return sbrickSensorMotion ? COLOR_GREYORANGEDIM : button->color;
    }
    else if (button->port == sbrickTiltSensorPort)
    {
      return sbrickSensorTilt != WedoTilt::Neutral ? COLOR_GREYORANGEDIM : button->color;
    }
  }

  if (button->action == Brake)
  {
    if (button->device == RemoteDevice::PoweredUp && lpf2PortSpeed[button->port] == 0 ||
        button->device == RemoteDevice::SBrick && sbrickPortSpeed[button->port] == 0 ||
        button->device == RemoteDevice::CircuitCubes && circuitCubesPortSpeed[button->port] == 0 ||
        button->device == RemoteDevice::PowerFunctionsIR && (irMode == 1 || irMode == 2) && irPortSpeed[irChannel][button->port] == 0)
    {
      return COLOR_GREYORANGEDIM;
    }
  }

  if (button->action == SpdUp || button->action == SpdDn)
  {
    int speed = 0;
    switch (button->device)
    {
    case RemoteDevice::PoweredUp:
      speed = lpf2PortSpeed[button->port];
      break;
    case RemoteDevice::SBrick:
      speed = sbrickPortSpeed[button->port];
      break;
    case RemoteDevice::CircuitCubes:
      speed = circuitCubesPortSpeed[button->port];
      break;
    case RemoteDevice::PowerFunctionsIR:
      speed = (irMode == 1 || irMode == 2) ? irPortSpeed[irChannel][button->port] : 0;
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

  // left port
  if (M5Cardputer.Keyboard.isKeyPressed('3'))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::LeftPortFunction;
  }
  if (M5Cardputer.Keyboard.isKeyPressed('8'))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::LeftPortFunction;
  }
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
  else if (M5Cardputer.Keyboard.isKeyPressed('4'))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::RightPortFunction;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('9'))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::RightPortFunction;
  }
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

  // aux
  else if (M5Cardputer.Keyboard.isKeyPressed('`'))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::AuxOne;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::AuxOne;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::AuxTwo;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('\\'))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::AuxTwo;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_FN))
  {
    isLeftRemote = true;
    pressedKey = RemoteKey::AuxFunction;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER))
  {
    isLeftRemote = false;
    pressedKey = RemoteKey::AuxFunction;
  }

  return pressedKey != RemoteKey::NoTouchy;
}

String getRemoteAuxOneLabel(bool isLeftRemote, RemoteDevice remote, int &y)
{
  switch (remote)
  {
  case RemoteDevice::PoweredUp:
    y = r1_5 - 2;
    return "BT";
  case RemoteDevice::PowerFunctionsIR:
    y = r1_5 - 2;
    return "CH";
  case RemoteDevice::SBrick:
  case RemoteDevice::CircuitCubes:
    y = r2 - 2;
    return "BT";
  default:
    return "";
  }
}

String getRemoteAuxTwoLabel(bool isLeftRemote, RemoteDevice remote, int &y)
{
  y = r2_5 + bw + 11; // TODO: incorporate font height?

  switch (remote)
  {
  case RemoteDevice::PoweredUp:
    return lpf2SensorStopDelay > 0 ? "GO" : "LED";
  case RemoteDevice::PowerFunctionsIR:
    return "MODE";
  default:
    return "";
  }
}

String getRemotePortString(bool isLeftPort, RemoteDevice remote)
{
  switch (remote)
  {
  case RemoteDevice::PoweredUp:
    return isLeftPort ? "A" : "B";
  case RemoteDevice::PowerFunctionsIR:
    return isLeftPort ? "RED" : "BLUE";
  case RemoteDevice::SBrick:
    return isLeftPort ? String(SBrickPortToChar[sbrickLeftPort]) : String(SBrickPortToChar[sbrickRightPort]);
  case RemoteDevice::CircuitCubes:
    return isLeftPort ? String(CircuitCubesPortToChar[circuitCubesLeftPort]) : String(CircuitCubesPortToChar[circuitCubesRightPort]);
  default:
    return "?";
  }
}

int getButtonX(RemoteColumn col, bool isLeftRemote)
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

int getButtonY(RemoteRow row)
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
  case Row0:
    return ry + 2 * om;
  default:
    return -h;
  }
}

void handleKeyboardInput(bool &actionTaken)
{
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed() && millis() - lastKeyPressMillis > KeyboardDebounce)
  {
    lastKeyPressMillis = millis();

    // handle remote key
    bool isLeftRemote;
    RemoteKey remoteKeyPressed;
    if (getPressedRemoteKey(remoteKeyPressed, isLeftRemote))
    {
      int activeRemote = isLeftRemote ? activeRemoteLeft : activeRemoteRight;

      for (int i = 0; i < remoteButtonCount[activeRemote]; i++)
      {
        if (remoteButton[activeRemote][i].key == remoteKeyPressed)
        {
          handleRemoteButtonPress(&remoteButton[activeRemote][i]);
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

    // save settings
    if (M5Cardputer.Keyboard.isKeyPressed('v'))
    {
      saveSettings();
    }
  }

  // take screenshot
  if (M5Cardputer.BtnA.isPressed())
  {
    saveScreenshot();
  }
}

void handleRemoteInput(bool &actionTaken)
{
  // sensor or button triggered action
  if (lpf2AutoAction != NoAction)
  {
    log_w("lpf2 auto action: %d", lpf2AutoAction);

    Button *lpf2Button = remoteButton[RemoteDevice::PoweredUp];
    for (int i = 0; i < remoteButtonCount[RemoteDevice::PoweredUp]; i++)
    {
      if (lpf2Button[i].action == lpf2AutoAction && (lpf2Button[i].port == 0xFF || lpf2Button[i].port == lpf2MotorPort))
      {
        handleRemoteButtonPress(&lpf2Button[i]);
        actionTaken = true;
      }
    }

    lpf2AutoAction = NoAction;
  }

  if (irAutoAction != NoAction)
  {
    Button *irButton = remoteButton[RemoteDevice::PowerFunctionsIR];
    for (int i = 0; i < remoteButtonCount[RemoteDevice::PowerFunctionsIR]; i++)
    {
      if (irButton[i].action == irAutoAction && irButton[i].port == irAutoActionPort)
      {
        if (irActionBroadcastRecv)
        {
          powerFunctionsHandleBroadcastAction(&irButton[i]);
        }
        else
        {
          handleRemoteButtonPress(&irButton[i]);
        }
        actionTaken = true;
      }
    }

    irAutoAction = NoAction;
    irAutoActionPort = 0xFF;
    irActionBroadcastRecv = false;
  }

  if (sbrickAutoAction != NoAction)
  {
    Button *sbrickButton = remoteButton[RemoteDevice::SBrick];
    for (int i = 0; i < remoteButtonCount[RemoteDevice::SBrick]; i++)
    {
      if (sbrickButton[i].action == sbrickAutoAction && sbrickPortSpeed[sbrickButton[i].port] != 0)
      {
        handleRemoteButtonPress(&sbrickButton[i]);
        actionTaken = true;
      }
    }

    sbrickAutoAction = NoAction;
  }
}

void draw()
{
  canvas.fillSprite(m5gfx::ili9341_colors::BLACK);

  // draw background, sidebar, header graphics
  canvas.fillRoundRect(hx, hy, hw, hh, 6, COLOR_DARKGRAY);
  canvas.fillRoundRect(rx, ry, rw, rh, 6, COLOR_DARKGRAY);
  canvas.fillRoundRect(lrx, lry, lrw, lrh, 6, COLOR_MEDGRAY);
  canvas.fillRoundRect(rrx, rry, rrw, rrh, 6, COLOR_MEDGRAY);

  canvas.setTextColor(TFT_SILVER);
  canvas.setTextDatum(middle_center);
  canvas.setTextSize(1);
  canvas.drawString("Lego Train Control", w / 2, hy + hh / 2, &fonts::Font2);

  // system bar indicators
  drawActiveRemoteIndicator(&canvas, 2 * om, hy + (hh / 2), activeRemoteLeft, activeRemoteRight);
  drawBatteryIndicator(&canvas, w - 34, hy + (hh / 2), batteryPct);

  // draw active remote titles
  drawRemoteTitle(&canvas, true, activeRemoteLeft, lrtx, lrty);
  drawRemoteTitle(&canvas, false, activeRemoteRight, rrtx, rrty);

  //  x = isLeftRemote ? c1 + bw / 2 : c6 + bw / 2;

  // draw port labels
  int auxY;

  canvas.setTextDatum(bottom_center);
  canvas.setTextSize(1);
  canvas.drawString(getRemoteAuxOneLabel(true, activeRemoteLeft, auxY), c1 + bw / 2, auxY);
  canvas.drawString(getRemoteAuxTwoLabel(true, activeRemoteLeft, auxY), c1 + bw / 2, auxY);

  canvas.drawString(getRemotePortString(true, activeRemoteLeft), c2 + bw / 2, r1 - 2);
  canvas.drawString(getRemotePortString(false, activeRemoteLeft), c3 + bw / 2, r1 - 2);
  canvas.drawString(getRemotePortString(true, activeRemoteRight), c4 + bw / 2, r1 - 2);
  canvas.drawString(getRemotePortString(false, activeRemoteRight), c5 + bw / 2, r1 - 2);

  canvas.drawString(getRemoteAuxOneLabel(false, activeRemoteRight, auxY), c6 + bw / 2, auxY);
  canvas.drawString(getRemoteAuxTwoLabel(false, activeRemoteRight, auxY), c6 + bw / 2, auxY);

  // power functions distance data
  // int lpf2DistanceX = 0;
  // if (lpf2SensorInit)
  // {
  //   if (activeRemoteLeft == RemoteDevice::PoweredUp)
  //   {
  //     lpf2DistanceX = c1;
  //   }
  //   else if (activeRemoteRight == RemoteDevice::PoweredUp)
  //   {
  //     lpf2DistanceX = c6;
  //   }

  //   if (lpf2DistanceX)
  //   {
  //     unsigned short distanceColor = lpf2SensorDistanceMovingAverage > BtDistanceStopThreshold ? TFT_RED : TFT_SILVER;
  //     canvas.setTextColor(distanceColor);
  //     canvas.drawString(String(lpf2SensorDistanceMovingAverage) + "cm", lpf2DistanceX + bw / 2, r1 - 2);
  //   }
  // }

  // sbrick sensor data
  int sbrickDataCol = 0;
  if (sbrickInit)
  {
    if (activeRemoteLeft == RemoteDevice::SBrick)
    {
      sbrickDataCol = c1;
    }
    else if (activeRemoteRight == RemoteDevice::SBrick)
    {
      sbrickDataCol = c6;
    }

    if (sbrickDataCol)
    {
      canvas.drawString(String(sbrickBatteryV, 1) + "V", sbrickDataCol + bw / 2, r1_5 - 13);
      canvas.drawString(String(sbrickTempF, 0) + "C", sbrickDataCol + bw / 2, r1_5 - 2);

      // if (sbrickMotionSensorInit)
      // {
      //   canvas.drawString(String(motionV, 2), sbrickDataCol + bw, r3_5 - 2);
      // }

      // if (sbrickTiltSensorInit)
      // {
      //   canvas.drawString(String(tiltV, 2), sbrickDataCol + bw, r3_5 + 9);
      // }

      if (sbrickMotionSensorInit || sbrickTiltSensorInit)
      {
        int tiltW = 12;
        int tiltPL = 3;
        int tiltPW = 3;
        int tiltT = 2;
        int tiltX = sbrickDataCol + bw / 2;
        int tiltY = r3 + bw / 2;
        int indicatorColor = sbrickSensorMotion ? TFT_RED : TFT_SILVER;

        switch (sbrickSensorTilt)
        {
        case WedoTilt::Forward:
        case WedoTilt::Backward:
          canvas.fillRect(tiltX - tiltW / 2, tiltY - tiltT / 2, tiltW, tiltT, indicatorColor);
          canvas.fillRect(tiltX - tiltT / 2, tiltY - tiltW / 2, tiltT, tiltW, TFT_RED);
          break;
        case WedoTilt::Left:
        case WedoTilt::Right:
          canvas.fillRect(tiltX - tiltT / 2, tiltY - tiltW / 2, tiltT, tiltW, indicatorColor);
          canvas.fillRect(tiltX - tiltW / 2, tiltY - tiltT / 2, tiltW, tiltT, TFT_RED);
          break;
        case WedoTilt::Neutral:
          canvas.fillRect(tiltX - tiltT / 2, tiltY - tiltW / 2, tiltT, tiltW, indicatorColor);
          canvas.fillRect(tiltX - tiltW / 2, tiltY - tiltT / 2, tiltW, tiltT, indicatorColor);
        }

        canvas.fillTriangle(tiltX, tiltY - tiltW / 2 - tiltPL, tiltX - tiltPW, tiltY - tiltW / 2, tiltX + tiltPW, tiltY - tiltW / 2,
                            sbrickSensorTilt == WedoTilt::Forward ? TFT_RED : indicatorColor);
        canvas.fillTriangle(tiltX, tiltY + tiltW / 2 + tiltPL, tiltX - tiltPW, tiltY + tiltW / 2, tiltX + tiltPW, tiltY + tiltW / 2,
                            sbrickSensorTilt == WedoTilt::Backward ? TFT_RED : indicatorColor);
        canvas.fillTriangle(tiltX - tiltW / 2 - tiltPL, tiltY, tiltX - tiltW / 2, tiltY - tiltPW, tiltX - tiltW / 2, tiltY + tiltPW,
                            sbrickSensorTilt == WedoTilt::Left ? TFT_RED : indicatorColor);
        canvas.fillTriangle(tiltX + tiltW / 2 + tiltPL, tiltY, tiltX + tiltW / 2, tiltY - tiltPW, tiltX + tiltW / 2, tiltY + tiltPW,
                            sbrickSensorTilt == WedoTilt::Right ? TFT_RED : indicatorColor);
      }
    }
  }

  // circuit cubes battery data
  int circuitCubesDataX = 0;
  if (circuitCubesInit)
  {
    if (activeRemoteLeft == RemoteDevice::CircuitCubes)
    {
      circuitCubesDataX = c1;
    }
    else if (activeRemoteRight == RemoteDevice::CircuitCubes)
    {
      circuitCubesDataX = c6;
    }

    if (circuitCubesDataX)
    {
      canvas.drawString(String(circuitCubesBatteryV, 1) + "V", circuitCubesDataX + bw / 2, r1_5 - 7);
    }
  }

  // draw layout for both active remotes
  State state = {lpf2Init, lpf2Rssi,
                 lpf2SensorColor, lpf2SensorPort,
                 lpf2SensorSpdUpColor, lpf2SensorStopColor, lpf2SensorSpdDnColor,
                 lpf2SensorSpdUpFunction, lpf2SensorStopFunction, lpf2SensorSpdDnFunction,
                 sbrickInit, sbrickRssi,
                 sbrickMotionSensorPort, sbrickMotionSensorV, sbrickMotionSensorNeutralV,
                 sbrickTiltSensorPort, sbrickTiltSensorV, sbrickTiltSensorNeutralV,
                 circuitCubesInit, circuitCubesRssi,
                 irChannel, irMode, irPortFunction[irChannel]};

  Button *leftRemoteButton = remoteButton[activeRemoteLeft];
  for (int i = 0; i < remoteButtonCount[activeRemoteLeft]; i++)
  {
    Button button = leftRemoteButton[i];
    unsigned short color = getButtonColor(&button);
    int x = getButtonX(button.col, true);
    int y = getButtonY(button.row);
    canvas.fillRoundRect(x, y, button.w, button.h, 3, color);
    drawButtonSymbol(&canvas, button, x, y, state);
  }

  Button *rightRemoteButton = remoteButton[activeRemoteRight];
  for (int i = 0; i < remoteButtonCount[activeRemoteRight]; i++)
  {
    Button button = rightRemoteButton[i];
    unsigned short color = getButtonColor(&button);
    int x = getButtonX(button.col, false);
    int y = getButtonY(button.row);
    canvas.fillRoundRect(x, y, button.w, button.h, 3, color);
    drawButtonSymbol(&canvas, button, x, y, state);
  }

  canvas.pushSprite(0, 0);
}

void setup()
{
  USBSerial.begin(115200);

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(brightness);
  canvas.createSprite(w, h);

  // workaround for Legoino PowerFunctions ctor (don't want to put on heap)
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);

  loadSettings();

  draw();
}

void loop()
{
  bool actionTaken = false;
  handleKeyboardInput(actionTaken);
  handleRemoteInput(actionTaken);

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

    lpf2LastAction = millis();
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

    if (lpf2Init)
    {
      lpf2Update();
    }

    powerFunctionsUpdate();

    if (sbrickInit)
    {
      sbrickUpdate();
    }

    if (circuitCubesInit)
    {
      circuitCubesUpdate();
    }
  }

  if (redraw)
  {
    draw();
    redraw = false;
  }
}