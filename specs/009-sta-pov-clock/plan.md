# Implementation Plan: STA POV Clock Display

**Branch**: `009-sta-pov-clock` | **Date**: 2026-07-10 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/009-sta-pov-clock/spec.md`

## Summary

Add a STA-mode wall clock that calibrates UTC time from the network, converts it
to China Standard Time (UTC+8), qualifies the measured Hall-sensor rotation, and
renders a compact `HH:MM:SS CST` POV clock on the 57-LED radial row. The design
keeps the existing super-loop, raw-lwIP polling model, DMA -> TX FIFO -> PIO
WS2812 output path, and statically bounded memory. A small raw UDP time client
calibrates time without sockets, a display state module owns CST conversion and
health, and a 40-column renderer advances columns from the measured rotation
period.

## Technical Context

**Language/Version**: C11 / C++17 (existing project standard)

**Primary Dependencies**: Pico SDK 2.2.0; `pico_cyw43_arch_lwip_poll`; lwIP raw
UDP and DNS client APIs; existing `ws2812_driver`, `hall_sensor`, and
`wifi_config` modules; existing DMA -> TX FIFO -> PIO WS2812 transport

**Storage**: N/A for new persistent storage; runtime state and render buffers
are bounded static or long-lived stack objects

**Testing**: Build verification with `ninja -C build`; pure function checks for
CST conversion, rotation suitability, and renderer column generation using
synthetic timestamps; manual device validation with STA network access, Hall
sensor speed readings, and observed POV output

**Target Platform**: Raspberry Pi Pico W / Pimoroni Pico Plus 2 W running the
existing firmware super-loop

**Project Type**: Single embedded firmware target

**Performance Goals**: Time calibration within 10 seconds on a reachable
network; displayed CST time within +/-1 second after calibration; nominal 600
RPM spin target with supported 480-800 RPM validation range; 40 angular columns
per revolution with per-column intervals derived from measured period; leave
normal clock mode within 2 revolutions or 1 second after invalid time/rotation

**Constraints**: No heap allocation in timekeeping, rotation synchronization, or
display rendering paths; no CPU bit-banging of LED protocol; maintain <1 us
column scheduling jitter target where timer-driven column changes are used; raw
lwIP/no sockets; preserve one cooperative firmware loop and existing STA portal;
static RAM delta target under 2 KB

**Scale/Scope**: One network time source domain with fallback retry, one CST
timezone (UTC+8, no DST), one Hall-measured spin domain, one 57-LED radial row,
one compact built-in clock layout

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | PASS | WS2812 frame emission remains DMA -> TX FIFO -> PIO. New rendering only prepares 57-word column frames; it never bit-bangs LED timing. |
| II. Timing Precision | PASS | Column cadence is derived from the measured Hall rotation period. WS2812 timing remains clock-derived through the existing driver. |
| III. Hardware Abstraction | PASS | Time calibration, CST/display health, rotation suitability, and column rendering are separate modules; `pov_leds.cpp` only wires them together. |
| IV. Minimal & Deterministic Memory | PASS | New time, renderer, and frame state are bounded; no heap allocation in timing-critical paths; RAM delta is tracked in design artifacts. |
| V. Single-Command Build & Flash | PASS | Source additions are wired into the existing `CMakeLists.txt`; normal `ninja -C build` and flashing workflow stay unchanged. |

**Post-design re-check**: PASS. Research, data model, contract, and quickstart
artifacts preserve all constitution gates; no justified violations are required.

## Project Structure

### Documentation (this feature)

```text
specs/009-sta-pov-clock/
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- pov-clock-contract.md
|-- checklists/
|   `-- requirements.md
`-- tasks.md            # created by /speckit-tasks
```

### Source Code (repository root)

```text
time_sync.h             # raw-lwIP network time calibration interface
time_sync.cpp           # DNS + raw UDP NTP request/response state machine
pov_clock.h             # CST clock state, rotation suitability, display health API
pov_clock.cpp           # pure time conversion and suitability derivation
pov_clock_renderer.h    # compact 40-column POV clock renderer interface
pov_clock_renderer.cpp  # HH:MM:SS CST glyph layout and column frame generation
pov_leds.cpp            # top-level orchestration of Wi-Fi, time, rotation, renderer
CMakeLists.txt          # add new sources and keep existing libraries

hall_sensor.*           # existing rotation-period measurement source
ws2812_driver.*         # existing PIO/DMA WS2812 transport
wifi_config/            # existing STA runtime and portal modules
```

**Structure Decision**: Keep the project as one firmware target and add three
small modules rather than growing the existing Hello demo. `time_sync.*` owns
network calibration, `pov_clock.*` owns UTC+8 wall-clock and health decisions,
and `pov_clock_renderer.*` owns text-to-column mapping. This preserves the
existing driver/display split and allows pure logic to be validated without
hardware.

## Complexity Tracking

No constitution violations requiring justification.

## Static RAM Notes

Build size after implementation:

```text
text=382456 data=0 bss=65212 dec=447668 file=build/pov_leds.elf
```

Feature-owned long-lived state is estimated under 512 bytes:

| Component | Bytes |
|-----------|-------|
| `time_sync_t` instance | ~80 |
| `pov_clock_time_t` instance | ~32 |
| `pov_clock_rotation_t` instance | ~24 |
| `pov_clock_renderer_t` instance including 48 column masks | ~72 |
| Existing 57-word frame buffer reused by clock renderer | 228 |
| **Estimated feature runtime state** | **~436** |

- The compact glyph tables are `static const` data and live in flash/rodata.
- The raw lwIP UDP PCB and NTP packet pbuf use existing lwIP memory pools; no
  heap allocation is introduced in the display rendering path.
- The estimated feature-owned state remains below the 2 KB target from the spec.
