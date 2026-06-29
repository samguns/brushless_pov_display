# Implementation Plan: Hall Sensor Rotation Speed

**Branch**: `005-hall-sensor-speed` | **Date**: 2026-06-29 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/005-hall-sensor-speed/spec.md`

## Summary

Add a Hall-effect rotation sensor on GP15 and a driver that reports the spinning
speed (RPM and Hz) and rotation period while the board sits on a spinning plate.
A reference magnet passes the sensor once per revolution, producing one digital
edge per turn. The driver captures each edge timestamp from the runtime
microsecond timebase in a minimal interrupt handler, rejects bounce/noise with a
configurable lockout interval, and derives speed/period in a non-blocking read
function callable from the existing super-loop. Rotation event capture is kept
separate from speed derivation so the derivation is unit-testable with synthetic
timestamps. The feature integrates into the current single-command build and
adds only bounded static state.

## Technical Context

**Language/Version**: C11 / C++17 (existing project standard)

**Primary Dependencies**: Pico SDK 2.2.0; `hardware_gpio`, `hardware_irq`,
`pico_time` (64-bit microsecond timer), `pico_sync` (critical sections);
existing `pico_stdlib` and build flow

**Storage**: N/A for persistent storage; runtime uses bounded static state only

**Testing**: Manual hardware validation on a spinning plate with a fixed
reference magnet; build verification via `ninja -C build`; the speed-derivation
function is a pure function that can be exercised with synthetic edge timestamps

**Target Platform**: Pimoroni Pico Plus 2 W (RP2350B) running the existing
firmware super-loop alongside the WS2812 and Wi-Fi modules

**Project Type**: Single embedded firmware target

**Performance Goals**: ±2% speed accuracy across 60–6000 RPM (1–100 Hz); reported
speed settles within 3 revolutions after a sustained change; zero/stale within a
1.5 s stop timeout; per-edge interrupt cost negligible; O(1) non-blocking read

**Constraints**: no heap allocation in the measurement or interrupt path; speed
math uses fixed time-unit conversions (s/min → µs) from the SDK microsecond
timebase, with no hardcoded system-clock (`clk_sys`) literals; input is GP15; the
GPIO interrupt path must coexist with the CYW43/RM2 driver's GPIO usage; static
RAM delta under 256 bytes

**Scale/Scope**: one Hall input channel, one rotation-speed measurement domain,
one read API consumed by the main loop (and, later, by display timing)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | PASS | This is an input-sensing feature; it does not change the LED output path. LED signaling remains DMA → TX FIFO → PIO. PIO is not required for a single low-rate digital input. |
| II. Timing Precision | PASS | Rotation period/speed derive from the SDK 64-bit microsecond timebase; only time-unit conversion constants are used, no hardcoded `clk_sys` literals. Provides the measured rotation period the display will consume. |
| III. Hardware Abstraction | PASS | GPIO/IRQ capture is isolated in the driver; speed derivation is a pure function over captured state, testable without hardware via synthetic timestamps. |
| IV. Minimal & Deterministic Memory | PASS | Bounded static state only; no heap in the measurement path or interrupt handler. RAM impact tracked in spec/validation. |
| V. Single-Command Build & Flash | PASS | New module added to `CMakeLists.txt`; `ninja -C build` and the current flashing workflow are unchanged. |

**Post-design re-check**: PASS. Research, data model, contract, and quickstart
artifacts preserve all gates; no violations introduced.

## Project Structure

### Documentation (this feature)

```text
specs/005-hall-sensor-speed/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── hall-sensor-contract.md
├── checklists/
│   └── requirements.md
└── tasks.md            # created by /speckit-tasks (not this command)
```

### Source Code (repository root)

```text
hall_sensor.h        # driver interface: config, init, update/read, getters
hall_sensor.cpp      # GPIO + IRQ edge capture, debounce, speed/period derivation
pov_leds.cpp         # integration: init sensor, periodic non-blocking read + logging
CMakeLists.txt       # add hall_sensor.cpp source and hardware_gpio/irq linkage

ws2812_driver.*      # existing LED driver (unchanged)
pov_demo.*           # existing demo logic (unchanged)
wifi_config/         # existing networking modules (unchanged)
```

**Structure Decision**: Add a dedicated `hall_sensor.*` module that owns GPIO
configuration and the edge-capture interrupt handler, keeping the handler minimal
(timestamp + count + lockout). Speed/period derivation is a separate pure
function operating on the captured sample state so it can be validated without
hardware. `pov_leds.cpp` only wires initialization and a periodic non-blocking
read into the existing super-loop.

## Complexity Tracking

No constitution violations requiring justification.
