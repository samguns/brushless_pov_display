# Implementation Plan: WiFi Configuration Web App

**Branch**: `001-wifi-config-webapp` | **Date**: 2026-06-25 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-wifi-config-webapp/spec.md`

## Summary

Implement WiFi provisioning firmware for the Raspberry Pi Pico W. On boot the device reads credentials from the last flash sector; if valid credentials are present it connects in STA mode. Otherwise (or after a failed STA connection) it starts a WPA2 AP (`pov-leds-setup` / `12345678`), runs a DNS captive-portal responder, and serves a lwIP httpd configuration page where the user selects their network from a scan list (with manual-entry fallback for hidden networks) and enters a password. The device validates the connection before persisting credentials, then reboots into STA mode.

## Technical Context

**Language/Version**: C11 / C++17 (existing project standard)

**Primary Dependencies**: Pico SDK 2.2.0 · CYW43 driver (cyw43-driver, bundled) · lwIP with `pico_lwip_http` (bundled) · `hardware/flash.h` (RP2040 flash write API)

**Storage**: Last 4 KB flash sector at offset `PICO_FLASH_SIZE_BYTES - 4096`. Single fixed-size credential record (106 bytes) with magic + CRC-32 validity check. See [data-model.md](data-model.md).

**Testing**: Manual hardware validation only (see [quickstart.md](quickstart.md)). No host-side unit test framework is available for RP2040 firmware in this project. Flash read/write and CRC logic can be validated on-device via serial debug output.

**Target Platform**: Raspberry Pi Pico W (RP2040 + CYW43439), 264 KB SRAM, 2 MB flash

**Project Type**: Embedded firmware module (C source files added to existing `pov_leds` CMake target)

**Performance Goals**: AP visible within 10 s of boot · configuration page loads within 3 s · connection attempt result within 30 s · auto-connect within 20 s on subsequent boots

**Constraints**: 264 KB SRAM total (shared with existing firmware) · no heap in flash-write or connection-validation paths · all web content served from flash (no SD card) · HTTP only (no TLS) · CYW43439 does not support simultaneous AP + STA

**Scale/Scope**: Single-user provisioning flow · one credential stored at a time · max 20 scan results displayed

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

[Gates determined based on constitution file]

## Project Structure

### Documentation (this feature)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | ✅ N/A | This feature adds no LED output paths. |
| II. Timing Precision | ✅ N/A | No POV column timing involved. |
| III. Hardware Abstraction | ✅ COMPLIANT | WiFi flash driver (`wifi_flash.c`), scan wrapper (`wifi_scan.c`), and httpd handlers (`wifi_http.c`) are separate from each other and from the main boot logic. |
| IV. Minimal & Deterministic Memory | ✅ COMPLIANT | Credential struct and scan result array are statically allocated. No `malloc` in flash-write or connection-validation paths. Web buffers bounded by lwIP config. |
| V. Single-Command Build | ✅ COMPLIANT | All new `.c` files added to `CMakeLists.txt` `target_sources`. `pico_lwip_http`, `pico_cyw43_arch_lwip_poll` linked via `target_link_libraries`. `ninja -C build` remains the only command. |

**Post-design re-check**: PASS. No violations introduced by the data model or API contract design.

## Project Structure

### Documentation (this feature)

```text
specs/001-wifi-config-webapp/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/
│   └── http-api.md      # Phase 1 output
└── tasks.md             # Phase 2 output (speckit-tasks)
```

### Source Code (repository root)

```text
wifi_config/
├── wifi_config.h        # Public API: wifi_config_init(), wifi_config_run()
├── wifi_config.c        # Boot orchestration: read flash → STA attempt → AP fallback
├── wifi_flash.h         # API: load_credentials(), save_credentials(), clear_credentials()
├── wifi_flash.c         # Raw flash erase+write, CRC-32, magic validation (runs from RAM)
├── wifi_scan.h          # API: wifi_scan_start(), wifi_scan_get_results()
├── wifi_scan.c          # CYW43 scan callback, deduplication, RSSI sort
├── wifi_http.h          # API: wifi_http_start(), wifi_http_stop()
├── wifi_http.c          # lwIP httpd CGI handlers: GET /, POST /connect, GET /scan
└── wifi_web.h           # Embedded HTML as C string constant (config page + error states)

pov_leds.cpp             # Existing main — calls wifi_config_init() before POV loop
CMakeLists.txt           # Existing — add wifi_config/ sources + pico_lwip_http + cyw43_arch
lwipopts.h               # lwIP configuration overrides (must exist; add if missing)
```

**Structure Decision**: Single embedded firmware project. All new WiFi configuration code lives in a `wifi_config/` subdirectory to isolate it from the existing POV LED code. The existing `pov_leds.cpp` calls `wifi_config_init()` at startup. No separate build target is created; the module is compiled into the same `pov_leds` executable. This satisfies Constitution Principle III (hardware abstraction via module separation) and Principle V (single build target, no new CMake targets needed).

## Complexity Tracking

> No constitution violations to justify.
