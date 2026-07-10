# Contract: STA POV Clock Behavior

**Feature**: 009-sta-pov-clock

## Scope

Defines observable behavior and module boundaries for the STA-mode POV clock.

## Time Calibration Contract

- Start condition: STA runtime is initialized and connected.
- Network behavior: the device attempts non-blocking network time calibration.
- Success behavior:
  - UTC time is accepted from a valid response.
  - Local display time is derived as UTC+8.
  - The display is eligible for normal clock mode once rotation is suitable.
- Failure behavior:
  - The device records a time-unavailable state.
  - The display must not show a normal-looking clock value.
  - Retry remains bounded and non-blocking.

## Rotation Contract

- Nominal target: 600 RPM.
- Initial suitable range: 480 RPM through 720 RPM inclusive.
- Source: existing Hall-sensor measurement.
- Unsuitable conditions:
  - stale or invalid Hall measurement,
  - speed below 480 RPM,
  - speed above 720 RPM,
  - excessive instability detected by the implementation.

## Display Contract

- Normal display text: `HH:MM:SS CST`.
- Time format: 24-hour China Standard Time (UTC+8, no DST).
- Update cadence: displayed seconds change once per wall-clock second.
- Rendering surface: 57 LEDs in one radial row.
- Angular layout: fixed 48-column compact clock layout.
- Output transport: existing DMA -> TX FIFO -> PIO WS2812 driver.
- Invalid states: show a bounded non-clock status pattern or blank output; never
  show a stale normal clock value.

## Observability Contract

Required development observations:

- time calibration start, success, retry, and failure,
- current CST value at second transitions,
- measured RPM and suitability state,
- display health mode changes,
- renderer column timing or delayed frame submission warnings.

## Stability Contract

- No heap allocation in timekeeping, rotation suitability, or render paths.
- No LED writes beyond 57 frame words.
- Existing STA portal and Wi-Fi reconfiguration behavior remain available.
- Normal build and flash workflow remains `ninja -C build` plus existing flash
  method.
