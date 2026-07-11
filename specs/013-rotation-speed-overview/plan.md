# Implementation Plan: Rotation Speed Overview

**Branch**: `master` | **Date**: 2026-07-11 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/013-rotation-speed-overview/spec.md`

## Summary

Publish the existing Hall-sensor rotation measurement through the established
main-loop -> Wi-Fi runtime -> STA HTTP status path and render it as a new
`Rotation Speed` Overview metric. The main loop retains whether a valid sample
has ever existed so startup/sensor failure displays unavailable, while a stale
measurement after prior rotation displays `0 RPM`.

## Technical Context

**Language/Version**: C11 and C++17; Pico SDK 2.2.0

**Primary Dependencies**: Existing `hall_sensor`, `wifi_config`, raw-lwIP STA
HTTP server, and inline management UI page builders

**Storage**: Fixed-size runtime scalars only; no persistent storage

**Testing**: `ninja -C build`; focused page-builder/status propagation checks;
hardware browser validation against Hall-sensor rotation

**Target Platform**: Raspberry Pi Pico W (RP2040)

**Project Type**: Embedded firmware with an inline management web UI

**Performance Goals**: Overview renders the most recent main-loop status at
request time without blocking sensor capture or display output

**Constraints**: No heap allocation; no Hall IRQ or speed-derivation changes;
no browser polling; preserve the 16 KiB fixed page buffer and existing UI

**Scale/Scope**: One Hall sensor, one latest RPM status, one Overview metric,
and the existing single-client HTTP server

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. PIO-First LED Drive | PASS | No PIO, DMA, frame, or LED-output code is changed. |
| II. Timing Precision | PASS | Existing Hall-derived RPM is consumed without changing capture or timing derivation. |
| III. Hardware Abstraction | PASS | Main publishes a scalar runtime status; Wi-Fi and web layers do not access Hall GPIO state. |
| IV. Minimal and Deterministic Memory Use | PASS | Fixed scalar fields only; no heap or interrupt-path allocation. |
| V. Single-Command Build and Flash | PASS | Existing `ninja -C build` flow remains unchanged. |

**Post-design re-check**: PASS. The contract carries only a boolean availability
flag and whole-number RPM through existing fixed-size module state. No
constitution exception or complexity waiver is required.

## Project Structure

### Documentation (this feature)

```text
specs/013-rotation-speed-overview/
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- rotation-speed-status.md
|-- checklists/
|   `-- requirements.md
`-- tasks.md
```

### Source Code (repository root)

```text
pov_leds.cpp                    # publish the latest Hall measurement
wifi_config/wifi_config.{c,h}   # retain application runtime status
wifi_config/wifi_sta_http.{c,h} # retain HTTP-facing status and route it
wifi_config/wifi_sta_web.{c,h}  # render Rotation Speed on Overview
```

**Structure Decision**: Extend the existing runtime status pipeline alongside
blink status. Keep measurement ownership in `pov_leds.cpp`, transport ownership
in `wifi_config`/`wifi_sta_http`, and presentation ownership in `wifi_sta_web`.

