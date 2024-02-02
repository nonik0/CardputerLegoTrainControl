#include <Arduino.h>
#include <Lpf2Hub.h>
#include <M5Cardputer.h>
#include <M5StackUpdater.h>
#include <PowerFunctions.h>

#include "common.h"
#include "draw_helper.h"
#include "CircuitCubesHub.h"
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

// lpf2 hub state
Lpf2Hub lpf2Hub;
bool lpf2Init = false;
volatile int lpf2Rssi = -1000;
short lpf2PortSpeed[2] = {0, 0};
unsigned long lpf2DisconnectDelay;    // debounce disconnects
unsigned long lpf2ButtonDebounce = 0; // debounce button presses
volatile Color lpf2LedColor = Color::ORANGE;
unsigned short lpf2LedColorDelay = 0;
unsigned long lpf2LastAction = 0; // track for auto-disconnect
volatile RemoteAction lpf2AutoAction = NoAction;

// lpf2 color/distance sensor
bool lpf2SensorInit = false;
byte lpf2SensorPort = NO_SENSOR_FOUND; // set to A or B if detected
byte lpf2MotorPort = NO_SENSOR_FOUND;  // set to opposite of sensor port if detected
short lpf2DistanceMovingAverage = 0;
volatile Color lpf2SensorColor = Color::NONE; // detected color by sensor
unsigned long lpf2SensorDebounce = 0;         // debounce sensor color changes
Color lpf2SensorIgnoreColors[] = {Color::BLACK, Color::BLUE};
Color lpf2SensorSpdUpColor = Color::GREEN;
Color lpf2SensorStopColor = Color::RED;
Color lpf2SensorSpdDnColor = Color::YELLOW;
int8_t lpf2SensorSpdUpFunction = 0; // TBD
int8_t lpf2SensorStopFunction = 0;  // <0=disabled, 0=brake, >0=wait time in seconds
int8_t lpf2SensorSpdDnFunction = 0; // TBD
unsigned long lpf2SensorStopDelay = 0;
short lpf2SensorStopSavedSpd = 0; // saved speed before stopping

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

// SBrick sensor state
bool sbrickMotionSensorInit = false;
byte sbrickMotionSensorPort = NO_SENSOR_FOUND;
bool sbrickTiltSensorInit = false;
byte sbrickTiltSensorPort = NO_SENSOR_FOUND;
WedoTilt sbrickSensorTilt;
bool sbrickSensorMotion;

// CircuitCubes state
const short circuitCubesSpdInc = 35; // ~ 255 / 10
CircuitCubesHub circuitCubesHub;
float circuitCubesBatteryV = 0;
bool circuitCubesInit = false;
short circuitCubesPortSpeed[3] = {0, 0, 0};
byte circuitCubesLeftPort = (byte)CircuitCubesHubPort::A;
byte circuitCubesRightPort = (byte)CircuitCubesHubPort::B;
volatile int circuitCubesRssi = -1000;
unsigned long circuitCubesDisconnectDelay; // debounce disconnects
unsigned long circuitCubesLastAction = 0;  // track for auto-disconnect
// volatile Action circuitCubesAutoAction = NoAction;

// IR train control state
const int IrMaxSpeed = 105;
const short IrSpdInc = 15; // hacky but to match 7 levels
PowerFunctions irTrainCtl(IR_TX_PIN);
bool irTrackState = false;
byte irChannel = 0;
short irPortSpeed[2] = {0, 0};

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

Button lpf2HubButtons[] = {
    {AuxTop, AuxCol, Row1_5, bw, bw, RemoteDevice::PoweredUp, 0xFF, BtConnection, COLOR_LIGHTGRAY, false},
    {AuxMid, AuxCol, Row2_5, bw, bw, RemoteDevice::PoweredUp, 0xFF, BtColor, COLOR_LIGHTGRAY, false},
    {NoTouchy, AuxCol, Row3_5, bw, bwhh, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::LED, NoAction, COLOR_MEDGRAY, false},
    {LeftPortSpdUp, LeftPortCol, Row1, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::A, SpdUp, COLOR_LIGHTGRAY, false},
    {LeftPortBrake, LeftPortCol, Row2, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::A, Brake, COLOR_LIGHTGRAY, false},
    {LeftPortSpdDn, LeftPortCol, Row3, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::A, SpdDn, COLOR_LIGHTGRAY, false},
    {RightPortSpdUp, RightPortCol, Row1, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::B, SpdUp, COLOR_LIGHTGRAY, false},
    {RightPortBrake, RightPortCol, Row2, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::B, Brake, COLOR_LIGHTGRAY, false},
    {RightPortSpdDn, RightPortCol, Row3, bw, bw, RemoteDevice::PoweredUp, (byte)PoweredUpHubPort::B, SpdDn, COLOR_LIGHTGRAY, false}};
uint8_t lpf2ButtonCount = sizeof(lpf2HubButtons) / sizeof(Button);

Button sbrickHubButtons[] = {
    {AuxTop, AuxCol, Row2, bw, bw, RemoteDevice::SBrick, 0xFF, BtConnection, COLOR_LIGHTGRAY, false},
    {LeftPortSpdUp, LeftPortCol, Row1, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::A, SpdUp, COLOR_LIGHTGRAY, false},
    {LeftPortBrake, LeftPortCol, Row2, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::A, Brake, COLOR_LIGHTGRAY, false},
    {LeftPortSpdDn, LeftPortCol, Row3, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::A, SpdDn, COLOR_LIGHTGRAY, false},
    {RightPortSpdUp, RightPortCol, Row1, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::B, SpdUp, COLOR_LIGHTGRAY, false},
    {RightPortBrake, RightPortCol, Row2, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::B, Brake, COLOR_LIGHTGRAY, false},
    {RightPortSpdDn, RightPortCol, Row3, bw, bw, RemoteDevice::SBrick, (byte)SBrickHubPort::B, SpdDn, COLOR_LIGHTGRAY, false},
};
uint8_t sbrickButtonCount = sizeof(sbrickHubButtons) / sizeof(Button);

Button circuitCubesButtons[] = {
    {AuxTop, AuxCol, Row2, bw, bw, RemoteDevice::CircuitCubes, 0xFF, BtConnection, COLOR_LIGHTGRAY, false},
    {LeftPortSpdUp, LeftPortCol, Row1, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::A, SpdUp, COLOR_LIGHTGRAY, false},
    {LeftPortBrake, LeftPortCol, Row2, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::A, Brake, COLOR_LIGHTGRAY, false},
    {LeftPortSpdDn, LeftPortCol, Row3, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::A, SpdDn, COLOR_LIGHTGRAY, false},
    {RightPortSpdUp, RightPortCol, Row1, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::B, SpdUp, COLOR_LIGHTGRAY, false},
    {RightPortBrake, RightPortCol, Row2, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::B, Brake, COLOR_LIGHTGRAY, false},
    {RightPortSpdDn, RightPortCol, Row3, bw, bw, RemoteDevice::CircuitCubes, (byte)CircuitCubesHubPort::B, SpdDn, COLOR_LIGHTGRAY, false},
};
uint8_t circuitCubesButtonCount = sizeof(circuitCubesButtons) / sizeof(Button);

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
uint8_t powerFunctionsIrButtonCount = sizeof(powerFunctionsIrButtons) / sizeof(Button);

// must match order of RemoteDevice enum
Button *remoteButton[] = {lpf2HubButtons, sbrickHubButtons, circuitCubesButtons, powerFunctionsIrButtons};
uint8_t remoteButtonCount[] = {lpf2ButtonCount, sbrickButtonCount, circuitCubesButtonCount, powerFunctionsIrButtonCount};
RemoteDevice activeRemoteLeft = RemoteDevice::PoweredUp;
RemoteDevice activeRemoteRight = RemoteDevice::SBrick;

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
}

void lpf2ButtonCallback(void *hub, HubPropertyReference hubProperty, uint8_t *pData)
{
  log_w("buttonCallback");

  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (hubProperty != HubPropertyReference::BUTTON || millis() < lpf2ButtonDebounce)
  {
    return;
  }

  lpf2ButtonDebounce = millis() + 200;
  ButtonState buttonState = trainCtl->parseHubButton(pData);

  if (buttonState == ButtonState::PRESSED)
  {
    if (lpf2SensorStopDelay > 0)
    {
      log_w("bt button: resume");
      lpf2ResumeTrainMotion();

      // also show press for led color button
      Button *lpf2Button = remoteButton[RemoteDevice::PoweredUp];
      for (int i = 0; i < remoteButtonCount[RemoteDevice::PoweredUp]; i++)
      {
        if (lpf2Button[i].action == BtColor)
        {
          lpf2Button[i].pressed = true;
          break;
        }
      }
    }
    else
    {
      log_w("lfp2 button: color");
      lpf2AutoAction = BtColor;
    }
  }
}

void lpf2RssiCallback(void *hub, HubPropertyReference hubProperty, uint8_t *pData)
{
  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (hubProperty != HubPropertyReference::RSSI)
  {
    return;
  }

  int rssi = trainCtl->parseRssi(pData);

  if (rssi == lpf2Rssi)
  {
    return;
  }

  log_d("rssiCallback: %d", rssi);
  lpf2Rssi = rssi;
  redraw = true;
}

void lpf2SensorCallback(void *hub, byte sensorPort, DeviceType deviceType, uint8_t *pData)
{
  Lpf2Hub *trainCtl = (Lpf2Hub *)hub;

  if (deviceType != DeviceType::COLOR_DISTANCE_SENSOR)
  {
    return;
  }

  int distance = trainCtl->parseDistance(pData);
  Color color = (Color)(trainCtl->parseColor(pData));

  // track moving average of distance to random noisy spikes, turn off motor if not on track
  lpf2DistanceMovingAverage = (lpf2DistanceMovingAverage * 3 + distance) >> 2; // I bet compiler does this anyway for dividing by 4
  if (lpf2DistanceMovingAverage > BtDistanceStopThreshold && lpf2PortSpeed[lpf2MotorPort] != 0)
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
  if (color == lpf2SensorSpdUpColor)
  {
    log_i("bt action: spdup");
    lpf2AutoAction = SpdUp;
  }
  else if (color == lpf2SensorStopColor)
  {
    // resumed by calling resumeTrainMotion() after delay
    log_i("bt action: brake");
    lpf2AutoAction = Brake;
    lpf2SensorStopDelay = millis() + lpf2SensorStopFunction * 1000;
    lpf2SensorStopSavedSpd = lpf2PortSpeed[lpf2MotorPort];
  }
  else if (color == lpf2SensorSpdDnColor)
  {
    log_i("bt action: spddn");
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
      lpf2Init = true;
      lpf2LedColor = Color::NONE;
      lpf2Hub.activateHubPropertyUpdate(HubPropertyReference::BUTTON, lpf2ButtonCallback); // TODO: not working anymore??
      lpf2Hub.activateHubPropertyUpdate(HubPropertyReference::RSSI, lpf2RssiCallback);
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
    lpf2SensorSpdUpColor = Color::GREEN;
    lpf2SensorStopColor = Color::RED;
    lpf2SensorSpdDnColor = Color::YELLOW;
    lpf2SensorStopFunction = 0;
    lpf2SensorStopDelay = 0;
  }
}

float motionV, tiltV;

void sbrickMotionSensorCallback(void *hub, byte channel, float voltage)
{
  SBrickHub *sbrickHub = (SBrickHub *)hub;

  byte port = SBrickAdcChannelToPort[channel];
  if (port != sbrickMotionSensorPort)
  {
    log_w("port %d not motion sensor", port);
    return;
  }

  motionV = voltage;
  byte motion = sbrickHub->interpretSensorMotion(voltage);

  // filter noise
  if (motion == sbrickSensorMotion)
  {
    return;
  }

  log_w("motion: %d", motion);
  sbrickSensorMotion = motion;
  redraw = true;
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

  tiltV = voltage;
  WedoTilt tilt = (WedoTilt)sbrickHub->interpretSensorTilt(voltage);

  // filter noise
  if (tilt == sbrickSensorTilt)
  {
    return;
  }

  log_w("tilt: %d", tilt);
  sbrickSensorTilt = tilt;
  redraw = true;
}

void sbrickConnectionToggle()
{
  log_w("sbrickConnectionToggle");

  if (!sbrickInit)
  {
    if (!sbrickHub.isConnecting())
    {
      sbrickHub.init();
      // sbrickHub.init("88:6b:0f:80:35:b8");
    }

    unsigned long exp = millis() + BtConnWait;
    while (!sbrickHub.isConnecting() && exp > millis())
      ;

    if (sbrickHub.isConnecting() && sbrickHub.connectHub())
    {
      sbrickInit = true;

      // detect sensors and subscribe to them
      for (byte port = 0; port < 4; port++)
      {
        byte detectedSensor = sbrickHub.detectPortSensor(port);

        // testing
        if (port == (byte)SBrickHubPort::C)
        {
          detectedSensor = (byte)WedoSensor::Motion;
        }
        if (port == (byte)SBrickHubPort::D)
        {
          detectedSensor = (byte)WedoSensor::Tilt;
        }

        if (detectedSensor == (byte)WedoSensor::Motion)
        {
          log_w("sbrick port %d detected as motion sensor %d", port, detectedSensor);
          sbrickHub.subscribeSensor(port, sbrickMotionSensorCallback);
          sbrickMotionSensorInit = true;
          sbrickMotionSensorPort = port;
          sbrickSensorMotion = false;
        }
        else if (detectedSensor == (byte)WedoSensor::Tilt)
        {
          log_w("sbrick port %d detected as tilt sensor %d", port, detectedSensor);
          sbrickHub.subscribeSensor(port, sbrickTiltSensorCallback);
          sbrickTiltSensorInit = true;
          sbrickTiltSensorPort = port;
          sbrickSensorTilt = WedoTilt::Neutral;
        }
      }

      // TODO: eventually figure out cadence for readings and set this appropriately
      if (sbrickHub.getWatchdogTimeout())
      {
        log_w("disabling sbrick watchdog");
        sbrickHub.setWatchdogTimeout(0);
      }

      sbrickDisconnectDelay = millis() + 500;
    }
    else if (sbrickDisconnectDelay < millis())
    {
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

bool sdCardInit()
{
  uint8_t retries = 3;
  SPI2.begin(M5.getPin(m5::pin_name_t::sd_spi_sclk),
             M5.getPin(m5::pin_name_t::sd_spi_miso),
             M5.getPin(m5::pin_name_t::sd_spi_mosi),
             M5.getPin(m5::pin_name_t::sd_spi_ss));
  while (!(sdInit = SD.begin(M5.getPin(m5::pin_name_t::sd_spi_ss), SPI2)) && retries-- > 0)
  {
    delay(100);
  }

  return sdInit;
}

void checkForMenuBoot()
{
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isKeyPressed('a') && sdCardInit())
  {
    updateFromFS(SD, "/menu.bin");
    ESP.restart();
  }
}

void saveScreenshot()
{
  if (!sdInit && !sdCardInit())
  {
    log_w("cannot initialize SD card");
    return;
  }

  size_t pngLen;
  uint8_t *pngBytes = (uint8_t *)M5Cardputer.Display.createPng(&pngLen, 0, 0, 240, 135);

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
    log_i("saved screenshot to %s", filename);
  }
  else
  {
    log_w("cannot save screenshot to file %s", filename);
  }

  free(pngBytes);
}

void handle_button_press(Button *button)
{
  log_d("[d:%d][p:%d][a:%d]", button->device, button->port, button->action);

  button->pressed = true;

  byte inactivePort;
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
  case BtColor:
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
      log_i("bt color: %d", lpf2LedColor);
      if (lpf2Hub.isConnected())
        lpf2Hub.setLedColor(lpf2LedColor);
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
    case RemoteDevice::PoweredUp:
      if (!lpf2Hub.isConnected())
        break;

      if (button->port == lpf2SensorPort)
      {
        if (M5Cardputer.Keyboard.isKeyPressed((activeRemoteLeft == RemoteDevice::PoweredUp) ? KEY_FN : KEY_ENTER))
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
        else if (button->action == Brake)
        {
          if (lpf2SensorStopFunction == 0)
            lpf2SensorStopFunction = 2;
          else if (lpf2SensorStopFunction == 2)
            lpf2SensorStopFunction = 5;
          else if (lpf2SensorStopFunction == 5)
            lpf2SensorStopFunction = 10;
          else if (lpf2SensorStopFunction == 10)
            lpf2SensorStopFunction = -1;
          else
            lpf2SensorStopFunction = 0;
        }
      }
      else
      {
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
      break;
    case RemoteDevice::CircuitCubes:
      if (!circuitCubesHub.isConnected())
        break;

      if (M5Cardputer.Keyboard.isKeyPressed((activeRemoteLeft == RemoteDevice::CircuitCubes) ? KEY_FN : KEY_ENTER))
      {
        // determine which port to activate
        byte inactivePort = (byte)CircuitCubesHubPort::A;
        for (byte port : {(byte)CircuitCubesHubPort::B, (byte)CircuitCubesHubPort::C})
        {
          if (port != circuitCubesLeftPort && port != circuitCubesRightPort)
          {
            inactivePort = port;
            break;
          }
        }

        byte portToInactive = button->port;

        // update corresponding port
        if (button->port == circuitCubesLeftPort)
        {
          circuitCubesLeftPort = inactivePort;
        }
        else
        {
          circuitCubesRightPort = inactivePort;
        }

        // then update all buttons to reflect active ports
        for (int i = 0; i < circuitCubesButtonCount; i++)
        {
          if (circuitCubesButtons[i].port == portToInactive)
          {
            circuitCubesButtons[i].port = inactivePort;
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

      break;
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

    if (button->action == BtColor)
    {
      return interpolateColors(COLOR_LIGHTGRAY, BtColors[lpf2LedColor], 50);
    }

    if (button->port == (byte)PoweredUpHubPort::LED)
    {
      if (lpf2SensorInit)
        return BtColors[lpf2SensorColor];
    }
  }

  if (button->action == Brake)
  {
    if (button->device == RemoteDevice::PoweredUp && lpf2PortSpeed[button->port] == 0 ||
        button->device == RemoteDevice::SBrick && sbrickPortSpeed[button->port] == 0 ||
        button->device == RemoteDevice::CircuitCubes && circuitCubesPortSpeed[button->port] == 0 ||
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

  // left port
  if (M5Cardputer.Keyboard.isKeyPressed('e'))
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

  // aux
  else if (M5Cardputer.Keyboard.isKeyPressed('`'))
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

  return pressedKey != RemoteKey::NoTouchy;
}

String getRemoteString(RemoteDevice remote)
{
  switch (remote)
  {
  case RemoteDevice::PoweredUp:
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
  case RemoteDevice::PoweredUp:
    return "A";
  case RemoteDevice::SBrick:
    return "A";
  case RemoteDevice::PowerFunctionsIR:
    return "RED";
  case RemoteDevice::CircuitCubes:
    return String(CircuitCubesPortToChar[circuitCubesLeftPort]);
  default:
    return "?";
  }
}

String getRemoteRightPortString(RemoteDevice remote)
{
  switch (remote)
  {
  case RemoteDevice::PoweredUp:
    return "B";
  case RemoteDevice::SBrick:
    return "B";
  case RemoteDevice::PowerFunctionsIR:
    return "BLUE";
  case RemoteDevice::CircuitCubes:
    return String(CircuitCubesPortToChar[circuitCubesRightPort]);
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

  draw_active_remote_indicator(&canvas, 2 * om, hy + (hh / 2), activeRemoteLeft, activeRemoteRight);
  draw_battery_indicator(&canvas, w - 34, hy + (hh / 2), batteryPct);

  // draw labels
  canvas.setTextColor(TFT_SILVER, COLOR_MEDGRAY);
  canvas.setTextDatum(bottom_center);
  canvas.setTextSize(1);
  canvas.drawString(getRemoteLeftPortString(activeRemoteLeft), c2 + bw / 2, r1 - 2);
  canvas.drawString(getRemoteRightPortString(activeRemoteLeft), c3 + bw / 2, r1 - 2);
  canvas.drawString(getRemoteLeftPortString(activeRemoteRight), c4 + bw / 2, r1 - 2);
  canvas.drawString(getRemoteRightPortString(activeRemoteRight), c5 + bw / 2, r1 - 2);

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
      canvas.drawString(String(sbrickBatteryV, 2), sbrickDataCol + bw / 2, r1_5 + 9);
    }
  }
  if (sbrickMotionSensorInit && sbrickDataCol)
  {
    canvas.drawString(String(motionV, 2), sbrickDataCol + bw / 2, r3_5 - 2);
  }

  if (sbrickTiltSensorInit && sbrickDataCol)
  {
    canvas.drawString(String(tiltV, 2), sbrickDataCol + bw / 2, r3_5 + 9);
  }

  int circuitCubesDataCol = 0;
  if (circuitCubesInit)
  {
    if (activeRemoteLeft == RemoteDevice::CircuitCubes)
    {
      circuitCubesDataCol = c1;
    }
    else if (activeRemoteRight == RemoteDevice::CircuitCubes)
    {
      circuitCubesDataCol = c6;
    }

    if (circuitCubesDataCol)
    {
      canvas.drawString(String(circuitCubesBatteryV, 2), circuitCubesDataCol + bw / 2, r1_5 + 9);
    }
  }

  // draw layout for both active remotes
  State state = {lpf2Init, lpf2Rssi,
                 lpf2LedColor, lpf2SensorPort,
                 lpf2SensorSpdUpColor, lpf2SensorStopColor, lpf2SensorSpdDnColor,
                 lpf2SensorSpdUpFunction, lpf2SensorStopFunction, lpf2SensorSpdDnFunction,
                 sbrickInit, sbrickRssi,
                 circuitCubesInit, circuitCubesRssi,
                 irChannel};

  // TODO: draw battery indicators if available

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

    // take screenshot
    if (M5Cardputer.BtnA.isPressed())
    {
      saveScreenshot();
    }
  }

  // sensor or button triggered action
  if (lpf2AutoAction != NoAction)
  {
    log_w("auto action: %d", lpf2AutoAction);

    Button *lpf2Button = remoteButton[RemoteDevice::PoweredUp];
    for (int i = 0; i < remoteButtonCount[RemoteDevice::PoweredUp]; i++)
    {
      if (lpf2Button[i].action == lpf2AutoAction && (lpf2Button[i].port == 0xFF || lpf2Button[i].port == lpf2MotorPort))
      {
        handle_button_press(&lpf2Button[i]);
        lpf2AutoAction = NoAction;
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
      if (!lpf2Hub.isConnected())
      {
        lpf2Init = false;
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

        if (!lpf2SensorInit) // TODO: timeout for checking for sensor?
        {
          lpf2SensorPort = lpf2Hub.getPortForDeviceType((byte)DeviceType::COLOR_DISTANCE_SENSOR);
          if (lpf2SensorPort == (byte)PoweredUpHubPort::A || lpf2SensorPort == (byte)PoweredUpHubPort::B)
          {
            lpf2Hub.activatePortDevice(lpf2SensorPort, lpf2SensorCallback);
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
          return; // TODO: make sure OK
        }

        if (millis() - lpf2LastAction > BtInactiveTimeoutMs && lpf2PortSpeed[lpf2MotorPort] == 0) // TODO: check both, only works with sensor?
        {
          lpf2ConnectionToggle();
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
      }
    }

    if (circuitCubesInit)
    {
      if (!circuitCubesHub.isConnected())
      {
        circuitCubesInit = false;
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
          log_w("circuitCubes battery: %.2f", newBatteryV);
          circuitCubesBatteryV = newBatteryV;
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