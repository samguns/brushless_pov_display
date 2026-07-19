# Implementation Plan: Web Log Viewer

**Branch**: `master` | **Date**: 2026-07-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/020-web-log-viewer/spec.md`

## Summary

Add a read-only Logs screen to the existing station-mode management portal so
an owner can inspect recent and live diagnostic events while the rotating PCB is
running without USB. The firmware will capture explicitly classified, sanitized
events in a fixed 128-entry `pov_log` ring initialized before Wi-Fi startup. A
self-contained browser page will retrieve bounded batches through short, same-
origin HTTP requests using boot-session and sequence cursors for de-duplication,
gap detection, pause/resume, and reconnect recovery. Development builds may
mirror the same safe entries to USB; production capture remains active with USB
and UART stdio disabled.

## Technical Context

**Language/Version**: C11 and C++17; self-contained HTML/CSS/JavaScript emitted
as C string builders

**Primary Dependencies**: Pico SDK 2.2.0, `pico_rand`,
`pico_cyw43_arch_lwip_poll`, lwIP raw TCP (`lwip/tcp.h`), and the existing
`wifi_sta_http`, `wifi_sta_web`, `wifi_config`, Hall/clock, and WS2812 modules;
no new third-party dependency

**Storage**: Current-boot-only fixed ring: 128 entries x 120 bytes plus bounded
metadata, no flash persistence and no heap. Page and JSON responses reuse the
existing 16 KiB STA HTTP response buffer.

**Testing**: Pure host tests compiled with g++ for ring, cursor, JSON/security,
and page behavior; existing host regressions; `ninja -C build`; release build
with `POV_DEMO_DEV_USB_STDIO=OFF`; ELF size/symbol inspection; browser, Wi-Fi
recovery, and logic-analyzer validation on hardware

**Target Platform**: Raspberry Pi Pico W (RP2040, 264 KiB SRAM) with the
repository's existing Pimoroni Pico Plus 2 W alternate target retained

**Project Type**: Single embedded firmware image with an onboard local web UI

**Performance Goals**: At least 95% of entries visible within 2 seconds and all
within 5 seconds on the local network; zero duplicate/reordered entries in a
1,000-entry test; newest 128 entries retained; no missing Hall events or columns
and no regression beyond the existing sub-microsecond display-jitter budget

**Constraints**: Added persistent static RAM <=16 KiB; fixed entry/response
sizes; no heap, blocking network send, or interrupt masking in log capture; no
logging from Hall/PIO/DMA interrupt or per-column paths; one active HTTP client;
short close-delimited requests; maximum 16 entries per JSON batch; fully local
and self-contained UI; secrets redacted before retention

**Scale/Scope**: One device, one current boot session, one active viewer, 128
retained entries, approximately a dozen fixed source categories, two new GET
routes, and existing normal log rates (roughly clock + Hall messages once per
second plus state transitions)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. PIO-First LED Drive | PASS | Logging and HTTP presentation do not alter the DMA -> TX FIFO -> PIO LED pipeline or introduce CPU bit-banging. |
| II. Timing Precision | PASS | Capture is a bounded main/poll-context write, never an ISR or per-column action. Responses are capped at 16 entries and reuse the existing buffer. Release console output is compiled out; hardware comparison with an active viewer remains a required acceptance gate. |
| III. Hardware Abstraction | PASS | `pov_log` owns hardware-independent history/order rules; `wifi_log_web` owns Logs HTML/JSON presentation and `wifi_sta_web` owns shared navigation; `wifi_sta_http` owns routing/transport; hardware modules only emit classified events from safe contexts. Pure history and presentation behavior are host-testable. |
| IV. Minimal and Deterministic Memory Use | PASS | The planned target layout is 15,392 bytes maximum for the 128-entry ring and metadata, below the 16 KiB feature cap. HTTP storage is reused, all buffers are fixed, and capture performs no heap allocation. |
| V. Single-Command Build and Flash | PASS | The new C source and existing Pico SDK library are registered in `CMakeLists.txt`; the normal build remains `ninja -C build` and no runtime service or external asset is added. |

**Post-design re-check**: PASS. Phase 1 keeps the event store independent from
the web server, bounds both stored entries and each serialized batch, treats
browser viewer state as client-only, and adds no persistent storage, interrupt
work, PIO/DMA changes, or extra HTTP response buffer. The final BSS delta and
physical timing comparison are validation gates, not deferred design choices.

## Project Structure

### Documentation (this feature)

```text
specs/020-web-log-viewer/
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- log-viewer-http.md
|-- checklists/
|   `-- requirements.md
`-- tasks.md                 # generated later by /speckit-tasks
```

### Source Code (repository root)

```text
CMakeLists.txt               # register pov_log, pico_rand, console mirror flag
pov_log.h
pov_log.c                    # bounded ring, session/order, sanitization, snapshots
pov_leds.cpp                 # initialize boot session; structured app log sources
dhcpserver.c                 # structured, safe DHCP events

wifi_config/
|-- wifi_config.c            # structured connection/runtime events
|-- wifi_flash.c             # structured safe persistence events
|-- wifi_scan.c              # structured scan events
|-- wifi_http.c              # safe AP events; remove credential-body logging
|-- wifi_dns.c               # structured DNS events
|-- wifi_firmware_update.c   # bounded update state-transition events
|-- wifi_sta_http.h
|-- wifi_sta_http.c          # /logs routes, query parsing, bounded JSON response
|-- wifi_sta_web.h
|-- wifi_sta_web.c           # shared portal pages and Logs navigation
|-- wifi_log_web.h
`-- wifi_log_web.c           # Logs page, JSON escaping, client viewer behavior

tests/
|-- pov_log_test.cpp         # ring, memory, order, overwrite, session, sanitizer
|-- wifi_log_viewer_test.cpp # page/JSON/cursor/gap/security contract tests
|-- wifi_reboot_controls_test.cpp # existing portal regression + Logs navigation
`-- pov_adaptive_rendering_test.cpp # existing display timing/phase regression
```

**Structure Decision**: Add one hardware-independent root logging module rather
than intercepting global stdio or placing history inside the Wi-Fi layer. This
gives C and C++ producers an auditable safe API even when stdio is disabled and
keeps history testable without lwIP. Extend the STA presentation and raw-TCP
modules: a dedicated pure Logs builder remains presentation-only, while routing and
the shared asynchronous response lifetime remain owned by `wifi_sta_http`.

## Design Decisions

- `pov_log` is initialized immediately after stdio setup and before
  `wifi_config_init`, using a nonzero random 64-bit boot identifier and a bounded
  boot-relative millisecond clock callback.
- Each 120-byte entry contains a 64-bit uptime, 32-bit sequence, source enum,
  flags, stored length, and 101-byte text array (up to 100 visible bytes plus
  NUL). Invalid UTF-8 is replaced and truncation ends on a code-point boundary.
- Explicit producer calls replace first-party operational `printf` sites. Safe
  entries optionally mirror to development USB; release builds compile the
  mirror out. The AP request-body print that can contain credentials is removed.
- `GET /logs` serves the UI. `GET /logs/updates` returns zero to sixteen entries
  from the shared response buffer. A 1-second sequential poll is used instead of
  SSE, WebSockets, or long polling so the sole HTTP client slot is released.
- The browser retains at most 128 rows. Pause uses metadata-only requests and
  does not advance its displayed cursor; resume catches up from that cursor.
  Reboot clears prior rows and shows an explicit new-session marker; overwritten
  ranges produce a visible gap marker.
- Logs use the existing unauthenticated, same-LAN read boundary of other portal
  GETs. Responses are same-origin, non-CORS, `no-store`, and rendered only via
  DOM text nodes/`textContent`.

## Complexity Tracking

No constitution violations or unjustified architectural complexity remain. The
fixed history consumes a material but explicitly capped amount of SRAM; it is
the minimum storage that satisfies the required 128-entry current-boot history
with useful source, timestamp, ordering, and message data.
