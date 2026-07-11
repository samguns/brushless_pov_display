# Implementation Plan: WiFi Firmware Update

**Branch**: `011-wifi-firmware-update` | **Date**: 2026-07-11 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/011-wifi-firmware-update/spec.md`

## Summary

Replace the management-page action that reboots into USB BOOTSEL mode with an authenticated
browser-uploaded FOTA flow. A pinned MIT `pico_fota_bootloader` dependency supplies dual
application slots, SHA-256 validation, and rollback; a small `.povota` envelope adds board
compatibility and size checks before the inner FOTA image reaches the inactive slot.

The installed single-slot firmware cannot safely become its own dual-slot bootloader while
executing from flash. Deployment therefore needs a one-time USB flash of the bootloader and
first OTA-capable application. Thereafter ordinary updates are WiFi-only, with ROM BOOTSEL
USB retained as recovery.

## Technical Context

**Language/Version**: C11 / C++17 (existing project standard)

**Primary Dependencies**: Pico SDK 2.2.0; raw lwIP/CYW43 STA portal; pinned MIT
`pico_fota_bootloader` with SHA-256 and rollback; Python 3 + `hashlib` packaging helper.

**Storage**: 2 MB QSPI flash partitioned into bootloader/metadata, active application,
inactive download, and an 8 KB reserved tail whose final 4 KB remains the persistent-settings block. No heap or full-image RAM
buffer.

**Testing**: `ninja -C build`; host package/parser tests; hardware success, malformed,
wrong-board, oversize, interrupted-upload, rollback, settings-retention, and USB-recovery
validation (see [quickstart.md](quickstart.md)).

**Target Platform**: Raspberry Pi Pico W (RP2040) release target; retain the existing
Pimoroni RP2350 option only with an explicitly matched FOTA image/board ID.

**Project Type**: Single embedded firmware target plus CMake-integrated host package output.

**Performance Goals**: Valid local-network update in under three minutes; progress for every
received chunk; preserve the 48-column POV pipeline except for the warned write/restart.

**Constraints**: One active upload/client; fixed metadata/parser state plus a 256-byte
aligned staging buffer; validate envelope size, board ID, and complete SHA-256 before
marking a slot valid; commit only after normal application startup; retain settings in the
final 4 KB; no CPU LED timing changes or heap in display/flash paths.

**Scale/Scope**: One owner-operated local HTTP interface, one `.povota` package per board,
no AP-mode update, no remote downloader, and no UI redesign beyond update pages.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | PASS | DMA → TX FIFO → PIO remains untouched. |
| II. Timing Precision | PASS | Scheduler is unchanged; UI warns of the intentional update pause/restart. |
| III. Hardware Abstraction | PASS | FOTA slot access, HTTP, UI, and persistence stay in distinct modules. |
| IV. Minimal & Deterministic Memory | PASS | Parser/session state and 256-byte staging are bounded; settings occupy the final sector of an 8 KB reserved flash tail. |
| V. Single-Command Build & Flash | PASS | CMake builds bootloader, app, USB UF2, and `.povota`; `ninja -C build` remains sufficient. |

**Post-design re-check**: PASS. New static RAM is bounded update/session state and a
256-byte aligned buffer; FOTA payloads reside in flash slots.

## Project Structure

### Documentation (this feature)

```text
specs/011-wifi-firmware-update/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/firmware-update-http.md
└── tasks.md
```

### Source Code (repository root)

```text
deps/pico_fota_bootloader/       # pinned third-party MIT source (new)
tools/package_firmware.py        # creates board-tagged .povota packages (new)
wifi_config/wifi_firmware_update.{c,h}  # session, stream, slot APIs (new)
wifi_config/wifi_sta_http.{c,h}  # streaming POST /update, authorization, status
wifi_config/wifi_sta_web.{c,h}   # upload/progress/recovery pages
wifi_config/wifi_flash.{c,h}     # settings layout compatibility
CMakeLists.txt                   # dependency, FOTA linker integration, artifacts
pov_leds.cpp                     # commits a healthy newly booted candidate
```

**Structure Decision**: Extend the STA portal rather than add a server. HTTP parses bounded
metadata/body chunks and delegates all FOTA transitions to `wifi_firmware_update`; the
third-party dependency owns slots/linker layout while this project owns its package envelope,
authorization, status messages, and settings reservation.

## Complexity Tracking

No constitution violations. A bootloader is required: a self-overwriting single-image
updater cannot meet the specified rollback/recovery guarantees on RP2040.
