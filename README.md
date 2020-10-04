# sonoff-knxd
This is an alternative firmware for Sonoff (or other ESP8266 based) devices for integrating in a KNX installation through eibd/knxd. Unlike other available projects, this Arduino Sketch communicates to the KNX bus via a TCP socket to an eibd/knxd daemon. This solution does not use IP multicast and therefore benefits from the TCP connection reliability on the transport layer. Instead of the standardized KNX multicast or tunneling protocol it requires a running eibd/knxd daemon which is connected to the KNX bus.

It's a simple project, all needed configuration (WLAN access / KNX group addresses) has to be done inside the Sketch before the compilation. The modules cannot be configured using the ETS software.

## Arduino IDE settings
Boards Manager URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json

### Sonoff S20
Option     | Value
:---       | :---
Board      | Generic ESP8266 Module
Flash Mode | DOUT (compatible)
Flash Size | 1MB (FS:64KB OTA:~470KB)

### Sonoff 4CH
Option     | Value
:---       | :---
Board      | Generic ESP8285 Module
Flash Mode | DOUT (compatible)
Flash Size | 1MB (FS:64KB OTA:~470KB)
