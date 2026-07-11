# Implementation Plan: Clock and RPM Overview

**Branch**: `master` | **Date**: 2026-07-11 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/014-clock-rpm-overview/spec.md`

## Summary

Extend the existing main-loop -> Wi-Fi runtime -> STA HTTP -> Overview status
pipeline with the current calibrated `HH:MM:SS` clock and an availability flag.
Overview will render that request-time CST clock beside the already implemented
Hall RPM metric, using explicit placeholders when either measurement is unknown.

## Technical Context

**Language/Version**: C11 and C++17; Pico SDK 2.2.0

**Primary Dependencies**: Existing `pov_clock`, `time_sync`, `hall_sensor`,
`wifi_config`, raw-lwIP STA HTTP server, and inline web page builders

**Storage**: Fixed-size runtime flags, whole RPM, and bounded 9-byte clock text;
no persistent storage

**Testing**: `ninja -C build`; host page-builder checks for calibrated and
unavailable combinations; hardware browser comparison to runtime clock/RPM

**Target Platform**: Raspberry Pi Pico W (RP2040)

**Project Type**: Embedded firmware with inline management web UI

**Performance Goals**: Refreshed values reflect the latest completed main-loop
snapshot, with calibrated clock lag bounded to one second

**Constraints**: No heap allocation, browser polling, extra page buffer, or
changes to time sync, Hall capture, rotation derivation, PIO, DMA, or LED timing

**Scale/Scope**: One bounded clock snapshot, the existing RPM snapshot, one new
Overview card, and the existing single-client server

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. PIO-First LED Drive | PASS | No LED, DMA, or PIO output path changes. |
| II. Timing Precision | PASS | Existing monotonic clock advancement and Hall measurement are consumed unchanged. |
| III. Hardware Abstraction | PASS | Main publishes bounded status; web modules do not access time-sync or Hall internals. |
| IV. Minimal and Deterministic Memory Use | PASS | Fixed flags/scalars and 9-byte clock arrays only; no heap. |
| V. Single-Command Build and Flash | PASS | Existing build configuration and command remain unchanged. |

**Post-design re-check**: PASS. The design adds bounded copied status only and
introduces no constitution exception.

## Project Structure

### Documentation (this feature)

```text
specs/014-clock-rpm-overview/
|-- spec.md
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- overview-runtime-status.md
|-- checklists/
|   `-- requirements.md
|-- validation.md
`-- tasks.md
```

### Source Code (repository root)

```text
pov_leds.cpp                    # publish calibrated clock snapshot
wifi_config/wifi_config.{c,h}   # retain application clock/RPM status
wifi_config/wifi_sta_http.{c,h} # retain HTTP-facing runtime snapshot
wifi_config/wifi_sta_web.{c,h}  # render Current Clock and Rotation Speed
```

**Structure Decision**: Extend the status pipeline already used by connectivity,
blink, and RPM. Measurement/calibration ownership remains in application modules;
the web builder receives only display-ready bounded values.

