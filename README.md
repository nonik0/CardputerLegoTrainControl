## Overview

This is a simple program for the [M5Stack Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps3) to control several LEGO trains or accessories with one device. All supported hubs/devices have functionality to control the motor speed with specific functionality described in the section for each device type. Supported devices are:
- [Powered Up hub](https://www.lego.com/en-us/product/hub-88009)
- [Power Functions](https://www.lego.com/en-us/product/lego-power-functions-ir-receiver-8884) (IR)
- [SBrick](https://sbrick.com/product/sbrick-plus/)
- [CircuitCubes](https://circuitcubes.com/collections/cubes/products/bluetooth-battery-cube)

![fullui](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/d713165a-d66a-4d2f-bdc0-0e0b87c3a16d) 

Thanks to the [Legoino library](https://github.com/corneliusmunz/legoino) for making this a lot easier to work with Power Functions and Powered Up, and giving me some initial boilerplate and design work for the SBrick and CircuitCubes BT libraries I wrote.  

## UI / Controls:

Two remote devices can be viewed on screen and controlled at the same time. The two active, on-screen remotes can be easily toggled between the four supported remote types using the controls and will be referenced as the left remote and the right remote. There is an indicator in the upper left that shows which types of the remote are active (left is red and right is blue). The button layout displayed on the screen roughly maps to the layout of the keys on the Cardputer's keyboard.

<img src="https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/bd67b872-6862-4e7d-930b-4c32f1ba2417" height="150">
<img src="https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/8cdf71d5-faee-4006-82c4-5f3f162098a8" height="150">

Function|Left Remote|Right Remote|Description
---|---|---|---
Left Port Control | e, s, d | i, j, n | motor control
Right Port Control | r, d, x | o, k, m | motor control
Aux1 | esc | del/backspace | BT connection toggle, channel toggle
Aux2 | tab | \ | specific to remote device
Change Remote | ctrl | space | change remote to show/control
Port Function Toggle | 3, 4 | 8, 9 | change function of port, specific to remote device
Function Shift | fn | ok/enter | hold down to change function of certain keys, specific to remote device
Display Brightness | | | control brightness of display

## Powered Up Control
![pu_0](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/b3bf5688-521c-4152-85a0-a81d316036f5)

Function|Description
---|---
Aux1 | Toggle BT connection
Aux2 | Toggle LED color

The BT connection will autodisconnect if the hub is left connected and no motors are on.

#### with Color & Distance Sensor:
![pu 1](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/96a9bf02-faa8-4c22-b871-32faa0d1c135) ![pu 2](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/6d80bd5f-246f-413b-90f1-6d7ad0c0e6c3)

Function|Description
---|---
Aux1 | Toggle BT connection
Aux2 | shows last color detected by sensor, resume after stop
Port Control Up | toggle "speed up" action: increment speed, off
Port Control Stop | toggle "stop" action: stop until interrupt (button on train or aux2 key), wait 2s/5s/9s and continue, off
Port Control Down | toggle "speed down" action: decrement speed, off
Function Shift + Port Key | change trigger color (defaults green, red, yellow)

When a [color & distance sensor](https://www.lego.com/en-us/product/color-distance-sensor-88007) is plugged in, it will be auto-detected and the port function will change. The sensor should attached to the train facing down onto the track. The sensor can trigger three different auto-actions when the corresponding color on the port button is seen by the sensor. These trigger colors can be changed, see the table. The sensor distance is also measured and the train motor will turn off if the distance measured is too great, such as when the train tips over on the track.

## Power Functions (IR) Control:
![pf 0](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/e86164c6-09c4-43ae-8f12-f5abf81bdc0b) ![pf 1](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/08ec4ebe-c890-48d8-826a-fd4885625818) ![pf 2](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/3453f76d-1aac-4895-ba0b-72ba40b0dc3d)

Function|Description
---|---
Aux1 | change IR channel (1-4)
Aux2 | change between modes
Port Function Toggle | toggle between motor and switch mode

Aux1 simply changes the IR channel being controlled and needs to match IR receiver.

Aux2 controls the remote modes, which cycle between normal -> state -> state/broadcast -> broadcast -> normal.
- State mode: tracks the speed of the motor on the Cardputer and sends specific speed commands to the remote device, as opposed to sending simple motor speed increment signals.
- Broadcast mode: broadcasts all IR commands using ESP-NOW such that other ESP devices can pick up and rebroadcast the IR signal, extending the range of the Cardputer's weak IR. See the [PowerFunctionsIRRepeater project](PowerFunctionsIrRepeater) for a simple implementation of a repeater using M5Atom. Can also listen to other broadcasts from other devices and show/update state locally.

Port Function Toggle will switch a port between motor and switch functionality. Switch functionality is for when a motor is used to control a track switch (like this [example](https://www.flickr.com/photos/skaako/albums/72157629455967364/with/7086199383)). Pressing the up/down port button will send a one second pulse to the motor to
switch the track to that direction, and the stop/center port button will toggle the switch state.

## SBrick Control
![sb 0](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/4cfd4a95-b84b-43b6-9d35-43b9bec420ea) ![sb 1](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/6d868b47-a0a2-4b16-ae91-9f238340835b) ![sb 0](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/faf889aa-e815-472d-80a9-a64d1f1c70bb)

Function|Description
---|---
Aux1 | toggle BT connection
Port Function Toggle | change port controlled on device (A, B, C, D)

SBrick has 4 ports to control. Use the port function toggle to change control between ports A, B, C, and D. Battery voltage and temperature are shown. The WeDo [motion](https://www.bricklink.com/v2/catalog/catalogitem.page?S=9583-1) and [tilt](https://www.bricklink.com/v2/catalog/catalogitem.page?S=9583-1) sensors are also (potentially) auto-detected and configured. When detected, an indicator on the remote is shown as the sensoor readings on the corresponding port buttons. If any motion or tilt is detected, like when the train is derailed, all motors will be turned off. If a sensor is not functioning properly, the sensor can be recalibrated by pressing any port key for the sensor.

## CircuitCubes Control
![cc 0](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/3fa51683-2be4-45fc-97ec-67601e9f7b19) ![cc 1](https://github.com/nonik0/CardputerLegoTrainControl/assets/17152317/ba52a255-dab0-4d76-a298-05d9549f51fb)

Function|Description
---|---
Aux1 | Toggle BT connection
Port Function Toggle | change port controlled on device (A, B, C)

CircuitCubes has 3 ports to control motors (or other devices via PWM like LEDs, etc.). Use the port function toggle to change control between ports A, B, and C. Battery voltage is also shown.

## Ideas
- track speed to be able to keep constant as battery voltage falls
  - track speed with specific calibration tile colors? measure time between two tiles and track
  - track speed with color sensor and rate of change? (i.e. motor moving but no color change after X time -> bump speed)
- support for using multiple remotes for one device type
