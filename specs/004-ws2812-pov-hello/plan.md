# Implementation Plan: WS2812 POV Hello Demo

**Branch**: `004-ws2812-pov-hello` | **Date**: 2026-06-26 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/004-ws2812-pov-hello/spec.md`

## Summary

Replace the current single-pin blink PIO path with a WS2812-capable DMA -> TX FIFO -> PIO output path for up to 57 LEDs, then deliver a deterministic POV demo that displays H, e, l, l, o character-by-character with a 1-second hold per character in a continuous loop. The implementation keeps the existing single-command build flow, uses bounded static memory, derives timing parameters from `clock_get_hz(clk_sys)`, and logs startup/transition/error states for validation.

## Technical Context

**Language/Version**: C11 / C++17 (existing project standard)

**Primary Dependencies**: Pico SDK 2.2.0, PIO toolchain via `pico_generate_pio_header`, `hardware_pio`, `hardware_dma`, `hardware_clocks`, `pico_stdlib`

**Storage**: N/A for persistent storage in this feature; runtime uses bounded static buffers/state only

**Testing**: Manual hardware validation on Pico W + WS2812 strip, build verification via `ninja -C build`, runtime timing checks via USB serial logs and observation

**Target Platform**: Raspberry Pi Pico W (RP2040) driving external WS2812 LED strip

**Project Type**: Single embedded firmware target

**Performance Goals**: 1.0 s per character (+/-0.1 s), correct H/e/l/l/o ordering, stable continuous playback for >=5 minutes at 57 LEDs

**Constraints**: Max LED count 57, no heap allocation in display path, deterministic timing from runtime clock queries (`clock_get_hz(clk_sys)`), no hardcoded system-clock constants in timing path, preserve current build/flash workflow

**Scale/Scope**: One built-in demo message (Hello), single renderer path, one strip configuration domain (1..57 LEDs)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | PASS | LED signaling uses DMA -> TX FIFO -> PIO; CPU does not bit-bang WS2812 protocol. |
| II. Timing Precision | PASS | Timing parameters are derived from `clock_get_hz(clk_sys)` and validated against cadence tolerance. |
| III. Hardware Abstraction | PASS | Driver-level WS2812 output and demo-sequence logic are separated in plan scope. |
| IV. Minimal & Deterministic Memory | PASS | Static bounded state/frame storage only; RAM impact documented in artifacts. |
| V. Single-Command Build & Flash | PASS | Existing `ninja -C build` and flashing workflow remain unchanged. |

**Post-design re-check**: PASS. Research, data model, contract, and quickstart artifacts maintain all constitution gates without exception.

## Project Structure

### Documentation (this feature)

```text
specs/004-ws2812-pov-hello/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── pov-demo-contract.md
├── checklists/
│   └── requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
blink.pio                        # replaced with WS2812-capable PIO program
ws2812_driver.h                  # hardware-facing WS2812 driver interface
ws2812_driver.cpp                # PIO initialization and frame push implementation
pov_demo.h                       # demo-sequence and playback API
pov_demo.cpp                     # Hello sequencing, scheduler, and rendering logic
pov_leds.cpp                     # top-level orchestration and wiring
CMakeLists.txt                   # existing PIO header generation/build wiring

wifi_config/                     # existing networking modules (unchanged unless required)
```

**Structure Decision**: Keep a single firmware target but separate hardware-facing WS2812 driver code from POV playback logic using dedicated modules (`ws2812_driver.*` and `pov_demo.*`) while keeping `pov_leds.cpp` as orchestration only.

## Complexity Tracking

No constitution violations requiring justification.
