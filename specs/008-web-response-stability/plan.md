# Implementation Plan: Web Response Stability

**Branch**: `008-web-response-stability` | **Date**: 2026-07-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/008-web-response-stability/spec.md`

## Summary

Make the STA management portal recover from silent browser sockets and stalled response transfers. The implementation keeps the existing raw-lwIP, single-client, static-buffer model but adds per-client progress tracking and a poll-driven timeout so an idle or wedged TCP PCB cannot permanently occupy the only client slot.

## Technical Context

**Language/Version**: C11 / C++17

**Primary Dependencies**: Pico SDK 2.2.0, `pico_cyw43_arch_lwip_poll`, lwIP raw TCP (`lwip/tcp.h`), existing `wifi_sta_http` and `wifi_sta_web` modules

**Storage**: N/A

**Testing**: Build verification with `ninja -C build`; manual device/browser validation using repeated page loads and idle/stalled socket scenarios

**Target Platform**: Raspberry Pi Pico W / Pimoroni Pico Plus 2 W running the existing firmware super-loop

**Project Type**: Single embedded firmware target

**Performance Goals**: Silent clients released within 5 seconds; stalled transfers released within 10 seconds; normal active transfers are not interrupted

**Constraints**: No heap allocation; preserve one active client at a time; do not add external assets or networking libraries; keep Wi-Fi polling cooperative; do not change LED timing paths

**Scale/Scope**: One management-portal client slot, fixed request/page/header buffers, existing portal routes only

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | PASS | The change is limited to management HTTP state and does not alter LED output. |
| II. Timing Precision | PASS | No PIO timing, timer alarm, or column strobe behavior changes. |
| III. Hardware Abstraction | PASS | HTTP connection reliability stays in `wifi_sta_http`; page builders and Wi-Fi configuration ownership remain unchanged. |
| IV. Minimal & Deterministic Memory | PASS | Adds only a few scalar fields to existing static module state; no heap and no larger buffers. |
| V. Single-Command Build & Flash | PASS | Build remains `ninja -C build`; no new scripts, tools, or dependencies. |

**Post-design re-check**: PASS. The design adds timeout bookkeeping inside the existing HTTP module and does not affect PIO, DMA, flash, or page rendering boundaries.

## Project Structure

### Documentation (this feature)

```text
specs/008-web-response-stability/
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- http-reliability.md
|-- checklists/
|   `-- requirements.md
`-- tasks.md
```

### Source Code (repository root)

```text
wifi_config/
|-- wifi_sta_http.c    # client progress tracking, poll timeout, close cleanup
`-- wifi_sta_http.h    # no public API change expected
```

**Structure Decision**: Extend the existing STA HTTP server in place. The reliability issue is caused by per-client TCP state lifetime, so the fix belongs in `wifi_sta_http.c` rather than in page rendering, Wi-Fi configuration, or the main loop.

## Complexity Tracking

No constitution violations or extra architectural complexity are introduced.
