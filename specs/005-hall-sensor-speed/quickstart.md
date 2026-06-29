# Quickstart: Hall Sensor Rotation Speed

**Feature**: 005-hall-sensor-speed

This guide validates that the Hall driver reports correct spinning speed on
hardware. See [contracts/hall-sensor-contract.md](contracts/hall-sensor-contract.md)
and [data-model.md](data-model.md) for interface and field details.

## Prerequisites

- Pimoroni Pico Plus 2 W (RP2350B) with the project firmware.
- HAL250SO Hall sensor output wired to **GP15**, with power/ground per the part's
  datasheet; the push-pull output drives GP15 directly, so no pull resistor is
  required.
- A spinning plate with a **single reference magnet** fixed so it passes the
  sensor once per revolution, oriented to the polarity the sensor detects.
- USB serial connected for log output.
- A reference speed source (e.g., a tachometer, or a plate driven at a known RPM).

## Build & Flash

```bash
ninja -C build
picotool load build/pov_leds.uf2 -fx
```

## Scenario 1 — Bench edge check (no plate)

1. With the board stationary, watch the serial log; speed should read 0 and the
   reading should be marked stale.
2. Sweep the magnet past the sensor by hand a few times.
3. **Expected**: each pass produces exactly one counted event; a transient
   speed/period is reported, then it returns to 0/stale after ~1.5 s.

## Scenario 2 — Steady speed accuracy

1. Run the plate at a known steady rate within 60–6000 RPM.
2. Compare the logged RPM/Hz against the reference.
3. **Expected**: reported speed is within ±2% of the reference (SC-001).

## Scenario 3 — Responsiveness to change

1. While spinning, change the plate speed up and then down.
2. **Expected**: the reported speed follows each sustained change and settles to
   the new value within 3 revolutions (SC-002).

## Scenario 4 — Stop detection

1. From a spinning state, stop the plate.
2. **Expected**: reported speed returns to 0 and is marked stale within 1.5 s
   (SC-003, SC-007).

## Scenario 5 — Count integrity & stability

1. Run continuously for at least 5 minutes at a steady speed.
2. **Expected**: exactly one event per revolution across the run (compare counted
   events to revolutions), with no hangs or invalid readings (SC-004, SC-006).

## Non-blocking check

- While the above run, confirm other super-loop work (Wi-Fi polling, WS2812
  output) continues uninterrupted, demonstrating the read path does not block
  (SC-005).

## Notes

- If the sensor variant is push-pull or active-high, set `active_low`/`pull_up`
  accordingly in the driver configuration.
- If more than one magnet is installed per revolution, set `magnets_per_rev` to
  match so RPM/Hz remain correct.
