
## Overview

This is a simple remote for the [M5Stack Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps3) to control two LEGO trains with one device. One LEGO train should be a newer [Powered Up hub](https://www.lego.com/en-us/product/hub-88009) train (BT) and the other LEGO train should be an older [Power Functions](https://www.lego.com/en-us/product/lego-power-functions-ir-receiver-8884) train (IR). It makes sense for the trains I own. :) The remote is able to control the speed of both channels/ports of the BT and IR hubs/controllers, change the color of the BT hub, and change IR channels. I have lights on the second channel of each train hub so I control the motor with one channel and the light brightness with the other. I also have functionality that uses the color/distance sensor.

Thanks to the [Legoino library](https://github.com/corneliusmunz/legoino) for making this a lot easier.  

## Key Bindings:
They should mostly make sense and roughly map to layout on screen for help to remember.

**Bluetooth Control:**
BT connect/disconnect: esc
BT LED toggle/stop resume: tab
Port A: e,s,z
Port B: r,d,x

With detected color/distance sensor:
Speed Up/Speed Down functions: Increment/decrement motor speed, TBD
Stop Function: stop until interrupt (button on train or LED key), wait 2s/5s/10s and continue
Sensor color change: fn (hold) + sensor function button (e.g. if sensor is on port B, press fn+d to change stop function color)

Color/distance sensor is automatically detected and button functions for corresponding port switch automatically.

**IR Control:**
Red Port: i,j,n
Blue Port: o,k,n
Switch Channel: ok/enter