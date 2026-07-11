# Implementation Plan: Adaptive Hall-Synchronized Rendering

**Branch**: `master` | **Date**: 2026-07-12 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/015-adaptive-hall-rendering/spec.md`

## Summary

Make the Hall sensor the source of both revolution period and angular phase.
Normal rendering is supported from 480 through 800 RPM using a compact 40-column
clock. Each render decision derives the current angular column from the latest
Hall edge timestamp, measured period, and current monotonic time, so missed loop
iterations skip expired columns without accumulating drift.

## Technical Context

**Language/Version**: C11 and C++17; Pico SDK 2.2.0

**Primary Dependencies**: Existing Hall GPIO/IRQ capture, `pov_clock` rotation
eligibility, compact renderer, WS2812 DMA-to-PIO driver, Pico monotonic timebase

**Storage**: Fixed-size Hall measurement/generation, rotation state, renderer
schedule, transfer-ready timestamp, and single radial frame buffer; no
persistent storage or heap

**Testing**: Host derivation/renderer tests with synthetic timestamps; boundary
and delayed-iteration tests; `ninja -C build`; hardware tests at 480, 600, ~764,
and 800 RPM plus speed transitions and a 15-minute soak

**Target Platform**: Raspberry Pi Pico W (RP2040)

**Project Type**: Embedded real-time POV firmware

**Performance Goals**: No persistent angular drift across 100 revolutions;
supported-speed changes settle within two revolutions; expired columns are
skipped; WS2812 transfers never overlap

**Constraints**: 57 serial WS2812 LEDs at 800 kbit/s; 40 angular columns; 480-800
RPM inclusive; one Hall reference event per revolution; no blocking or heap in
measurement/render paths; preserve existing DMA-to-PIO transport

**Scale/Scope**: One Hall channel, one rotation/phase domain, one compact clock,
one active radial column frame, fixed clockwise installation

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. PIO-First LED Drive | PASS | WS2812 bits remain DMA-fed to PIO; CPU selects bounded column data only. |
| II. Timing Precision | PASS | Period and phase derive from Hall timestamps on the SDK microsecond timebase; absolute phase prevents cumulative drift. Hardware validation remains required for the sub-microsecond jitter budget. |
| III. Hardware Abstraction | PASS | Hall capture exposes a measurement; rotation eligibility and renderer phase mapping remain pure/testable layers. |
| IV. Minimal and Deterministic Memory Use | PASS | Only fixed scalar timestamps/state are added; angular arrays shrink to 40 columns and no heap is used. |
| V. Single-Command Build and Flash | PASS | Existing CMake configuration and `ninja -C build` remain unchanged. |

**Post-design re-check**: PASS. Phase propagation adds bounded scalar fields and
does not alter PIO code, DMA configuration, GPIO ownership, or build topology.
The driver also accounts for PIO drain and WS2812 latch time before permitting a
new transfer. Hardware timing measurements remain a validation obligation rather
than claiming unmeasured jitter performance.

## Project Structure

### Documentation (this feature)

```text
specs/015-adaptive-hall-rendering/
|-- spec.md
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- adaptive-rendering-contract.md
|-- checklists/
|   `-- requirements.md
|-- validation.md
`-- tasks.md
```

### Source Code (repository root)

```text
hall_sensor.{h,cpp}       # expose accepted Hall edge timestamp in measurement
pov_clock.{h,cpp}         # retain phase reference with rotation eligibility
pov_clock_renderer.{h,cpp} # map absolute Hall phase to angular column
pov_leds.cpp              # pass phase-aware state through existing render loop
ws2812_driver.{h,cpp}     # enforce wire-time plus latch completion
```

**Structure Decision**: Extend the existing measurement and pure-rendering
boundaries rather than coupling the renderer to GPIO/IRQ state. The Hall driver
owns capture, `pov_clock` owns eligibility, and the renderer owns phase-to-column
mapping; main only wires the values together.
