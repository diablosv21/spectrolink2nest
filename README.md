# spectrolink2nest
Arduino-based solution for connecting a Nest thermostat to a Braemar Spectrolink system

This solution doesn't specifically require that you use a Nest system. This code allows you to control the Braemar Spectrolink system with anything you want, but has been designed with the Nest system specifically in mind.

I have set up the connection with the Spectrolink system to use Software Serial so that you still have the USB serial connection for debugging.

PIN Connections:

Connect any of these pins to ground to turn them ON

3 - Zone 1
4 - Zone 2
5 - Zone 3

7 - Air Conditioning
8 - Heater

Software Serial Pins

10 - RX -> Spectrolink
11 - TX -> Spectrolink

A single Nest unit is not capable of controlling zones, so I used 3 Shelly relays to turn the Zones on/off.

The Nest will also be operating at 24V AC so you will need to find a use that to throw 2 relays for Air Con/Heating. I used 2 24VAC to 12VDC convertors and then had those throw 12V Relays.
