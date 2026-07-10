# Quickstart: STA POV Clock Display Validation

## Prerequisites

- Board flashed with stored Wi-Fi credentials and able to enter STA mode.
- 57-LED WS2812 radial row connected to the existing data pin.
- Hall sensor on GP15 with one reference magnet per revolution.
- Motor capable of a steady 600 RPM target.
- Development build with debug output available for validation.

## Build

```sh
ninja -C build
```

Expected outcome: build completes without errors and produces the normal firmware
artifacts.

Validation result on 2026-07-10:

```text
ninja -C build
result: PASS

arm-none-eabi-size build/pov_leds.elf
text=382456 data=0 bss=65212 dec=447668
```

## Scenario 1: Time Calibration

1. Boot the board in STA mode on a network with Internet time reachability.
2. Observe debug output for time calibration start and success.
3. Compare the reported CST value with a trusted UTC+8 reference.

Expected outcome: calibration completes within 10 seconds on a reachable network,
and the reported CST time is within +/-1 second of the reference.

## Scenario 2: Rotation Suitability

1. Spin the PCB at the nominal 600 RPM target.
2. Observe Hall-sensor speed output.
3. Slowly vary speed below 480 RPM and above 720 RPM.

Expected outcome: 600 RPM is reported as suitable, 480-720 RPM remains suitable,
and speeds outside the range transition to a non-clock status state.

## Scenario 3: POV Clock Display

1. Keep time calibrated and rotation suitable.
2. Observe the spinning display for at least 2 minutes.
3. Check readability and second cadence.

Expected outcome: the display shows `HH:MM:SS CST`, updates once per second, and
stays synchronized without persistent angular drift.

## Scenario 4: Invalid Conditions

1. Boot without reachable network time.
2. Stop the motor while the clock is running.
3. Resume at a suitable speed after time is calibrated.

Expected outcome: the display does not show a normal clock while time is
unavailable or rotation is stale; it returns to normal clock mode when both time
and rotation are valid again.

## Scenario 5: Boundary Time Rollovers

1. Validate with synthetic or controlled time values near `HH:MM:59`.
2. Validate near `23:59:59 CST`.

Expected outcome: seconds, minutes, hours, and local day wrap correctly, and the
displayed seconds change exactly once per wall-clock second.

## Hardware Validation Status

- Time calibration on real STA network: not run in this shell; requires flashed
  hardware and network access.
- 600 RPM POV readability: not run in this shell; requires motor, Hall sensor,
  and visual observation.
- Invalid-state scenarios: not run in this shell; requires hardware control over
  network reachability and rotation speed.
- Build validation: passed as recorded above.
