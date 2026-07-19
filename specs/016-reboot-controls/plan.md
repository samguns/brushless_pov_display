# Implementation Plan: Reboot Controls

**Branch**: `master` | **Date**: 2026-07-12 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/016-reboot-controls/spec.md`

## Summary

Add a confirmed normal-reboot action to the STA Settings System card and execute
it only after the browser acknowledgement has been flushed. The existing HTTP
module will coordinate one mutually exclusive pending reboot target, use the
Pico watchdog for normal restart, preserve USB BOOTSEL update behavior, and
reject manual reboot while OTA is receiving, validating, or ready to restart.
Remove the synthetic blink-frequency value across the application, runtime,
HTTP, HTML, and JSON layers while retaining Blink Active/Idle readiness.

## Technical Context

**Language/Version**: C11 and C++17; Pico SDK 2.2.0

**Primary Dependencies**: Existing raw-lwIP STA portal,
`pico_cyw43_arch_lwip_poll`, Pico SDK `hardware_watchdog`, existing FOTA update
state, static server-rendered HTML/CSS/JavaScript

**Storage**: Existing flash-backed Wi-Fi credentials and brightness are preserved;
no new persistent data

**Testing**: Focused host tests for web builders and pure coordination rules;
`ninja -C build`; binary size/symbol inspection; browser and hardware reboot,
persistence, duplicate-request, update-interlock, and regression scenarios

**Target Platform**: Raspberry Pi Pico W (RP2040) with the existing FOTA
bootloader and STA-mode management portal

**Project Type**: Single embedded firmware target with an inline local web UI

**Performance Goals**: Reboot begins within five seconds of request acceptance;
acknowledgement is visible before disconnect; no additional work in the
timing-critical rendering path; all generated pages fit the existing 16 KiB
response buffer

**Constraints**: Normal reboot must not enter USB BOOTSEL, install firmware, or
erase settings; one reboot action at a time; OTA receive/validate/ready states
block manual reboot; no heap or new static page buffers; preserve raw-lwIP
single-client behavior and both firmware-update routes

**Scale/Scope**: One Settings action, two new routes, three reboot-action states,
one OTA availability predicate, removal of one frequency field through four
runtime layers, and focused host/hardware validation

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. PIO-First LED Drive | PASS | No PIO, DMA, pixel, or LED transport behavior changes. Blink-frequency cleanup removes management telemetry only. |
| II. Timing Precision | PASS | Reboot handling remains in the HTTP/runtime poll path and is inactive during ordinary rendering. No ISR, column schedule, or PIO timing changes. |
| III. Hardware Abstraction | PASS | `wifi_sta_web` owns presentation, `wifi_sta_http` owns routes/deferred action, `wifi_firmware_update` owns update availability, and the watchdog API owns hardware reset. |
| IV. Minimal and Deterministic Memory Use | PASS | Existing fixed page/request buffers are reused, no heap or frame-buffer growth is added, and removal of frequency scalars offsets bounded reboot state. |
| V. Single-Command Build and Flash | PASS | `hardware_watchdog` is already linked; the existing `ninja -C build` workflow and linker topology are unchanged. |

**Post-design re-check**: PASS. The contracts require a single bounded pending
action, server-side OTA interlock, fixed response builders, and no new persistent
or dynamically allocated state. The design does not alter LED transport or
timing and introduces no constitutional exception.

## Project Structure

### Documentation (this feature)

```text
specs/016-reboot-controls/
|-- spec.md
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- reboot-controls.md
|-- checklists/
|   `-- requirements.md
`-- tasks.md                 # generated later by /speckit-tasks
```

### Source Code (repository root)

```text
pov_leds.cpp                       # publish Blink readiness without frequency
wifi_config/
|-- wifi_config.{c,h}              # remove frequency runtime state/API
|-- wifi_sta_http.{c,h}            # routes, pending target, OTA interlock, reset
|-- wifi_sta_web.{c,h}             # Settings/confirmation/ack pages; status cleanup
`-- wifi_firmware_update.{c,h}     # expose bounded update-in-progress predicate
tests/
`-- wifi_reboot_controls_test.cpp  # host page/JSON and coordination assertions
```

**Structure Decision**: Extend the existing portal modules in place. The reboot
action is another bounded management operation beside USB update and OTA, while
blink-frequency removal follows its existing publication chain. No new runtime
service, storage layer, page buffer, or build target dependency is needed.

## Complexity Tracking

No constitution violations require justification.
