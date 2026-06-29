# Validation Log: Hall Sensor Rotation Speed

## Build Validation

- Completed: `ninja -C build` succeeded; `hall_sensor.cpp` compiles and links into
  `pov_leds.elf` for the Pimoroni Pico Plus 2 W (RP2350B).

## Static RAM Budget (Measured)

- Measured with `arm-none-eabi-gcc` (Cortex-M33, RP2350):
  `hall_sensor_config_t` = 16 B, `hall_capture_t` = 32 B,
  `hall_rotation_measurement_t` = 24 B (transient), `hall_sensor_t` = 56 B.
- Persistent feature footprint ≈ 56 B (the `hall_sensor_t` instance) plus ~8 B of
  main-loop scratch ≈ 64 B total, under the 256 B ceiling.

## Scenario 1: Bench Edge Check (no plate)

- Pending hardware verification (stationary reads zero/stale; hand sweeps count one
  event per pass).

## Scenario 2: Steady-Speed Accuracy

- Pending hardware verification (reported speed within ±2% of reference, SC-001).

## Scenario 3: Responsiveness to Change

- Pending hardware verification (settles within 3 revolutions, SC-002).

## Scenario 4: Stop Detection

- Pending hardware verification (zero/stale within 1.5 s, SC-003/SC-007).

## Scenario 5: Count Integrity & Stability

- Pending hardware verification (one event per revolution over 500+ passes,
  5-minute soak, SC-004/SC-006).

## Non-Blocking Check

- Software evidence: `hall_sensor_read` snapshots under a brief critical section
  and performs O(1) math; no `sleep`/busy-wait in the read path (SC-005).
- Pending runtime confirmation that Wi-Fi polling and WS2812 output continue
  uninterrupted while measuring.

## Timing Source Evidence

- Software evidence: period/speed derive from the SDK 64-bit microsecond timebase
  (`time_us_64`); only second/minute → microsecond conversions are used, with no
  hardcoded `clk_sys` literals (FR-009).
