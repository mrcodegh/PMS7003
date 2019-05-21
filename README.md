# PMS7003

This repository is Arduinio code (lib 2.5.1 based) to read PMS7003 data and relay it to a server, configurable over-the-air via WiFi Manager.

Features:
- Uses default serial port pins for debug/code load and swap pins for pms7003(no ESP8266 serial boot garbage)
- Periodic Reporting (10 minutes) of PM 1.0, PM2.5, and PM 10.0 data.
- Deep Sleep mode for 8266 and sleep mode for pms7003.
- Initial configuration of website/server destination via PMS_7003 WiFi Access Point.
- Allows over the air updates. Configure post response to send "update" to initiate.

Known Issues:
- WiFiManager.cpp needs patch to compile - see issues in that library.
- Access Point password protect didn't seem to work.

Thanks to library code providers listed at top of source code file!
