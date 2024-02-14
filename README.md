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
![pu_0](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/b3bf5688-521c-4152-85a0-a81d316036f5) ![pu 1](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/96a9bf02-faa8-4c22-b871-32faa0d1c135)


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
![pf 0](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/e86164c6-09c4-43ae-8f12-f5abf81bdc0b) ![pf 1](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/08ec4ebe-c890-48d8-826a-fd4885625818) ![pf 2](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/3453f76d-1aac-4895-ba0b-72ba40b0dc3d)



Function|Key Mapping
---|---
Red Port Control|i, j, n
Blue Port Control|o, k, n
Switch Channel|ok/enter

Port control actions are the speed up, stop, and speed down actions, respectively.

### SBrick Control
![sb 0](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/4cfd4a95-b84b-43b6-9d35-43b9bec420ea) ![sb 1](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/6d868b47-a0a2-4b16-ae91-9f238340835b) ![sb 0](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/faf889aa-e815-472d-80a9-a64d1f1c70bb)




SBrick has 4 ports to control. Currently 2 ports are hardcoded to be the motion and tilt sesnor.

### CircuitCubes Control
![cc 0](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/3fa51683-2be4-45fc-97ec-67601e9f7b19) ![cc 1](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/ba52a255-dab0-4d76-a298-05d9549f51fb)



CircuitCubes control is fairly simple and just has 3 ports to control motors (or LEDs, etc.)

## Ideas
- track speed to increment speed as battery voltage falls
-- use calibration tiles and measure time between
-- track average period between colors/actions
- toggle to replace IR with second BT control
- secondary device for making train noises (M5 Atom Echo + battery?)
- add support for controlling 2 BT trains
-- longer term potentially connect to several devices, can "page" through connected remotes, showing two at time, or more potentially? great for controlling whole setup with switches, multiple trains, etc.
