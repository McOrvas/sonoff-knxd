# sonoff-knxd
This is an alternative firmware for Sonoff (or other ESP8266 based) devices for integrating in a KNX installation through eibd/knxd. Unlike other available projects, this Arduino Sketch communicates to the KNX bus via a TCP socket to an eibd/knxd daemon. This solution does not use IP multicast and therefore benefits from the TCP connection reliability on the transport layer. Instead of the standardized KNX multicast or tunneling protocol it requires a running eibd/knxd daemon which is connected to the KNX bus.

It's a simple project, all needed configuration (WLAN access / KNX group addresses) has to be done inside the Sketch before the compilation. The modules cannot be configured using the ETS software.

## Arduino IDE settings
Boards Manager URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json

### Used libraries
- Time (1.6.1)
  - https://playground.arduino.cc/Code/Time/
  - https://www.pjrc.com/teensy/td_libs_Time.html
  - https://github.com/PaulStoffregen/Time
- SparkFun SCD30 Arduino Library (1.0.20) (for CO2 traffic light with Sensirion SCD30)
  - https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library
- Grove - LCD RGB Backlight (1.0.2) (optional for CO2 traffic light with Sensirion SCD30)
  - https://github.com/Seeed-Studio/Grove_LCD_RGB_Backlight

### Tested hardware
#### Sonoff S20 / Nous A1T
Option     | Value
:---       | :---
Board      | Generic ESP8266 Module
Flash Size | 1MB (FS:64KB OTA:~470KB)

#### Sonoff 4CH
Option     | Value
:---       | :---
Board      | Generic ESP8285 Module
Flash Size | 1MB (FS:64KB OTA:~470KB)

#### CO2 traffic light with Sensirion SCD30 air quality sensors
Option     | Value
:---       | :---
Board      | Adafruit Feather HUZZAH ESP8266
Flash Size | 4MB (FS:2MB OTA:~1019KB)
