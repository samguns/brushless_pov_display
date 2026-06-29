# Implementation Plan: Management Portal UI/UX Redesign

**Branch**: `007-portal-ui-redesign` | **Date**: 2026-06-29 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/007-portal-ui-redesign/spec.md`

## Summary

Re-skin the existing STA management portal to match the `pov-mgmt` Figma design:
a dark, card-based admin layout with a left navigation sidebar, an **Overview**
dashboard (Status / Network (SSID) / IP Address / blink metric cards) and a
**Settings** screen grouped into **Display**, **System**, and **Network** cards.
The redesign reuses the existing server-rendered, flash-served HTML model
(`wifi_sta_web` builds pages, `wifi_sta_http` routes, no external assets) and
preserves all current behavior (status, scan, Wi-Fi change, firmware update).

Two design controls are made functional: a **client-side Dark/Light theme**
toggle (CSS variables + a few lines of inline JS, dark default, browser-persisted)
and a **brightness control** that scales the live WS2812 frame output and
**persists across reboots** (a new byte in the reserved flash credential sector,
read at boot and applied each frame). The Static-IP toggle + IP/Subnet/Gateway
fields and the account/notification/logout chrome are rendered for visual
fidelity only (read-only / decorative). Typography uses a self-contained system
font stack (no embedded webfonts).

## Technical Context

**Language/Version**: C11 / C++17 (existing project standard)

**Primary Dependencies**: Pico SDK 2.2.0, `pico_cyw43_arch_lwip_poll`, lwIP raw
TCP (`lwip/tcp.h`), `hardware_flash` (via `wifi_flash`), the existing
`ws2812_driver` (PIO/DMA WS2812 output), and the existing `wifi_sta_web` /
`wifi_sta_http` / `wifi_config` / `wifi_scan` modules. No new third-party
libraries; the redesign is HTML/CSS/inline-JS authored as C string builders.

**Storage**: Existing reserved last-4 KB flash credential sector (`wifi_flash`).
A new **V3** record adds a single `uint8_t brightness` byte while remaining
backward-compatible with V1/V2 records (older records read with a default
brightness). Record stays well within the 4 KB sector; no linker-script change.

**Testing**: Manual hardware validation with a browser (desktop + phone width)
and the running device; build verification via `ninja -C build`; visual
comparison of rendered pages against the two Figma frames (Overview `1:2`,
settings-screen `9:4`); brightness verified by observing the LED panel and a
reboot; page-size headroom verified by inspecting the returned `Content-Length`
against the static page buffer.

**Target Platform**: Pimoroni Pico Plus 2 W (RP2350B + RM2) running the existing
firmware super-loop (`wifi_config_runtime_step()` + WS2812 render each iteration).

**Project Type**: Single embedded firmware target.

**Performance Goals**: Pages render/serve as fast as today (single client, one
page per request); brightness changes take effect on the next rendered frame
(sub-frame latency, no perceptible lag); theme toggle is instant (client-side);
no regression to LED column/render timing.

**Constraints**: Self-contained pages, no external/CDN assets, minimal inline JS;
single HTTP client at a time; no heap in the flash-write or LED-render paths;
brightness scaling must not alter WS2812 bit timing or rotation/column timing;
flash writes for brightness must be infrequent (persist only on an explicit,
changed submit — not per slider drag) to limit wear; redesigned pages must fit
the static page buffer without truncation.

**Scale/Scope**: Two primary screens (Overview, Settings) plus re-skinned
transitional pages (firmware confirm, rebooting, applying); up to 20 scan results
(existing `WIFI_SCAN_MAX_RESULTS`); brightness range 0–100% mapped to a safe
non-zero floor; one management-portal client at a time.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | PASS | Brightness only **scales pixel data values** before the existing DMA → TX FIFO → PIO pipeline. No bit-banging and no change to the output path structure; the CPU still just prepares the frame buffer. |
| II. Timing Precision | PASS | Brightness changes data values only — WS2812 bit timing (PIO clock divider) and any rotation/column strobe timing are untouched. Scaling is O(active_count ≤ 57) integer math in the super-loop, not in an ISR or the PIO feed; jitter budget unaffected. |
| III. Hardware Abstraction | PASS | Page rendering stays in `wifi_sta_web`, HTTP routing/parsing in `wifi_sta_http`, connection control in `wifi_config`, persistence in `wifi_flash`, and brightness *application* in `ws2812_driver` (driver-level global intensity) with the brightness *value* owned by `wifi_config`. Display logic stays testable behind these seams. |
| IV. Minimal & Deterministic Memory | PASS (with budget) | No heap added. Richer markup enlarges the static page buffer from 8 KB to a documented fixed size (see research RES-004); flash record grows by 1 byte (V3). RAM delta is bounded, static, and recorded. |
| V. Single-Command Build & Flash | PASS | Still builds with `ninja -C build`; only source edits + (optionally) new `wifi_sta_web` helpers. No new build steps, scripts, or linker changes. |

**Post-design re-check**: PASS. The design confines new behavior to (a) a global
brightness scalar applied to the already-prepared frame and (b) one persisted
byte; both respect the existing module seams and timing/memory rules. The only
tracked tradeoff is enlarging the static page buffer (Complexity Tracking).

## Project Structure

### Documentation (this feature)

```text
specs/007-portal-ui-redesign/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── http-api.md
├── checklists/
│   └── requirements.md
└── tasks.md            # created by /speckit-tasks (not this command)
```

### Source Code (repository root)

```text
wifi_config/
├── wifi_sta_web.h / .c     # redesigned page builders: Overview (status),
│                           #   Settings (Display+System+Network), re-skinned
│                           #   firmware/applying/rebooting pages; shared dark
│                           #   theme CSS (with light-theme variables) + tiny
│                           #   theme-toggle JS; brightness slider markup
├── wifi_sta_http.h / .c    # routing: GET / (Overview), GET /settings
│                           #   (+?scan=1), POST /config (unchanged), new
│                           #   POST /display (brightness); enlarged page buffer
├── wifi_config.h / .c      # owns runtime brightness value; get/set + persist;
│                           #   exposes firmware version + brightness to portal
└── wifi_flash.h / .c       # V3 record (adds brightness byte); load/save helpers
                            #   preserve credentials when updating brightness

ws2812_driver.h / .cpp      # add global brightness (0–255) setter; scale GRB
                            #   words inside submit_frame (no timing change)

pov_leds.cpp                # apply persisted brightness at startup; keep frame
                            #   render→submit loop (brightness handled in driver)
```

**Structure Decision**: Extend the existing STA management portal in place rather
than introduce new modules. The two-screen design maps onto existing routes:
`GET /` becomes the **Overview** screen and the former `GET /wifi` becomes the
**Settings** screen (served at `GET /settings`, with `GET /wifi` kept as an alias
for compatibility) that hosts the Display, System, and Network cards. Brightness
is the one genuinely new capability; it is layered as a driver-level intensity
scalar plus a persisted byte, keeping the HTTP, render, and flash responsibilities
in their current modules.

## Complexity Tracking

| Item | Why Needed | Mitigation / Why Acceptable |
|------|------------|-----------------------------|
| Enlarge the static page buffer (`STA_PAGE_BUF_SIZE`) beyond 8 KB | The redesigned dark/card CSS plus the Settings page (three cards + up to 20-network scan list) produce larger HTML than the current plain pages | Buffer is a single fixed, statically-allocated array sized with headroom (see research RES-004) on a 520 KB-SRAM RP2350B; no heap, no per-request allocation; size is documented and guarded so overflow returns an error rather than truncating. |
| Brightness scales the live LED frame | FR-017 requires the brightness control to change actual LED output | Applied as an O(≤57) integer scale on already-prepared GRB words inside `ws2812_driver_submit_frame`; does not touch PIO bit timing or rotation/column timing, and runs in the super-loop, not an ISR (Principles I & II preserved). |
| New persisted byte (flash V3 record) | FR-017 requires brightness to survive reboot | One extra byte in the already-reserved 4 KB sector; V1/V2 still readable; writes happen only on an explicit, value-changed brightness submit (not per drag) to limit flash wear; existing atomic erase+write+verify reused. |
