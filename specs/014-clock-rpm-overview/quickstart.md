# Quickstart: Validate Clock and RPM Overview

## Build

```powershell
ninja -C build
```

Expected: firmware compiles and links without new warnings or errors.

## Scenario 1: both values unavailable

1. Open Overview before time calibration and before two Hall events.
2. Confirm Current Clock shows `--:--:-- CST`.
3. Confirm Rotation Speed shows `-- RPM`.

## Scenario 2: calibrated clock without rotation

1. Allow network time calibration to complete while the PCB is stationary.
2. Refresh Overview twice at least one second apart.
3. Confirm Current Clock shows valid CST and advances; RPM remains unavailable
   until rotation is measured.

## Scenario 3: calibrated clock and valid rotation

1. Rotate within the Hall sensor's supported measurement range.
2. Refresh Overview after at least two Hall events.
3. Compare `HH:MM:SS CST` with the device clock and RPM with the latest Hall
   measurement; clock error must be at most one second and RPM must match rounding.

## Scenario 4: stopped rotation

1. Establish both values, then stop rotation.
2. Wait for the existing Hall stop timeout and refresh Overview.
3. Confirm the clock continues advancing while speed shows `0 RPM`.

## Scenario 5: layout and capacity

1. Confirm all prior Overview metrics remain visible at desktop and narrow widths.
2. Verify generated page size remains within the existing fixed response buffer.

