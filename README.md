
## Overview

This is a simple remote for the [M5Stack Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps3) to control two LEGO trains with one device. One LEGO train should be a newer [Powered Up hub](https://www.lego.com/en-us/product/hub-88009) train (BT) and the other LEGO train should be an older [Power Functions](https://www.lego.com/en-us/product/lego-power-functions-ir-receiver-8884) train (IR). It makes sense for the trains I own. :) The remote is able to control the speed of both ports of the BT hub and IR controller, change the LED color of the BT hub, and change IR channels. I have lights on the second channel of each train hub so I control the motor with one channel and the light brightness with the other. I also have functionality that uses the [color/distance sensor](https://www.lego.com/en-us/product/color-distance-sensor-88007) to automatically control the bluetooth train based on what color is seen by the sensor.

Thanks to the [Legoino library](https://github.com/corneliusmunz/legoino) for making this a lot easier.  

## Key Bindings:
The button layout displayed on the screen roughly maps to the layout of the keys on the Cardputer's keyboard to aid memory.

### Bluetooth Control:
Function|Key Mapping
---|---
BT toggle connection|esc
BT toggle LED color|tab
Port A control|e, s, z
Port B control|r, d, x

Port control actions are the speed up, stop, and speed down actions, respectively.

### Bluetooth Control with Sensor:
Sensor Function|Default Color|Key Mapping
---|---|---
Speed Up|green|Increment motor speed, TBD
Stop|red|stop until interrupt (button on train or LED key), wait 2s/5s/10s and continue
Speed Down|yellow|Decrement motor speed, TBD

When a distance/color sensor is plugged into the hub, it will be auto-detected. The corresponding buttons for the sensor's port will change to the colors that trigger the sensor functions. Pressing the button for the sensor function will toggle the function (i.e. if sensor is on channel B, pressing 'd' key will toggle the sensor's stop function). The trigger colors can also be changed by holding the fn key and pressing a sensor function button (e.g. if sensor is on port B, press fn+d to change sensor stop function's trigger color)

###IR Control:
Function|Key Mapping
---|---
Red Port Control|i, j, n
Blue Port Control|o, k, n
Switch Channel|ok/enter

Port control actions are the speed up, stop, and speed down actions, respectively.
