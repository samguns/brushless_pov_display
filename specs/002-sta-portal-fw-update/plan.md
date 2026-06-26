# Implementation Plan: STA Management Portal & Firmware Update

**Branch**: `002-sta-portal-fw-update` | **Date**: 2026-06-25 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/002-sta-portal-fw-update/spec.md`

## Summary

Extend the existing WiFi-provisioning firmware (feature 001) so that (1) WiFi credentials survive a UF2 firmware update, (2) the device serves a management web page while connected in STA mode, and (3) that page can reboot the device into USB mass-storage (BOOTSEL) mode for firmware updates.

Credential persistence is achieved passively: a custom linker script reserves the last 4 KB flash sector (the same sector `wifi_flash.c` already uses) by shortening the linkable `FLASH` region to `PICO_FLASH_SIZE_BYTES - 4096` and adding a linker `ASSERT` that fails the build if the firmware binary would overflow into the reserved region. Because the RP2040 UF2 bootloader only writes the pages contained in the UF2 file, a normal firmware update never touches the reserved sector.

The STA management portal reuses the existing raw-lwIP TCP HTTP server pattern from `wifi_http.c`. After `wifi_config_init()` returns (STA connected), `main()` calls a new `wifi_config_run_sta()` that prints the acquired IP/SSID to serial, starts an STA-mode HTTP server, and polls it. The server serves a status page (`GET /`), a confirmation page (`GET /update`), and a confirm action (`POST /update`) that calls `reset_usb_boot(0, 0)`. A 60-second countdown is shown on the confirmation page; if WiFi drops, the device attempts silent reconnect and falls back to AP provisioning after 15 s.

## Technical Context

**Language/Version**: C11 / C++17 (existing project standard)

**Primary Dependencies**: Pico SDK 2.2.0 · CYW43 driver (`pico_cyw43_arch_lwip_poll`) · lwIP raw TCP API (`lwip/tcp.h`) · `pico/bootrom.h` (`reset_usb_boot`) · `hardware/watchdog.h` · existing `wifi_config/` modules from feature 001

**Storage**: Reuses the feature-001 credential sector at `PICO_FLASH_SIZE_BYTES - 4096` (`0x1FF000`, XIP `0x101FF000`). No format change. Protection is added via linker reservation. See [data-model.md](data-model.md).

**Testing**: Manual hardware validation only (see [quickstart.md](quickstart.md)). Build-time enforcement (linker `ASSERT`) is verifiable on the host by inspecting build output; runtime behaviour is validated on-device via USB serial debug output (`pico_enable_stdio_usb 1`, already enabled).

**Target Platform**: Raspberry Pi Pico W (RP2040 + CYW43439), 264 KB SRAM, 2 MB flash

**Project Type**: Embedded firmware module (C source files added to the existing `pov_leds` CMake target)

**Performance Goals**: IP printed to serial within 500 ms of DHCP lease (SC-002) · management page loads within 3 s (SC-003) · confirm → USB MSD within 3 s (SC-004) · credentials survive 10 update cycles (SC-001)

**Constraints**: 264 KB SRAM shared with feature 001 (~26 KB already budgeted) · no heap in flash-write paths · all web content served from flash · HTTP only (no TLS) · CYW43439 is not simultaneous AP+STA (STA portal and AP portal never run concurrently) · firmware binary must not overflow the reserved credential sector

**Scale/Scope**: Single connected user · one management page · one credential record · firmware binary currently ~350 KB, far below the ~2044 KB reserved boundary

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | ✅ N/A | This feature adds no LED output paths. |
| II. Timing Precision | ✅ N/A | No POV column timing involved. The 60 s update countdown is UX, not LED timing. |
| III. Hardware Abstraction | ✅ COMPLIANT | New STA HTTP handlers live in `wifi_sta_http.c`; the USB-MSD reboot is a one-line `reset_usb_boot()` call isolated in the STA-run path. Flash protection is a linker concern, separate from `wifi_flash.c` runtime logic. |
| IV. Minimal & Deterministic Memory | ✅ COMPLIANT | STA HTTP server reuses statically sized buffers (mirrors `wifi_http.c`). No new heap in flash paths. Net new static RAM < 8 KB; documented in [data-model.md](data-model.md). |
| V. Single-Command Build | ✅ COMPLIANT | New `.c` files added to `CMakeLists.txt` `add_executable`. Custom linker script wired via `pico_set_linker_script()`. `ninja -C build` remains the only build command. |

**Post-design re-check**: PASS. The linker-script reservation and the new STA HTTP module introduce no constitution violations. No new build targets, no heap in flash paths, no hardcoded clock frequencies.

## Project Structure

### Documentation (this feature)

```text
specs/002-sta-portal-fw-update/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/
│   └── http-api.md      # Phase 1 output (STA management endpoints)
├── checklists/
│   └── requirements.md  # Spec quality checklist (already present)
└── tasks.md             # Phase 2 output (speckit-tasks)
```

### Source Code (repository root)

```text
wifi_config/
├── wifi_config.h        # +wifi_config_run_sta() declaration
├── wifi_config.c        # Existing AP/boot orchestration (unchanged logic);
│                        #   captures connected SSID, exposes it to STA run
├── wifi_sta_http.h       # NEW — API: wifi_sta_http_start/stop/poll, update-pending flag
├── wifi_sta_http.c       # NEW — STA-mode raw-lwIP server: GET /, GET /update, POST /update
├── wifi_sta_web.h        # NEW — status page + update confirmation page builders
├── wifi_sta_web.c        # NEW — HTML builders for the STA management portal
├── wifi_flash.{c,h}     # Unchanged (offset already at last sector)
├── wifi_http.{c,h}      # Unchanged (AP-mode provisioning server)
├── wifi_scan.{c,h}      # Unchanged
├── wifi_dns.{c,h}       # Unchanged
└── wifi_web.{c,h}       # Unchanged

memmap_wifi_creds.ld     # NEW — custom linker script: FLASH length = 2MB-4KB + ASSERT
pov_leds.cpp             # Existing main — after wifi_config_init() returns, calls
                         #   wifi_config_run_sta() before the POV/blink loop
CMakeLists.txt           # +wifi_sta_http.c, +wifi_sta_web.c, +pico_set_linker_script()
```

**Structure Decision**: Single embedded firmware project; this feature extends the existing `wifi_config/` module set. STA-mode serving is intentionally placed in separate files (`wifi_sta_http.c`, `wifi_sta_web.c`) rather than overloading the AP-mode `wifi_http.c`, because the two servers run in mutually exclusive phases and have different page sets — keeping them separate satisfies Constitution Principle III. The credential-protection mechanism is a build-time linker concern with no runtime code, wired through `CMakeLists.txt`.

## Complexity Tracking

> No constitution violations to justify.
