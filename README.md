# Chicken Coop

Controls the doors and lights of a backyard chicken coop based on sunrise and sunset calculations. Firmware runs on an ESP32 with an external RTC and TFT status display and has been in production since 2020.

## Overview
- Automates two linear actuators (coop door, run door) plus light and auxiliary relays.
- Uses a pre-computed sunrise/sunset table with offsets to drive a daily schedule.
- Syncs time over Wi-Fi/NTP and falls back to a DS3231 RTC when offline.
- Shows current state, mode, output status, and battery voltage on a 160x128 TFT display.
- Pushes state transitions to a simple HTTP logging endpoint for historical tracking.

## Hardware
- ESP32 development board with deep-sleep support.
- DS3231 real-time clock module connected via I²C.
- ST7735-based TFT display driven by the `TFT_eSPI` library (pins configured in `User_Setup.h`).
- Two bidirectional relay pairs for the coop and run actuator motors:
  - `COOP_DOOR_1_POS` GPIO27 / `COOP_DOOR_1_NEG` GPIO26
  - `COOP_DOOR_2_POS` GPIO25 / `COOP_DOOR_2_NEG` GPIO33
  - `RUN_DOOR_1_POS` GPIO32 / `RUN_DOOR_1_NEG` GPIO13
  - `RUN_DOOR_2_POS` GPIO15 / `RUN_DOOR_2_NEG` GPIO2
- Lighting relay on GPIO17 and auxiliary relay on GPIO12.
- Mode buttons on GPIO0 (`BUTTON_1`) and GPIO35 (`BUTTON_2`).
- Battery voltage sense on GPIO38 (`VOLTAGE`) scaled through `VOLTAGE_FACTOR`.

![Electronics enclosure](IMG_8044.jpg)

## Firmware Highlights
- `sunTable`: coarse sunrise/sunset lookup (36 entries) tuned for San Francisco; adjust for other locations.
- `cron`: eight daily states with door/light targets and sunrise/sunset offsets:
  | State   | Trigger                | Coop door | Run door | Light | Notes |
  |---------|------------------------|-----------|----------|-------|-------|
  | Wakeup  | Sunrise − 28 minutes   | Closed    | Closed   | On    | Pre-dawn lighting |
  | RunAM   | Sunrise − 25 minutes   | Open      | Closed   | On    | Allows chickens into run |
  | Day     | Sunrise                | Open      | Open     | Off   | Optional deep-sleep entry |
  | Dawn    | Sunset − 10 minutes    | Open      | Open     | On    | Evening lighting |
  | RunPM   | Sunset + 20 minutes    | Open      | Closed   | On    | Begins locking run |
  | Roost   | Sunset + 35 minutes    | Open      | Closed   | Off   | Lights out |
  | Night   | Sunset + 40 minutes    | Closed    | Closed   | Off   | Optional deep-sleep entry |
- States flagged with `sleep=true` (Day, Night) trigger deep sleep roughly two minutes after the transition; the ESP32 wakes via timer for the next scheduled state.
- Voltage readings below ~10.0 V are treated as zero to filter noise before logging.
- `sendLog()` hits an AWS API Gateway endpoint with timestamp, state name, and pack voltage.

## Build & Upload
- Install the Arduino IDE or PlatformIO with ESP32 board support.
- Required libraries: `TFT_eSPI`, `RTClib` (others come with the ESP32 core).
- Configure `TFT_eSPI`'s `User_Setup.h` to match your display wiring.
- Update the Wi-Fi credentials in `ssid`/`password` and adjust `gmtOffset_sec`/`daylightOffset_sec` for your time zone.
- Optionally refine `sunTable`, `cron`, and `VOLTAGE_FACTOR` to match your location, routine, and voltage divider.
- Flash `ChickenCoop.ino` to the ESP32 and monitor the serial console at 115200 baud for startup diagnostics.

## Operation
- On boot the controller increments `bootCount`, connects to Wi-Fi for NTP, and synchronizes the RTC if drift is detected.
- The TFT displays current time, mode (`Auto`, `Manual`, `Test`), active state, minutes until the next state, supply voltage, and a row of colored blocks for each output (green = energized relay).
- Modes are switched with `BUTTON_2` (active-low): Auto → Test → Manual → Auto. In Manual mode, pressing `BUTTON_1` steps forward through the cron states immediately.
- Test mode continuously walks each relay, pausing long enough to verify wiring and prints the calculated sunrise/sunset schedule to the serial console.
- When Auto mode reaches a sleeping state, the ESP32 goes into timed deep sleep after approximately two minutes to conserve power. Either the scheduled timer or `BUTTON_1` on GPIO0 wakes the device.

## Logging & Monitoring
- Successful state changes trigger an HTTP GET request to `https://12bx3x78jb.execute-api.us-west-2.amazonaws.com/default/cronLogger` with the timestamp, state name, and measured voltage.
- Adjust `sendLog()` if you want to point at a different endpoint or use HTTPS client certificates.
- Serial output mirrors status changes, Wi-Fi connection attempts, and any HTTP errors for easy debugging.

## Customization Tips
- Update `sunTable` or add interpolation if you relocate outside the Bay Area.
- Extend the `cron` array to add morning/evening states or additional behaviors.
- Use the `AUX` relay pin for future accessories (heater, fan) and mirror the control pattern in new cron entries.
- Calibrate `VOLTAGE_FACTOR` based on your voltage divider to improve battery readings.

In production since 2020 and still waking the flock on schedule.
