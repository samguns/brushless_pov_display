# Implementation Plan: PIO Blink Concurrent with STA HTTP Server

**Branch**: `003-pio-blink-sta-server` | **Date**: 2026-06-26 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/003-pio-blink-sta-server/spec.md`

## Summary

Refactor runtime control flow so PIO blink output and STA HTTP servicing run concurrently in one non-blocking main loop. The STA portal remains active while blinking continues at stable cadence, including during link drops and reconnects. Existing AP provisioning and credential persistence are preserved, while mutating STA endpoints are protected by a persisted admin token and throttled on repeated unauthorized requests.

## Technical Context

**Language/Version**: C11 / C++17 (existing project standard)

**Primary Dependencies**: Pico SDK 2.2.0, `pico_cyw43_arch_lwip_poll`, lwIP raw TCP API, `hardware_pio`, `hardware_timer`, existing `wifi_config/*` and `wifi_sta_http/*` modules

**Storage**: Existing credential flash record at last 4 KB sector (`memmap_wifi_creds.ld` protected); extended to include persisted STA admin token

**Testing**: Manual hardware validation on Pico W, build validation via `ninja -C build`, runtime validation via USB serial logs and LAN browser checks

**Target Platform**: Raspberry Pi Pico W (RP2040 + CYW43)

**Project Type**: Single embedded firmware target

**Performance Goals**: No visible blink freeze during 60 s repeated HTTP access; >=95% HTTP success during normal LAN usage; reconnect recovery <=20 s after network returns; blink cadence error within +/-5%

**Constraints**: Deterministic memory use (no heap in blink/ISR/hot polling paths), maintain PIO-first timing path, preserve AP provisioning behavior, protect mutating endpoints with token + throttling, single-command build remains `ninja -C build`

**Scale/Scope**: Single device, typically one LAN operator at a time, small embedded HTTP surface (`/`, status and config/update endpoints)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | PASS | Blink remains PIO-driven; no CPU bit-banging introduced. |
| II. Timing Precision | PASS | Blink cadence maintained with runtime-derived clock assumptions and non-blocking scheduling. |
| III. Hardware Abstraction | PASS | Blink runtime and WiFi/HTTP runtime remain in separate modules with explicit integration points. |
| IV. Minimal/Deterministic Memory | PASS | Static state only for blink + auth throttle tracking; no dynamic allocation in critical paths. |
| V. Single-Command Build/Flash | PASS | Changes stay in existing `pov_leds` target; `ninja -C build` unchanged. |

**Post-design re-check**: PASS. Research and design artifacts preserve all constitution gates with no required exceptions.

## Project Structure

### Documentation (this feature)

```text
specs/003-pio-blink-sta-server/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── http-api.md
├── checklists/
│   └── requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
pov_leds.cpp                    # main orchestration loop (blink + wifi/http poll)
blink.pio                       # PIO blink program (unchanged instruction set)
wifi_config/
├── wifi_config.h               # split init/step API for non-blocking runtime
├── wifi_config.c               # provisioning/STA state orchestration refactor
├── wifi_flash.h
├── wifi_flash.c                # credential + token persistence updates
├── wifi_sta_http.h
├── wifi_sta_http.c             # endpoint auth + throttling behavior
├── wifi_sta_web.h
├── wifi_sta_web.c              # status/config/update page content
├── wifi_http.*                 # AP-mode portal (unchanged behavior)
├── wifi_dns.*
└── wifi_scan.*

CMakeLists.txt                  # source wiring only if new files/symbols are introduced
```

**Structure Decision**: Keep a single embedded project. Add concurrency by introducing non-blocking runtime-step APIs and central event loop control in `pov_leds.cpp`, while preserving module boundaries for blink, provisioning/AP, and STA HTTP concerns.

## Complexity Tracking

No constitution violations requiring justification.
