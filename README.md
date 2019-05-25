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
- Server port is hard coded - adjust as needed.
- Had one instance of ESP8266 locking up during thunderstorm. No water on board but wondering about EMF from lightning.

Project Construction:
- I just used point-to-point wiring. PMS7003 adapter board is recommended.
- Upside down blue electrical box from Home Depot seems to shield OK from rain. Recommend leave cutouts in. Oops, too late for me.
- Fiberglass mesh was added after spider added web inside unit which corrupted readings. However, spider mites are smaller than the mesh size so will block only larger insects.

Thanks to library code providers listed at top of source code file!
