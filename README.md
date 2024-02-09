## TODO: this README needs lots update with lots more functionality.
- Support for SBrick, including basic support for WeDo motion and tilt sensors
- Support for CircuitCubes
- New IR broadcast mode for Powered Up IR control: use ESP-NOW and secondary ESP+IR to vastly extend IR range
- UX design/controls -- switching remotes, changing ports, changing port functions, etc.


## Overview

This is a simple program for the [M5Stack Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps3) to control several LEGO trains or accessories with one device. All supported hubs/devices have functionality to control the motor speed, with specific functionality described in the section for each device type. Supported devices are:
- [Powered Up hub](https://www.lego.com/en-us/product/hub-88009) train
- [Power Functions](https://www.lego.com/en-us/product/lego-power-functions-ir-receiver-8884) train (IR)
- [SBrick](https://sbrick.com/product/sbrick-plus/)
- [CircuitCubes](https://circuitcubes.com/collections/cubes/products/bluetooth-battery-cube)

Thanks to the [Legoino library](https://github.com/corneliusmunz/legoino) for making this a lot easier to work with Power Functions and Powered Up, and giving me some initial boilerplate and design work for the SBrick and CircuitCubes BT libraries I wrote.  

## UI / Key Bindings:
Two remotes can be shown at the same time. The button layout displayed on the screen roughly maps to the layout of the keys on the Cardputer's keyboard to aid memory.

Function|Left Remote Key|Right Remote Key|Description
---|---|---|---
SPEED UP | . | . | . 
BRAKE | . | . | .
SPEED DOWN | . | . | .
AUX1 | ESC | BACKSPACE | BT connection toggle, channel toggle
AUX2 | . | . | .
FUNCTION SHIFT | . | . | .
PORT FUNCTION | . | . | .


### Powered Up Control
![btControl](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/5bf50171-f444-4a10-8230-2677134f0437)
Function|Key Mapping
---|---
BT toggle connection|esc
BT toggle LED color|tab
Port A control|e, s, z
Port B control|r, d, x

Port control actions are the speed up, stop, and speed down actions, respectively.

### Powered Up Control with Color & Distance Sensor:
![btControlWithSensor](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/c2dd324b-3905-4366-8ffa-ee84e73a4140)
Button Function|Default Color|Sensor Function
---|---|---
Speed Up|green|Increment motor speed, TBD
Stop|red|stop until interrupt (button on train or LED key), wait 2s/5s/10s and continue
Speed Down|yellow|Decrement motor speed, TBD

When a distance/color sensor is plugged into the hub, it will be auto-detected. The corresponding buttons for the sensor's port will change to the colors that trigger the sensor functions. Pressing the button for the sensor function will toggle the function (i.e. if sensor is on channel B, pressing 'd' key will toggle the sensor's stop function). The trigger colors can also be changed by holding the fn key and pressing a sensor function button (e.g. if sensor is on port B, press fn+d to change sensor stop function's trigger color)

### Power Functions (IR) Control:
![irControl](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/73e08288-a04e-4e22-aecf-69632ee6648b)
Function|Key Mapping
---|---
Red Port Control|i, j, n
Blue Port Control|o, k, n
Switch Channel|ok/enter

Port control actions are the speed up, stop, and speed down actions, respectively.

### SBrick Control

SBrick has 4 ports to control. Currently 2 ports are hardcoded to be the motion and tilt sesnor.

### CircuitCubes Control

CircuitCubes control is fairly simple and just has 3 ports to control motors (or LEDs, etc.)

## Ideas
- track speed to increment speed as battery voltage falls
-- use calibration tiles and measure time between
-- track average period between colors/actions
- toggle to replace IR with second BT control
- secondary device for making train noises (M5 Atom Echo + battery?)
- add support for controlling 2 BT trains
-- longer term potentially connect to several devices, can "page" through connected remotes, showing two at time, or more potentially? great for controlling whole setup with switches, multiple trains, etc.
