# ESP32 GPS Tracker

[![Compile](https://github.com/Sri-Express/ESP32-GPS-Tracker/actions/workflows/compile.yml/badge.svg)](https://github.com/Sri-Express/ESP32-GPS-Tracker/actions/workflows/compile.yml)

Arduino C++ firmware for the bus GPS unit in [Sri Express](https://github.com/Sri-Express). An ESP32 reads NMEA sentences from a NEO-8M GPS module over UART, parses the `$GNRMC` fix and posts the bus's position to the Sri Express backend every 10 seconds.

Written in September 2025 and set up for device `bus-002` on the Kottawa – Mount Lavinia route. The code is as it ran then, except that the Wi-Fi name and password now come from a gitignored header. The Heroku backend it posts to is offline now, so the repo is kept as a reference.

## Hardware

- ESP32 dev board, built for the ESP32 Dev Module (`esp32:esp32:esp32`)
- NEO-8M GPS module on UART2 at 9600 baud, 8N1

| NEO-8M | ESP32 |
| --- | --- |
| TX | GPIO2 (UART2 RX) |
| RX | GPIO4 (UART2 TX) |

## How it works

1. `readGPSData()` reads lines from UART2 and passes each `$GNRMC` or `$GPRMC` sentence to `parseGNRMC()`.
2. `parseGNRMC()` finds the comma positions and only accepts a fix when the status field is `A`. It then reads latitude, longitude, speed and course.
3. `convertDMtoDDFloat()` turns NMEA's degrees and minutes (`ddmm.mmmm`, or `dddmm.mmmm` for longitude) into decimal degrees, and `S` or `W` makes the value negative. Speed is converted from knots to km/h (× 1.852).
4. Every 10 seconds, if there's a valid fix and Wi-Fi, `sendLocationUpdate()` builds the JSON body with ArduinoJson and POSTs it with `HTTPClient`. Otherwise it logs why it skipped. `loop()` reconnects when Wi-Fi drops.

The GPS supplies `location.latitude`, `longitude`, `speed` and `heading`. `deviceId`, `vehicleId` and `operationalInfo.tripInfo.routeId` identify the bus and route. Everything else (`routeProgress`, `passengerLoad`, the driver and trip details, `environmentalData`, and the location's `accuracy` and `altitude`) is a fixed placeholder that fills out the backend's tracking record.

## Backend

The device posts to `POST /api/tracking/update` in [Sri-Express/Backend](https://github.com/Sri-Express/Backend/blob/main/src/routes/trackingRoutes.ts). The [handler](https://github.com/Sri-Express/Backend/blob/main/src/controllers/trackingController.ts) needs `deviceId`, `vehicleId`, `location` and `operationalInfo.tripInfo.routeId`. It checks that the device and route exist, saves a `LocationTracking` record and updates the device's last position. The web app's [live map](https://github.com/Sri-Express/WebApp/blob/main/src/app/components/GoogleMapsTracker.tsx) polls `/api/tracking/live` every 10 seconds.

## Setup

1. In the Arduino IDE, add `https://espressif.github.io/arduino-esp32/package_esp32_index.json` to the Boards Manager URLs. Then install **esp32** by Espressif and the **ArduinoJson** library (v7).
2. Copy `gps_tracker/arduino_secrets.example.h` to `gps_tracker/arduino_secrets.h` and fill in your Wi-Fi name and password. Git ignores `arduino_secrets.h`.
3. In `gps_tracker.ino`, point `serverURL` at your backend and set `deviceId`, `vehicleId` and `routeId` to a device and route in its database.
4. Open `gps_tracker/gps_tracker.ino`, pick **ESP32 Dev Module**, upload, and open the Serial Monitor at 115200 baud.

## CI

[compile.yml](.github/workflows/compile.yml) builds the sketch for the ESP32 Dev Module on every push to `main` and every pull request. It uses the esp32 core 3.3.12, ArduinoJson 7.4.3 and the placeholder Wi-Fi settings.
