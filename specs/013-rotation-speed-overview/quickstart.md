# Quickstart: Validate Rotation Speed Overview

## Prerequisites

- Raspberry Pi Pico W with the Hall sensor and magnet arrangement already used
  by the rotation-speed feature.
- Access to the device STA management portal.
- A reference RPM source for the rotating test.

## Build

```powershell
ninja -C build
```

Expected: the firmware compiles and links with no new warnings or errors.

## Scenario 1: startup unavailable state

1. Boot the device with the plate stationary and do not pass the magnet twice.
2. Open Overview.
3. Confirm `Rotation Speed` is present and shows `-- RPM`.
4. Confirm all pre-existing metric cards and navigation remain present.

## Scenario 2: valid rotation

1. Spin at a known steady rate within the supported Hall measurement range.
2. After at least two magnet passes, reload Overview.
3. Confirm `Rotation Speed` shows a whole-number RPM matching the latest device
   measurement after nearest rounding.

## Scenario 3: stopped rotation

1. Establish a valid rotating reading, then stop the plate.
2. Wait for the existing Hall stop timeout and reload Overview.
3. Confirm the metric shows `0 RPM`, not the prior moving value.

## Scenario 4: recovery and responsive layout

1. Resume rotation and reload Overview; confirm the value becomes non-zero.
2. View the page at desktop and narrow mobile widths.
3. Confirm the new card wraps consistently and does not obscure other content.

