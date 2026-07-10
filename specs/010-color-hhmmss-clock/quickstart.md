# Quickstart: Color HHMMSS Clock Validation

## Build

```sh
ninja -C build
```

Expected outcome: build completes successfully.

Validation result on 2026-07-10:

```text
ninja -C build
result: PASS

arm-none-eabi-size build/pov_leds.elf
text=382400 data=0 bss=65212 dec=447612
```

## Preview

Open:

```text
specs/010-color-hhmmss-clock/color_hhmmss_preview.jpg
```

Expected outcome: the JPEG shows sample time `12:34:56`, with `12` red, `34`
green, `56` blue, and white colons. No `CST` letters appear.

Validation result on 2026-07-10:

```text
specs/010-color-hhmmss-clock/color_hhmmss_preview.jpg
result: PASS, generated and visually inspected
```

## Hardware Check

1. Boot the board in STA mode and wait for time calibration.
2. Spin the PCB within the suitable range from the previous clock feature.
3. Observe the POV display.

Expected outcome: normal display shows only `HH:MM:SS`; hours are red, minutes
green, seconds blue, and seconds update once per wall-clock second.

## Validation Result

- Build validation: passed.
- JPEG preview generation: passed.
- Hardware validation: not run in this shell; requires flashed device and motor.
