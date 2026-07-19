# Implementation Plan: Round-Display Cartesian Text Rendering

**Branch**: `018-round-display-rendering` | **Date**: 2026-07-12 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/018-round-display-rendering/spec.md`

## Summary

Replace the polar glyph-column model in `pov_clock_renderer` with a Cartesian
framebuffer plus per-column polar sampling. The clock text is rasterized upright
into a fixed 57x57 palette framebuffer. For each angular column, each LED's signed
radius (i - 28) is projected to a Cartesian point using a precomputed fixed-point
`cos/sin` table, and that framebuffer pixel's color is written to the LED. Pixels
outside the inscribed disc are dark. The rotation phase/period pipeline and the
40-column cadence are unchanged.

## Technical Context

**Language/Version**: C11 and C++17; Pico SDK 2.2.0

**Primary Dependencies**: `pov_clock_renderer` (font, rasterizer, sampler),
`pov_clock` (phase/rotation), WS2812 DMA-to-PIO driver, existing radial frame buffer

**Storage**: One static 57x57 palette framebuffer, two static per-column
fixed-point trig tables, existing renderer scalar state; no heap, no persistent
storage

**Testing**: Host unit tests (`tests/pov_adaptive_rendering_test.cpp`, g++);
`ninja -C build`; a Python preview tool renders the 40-column reconstruction for
visual confirmation

**Target Platform**: Raspberry Pi Pico W (RP2040, Cortex-M0+, no hardware FPU)

**Project Type**: Embedded real-time POV firmware

**Performance Goals**: One column rendered per loop step; sampling is integer
math (57 LEDs x a few ops); trig computed once at init; transfers never overlap

**Constraints**: 57 WS2812 LEDs (~1.81 ms/frame) cap ~40 columns/rev over 480-800
RPM; no heap or blocking in the render path; no runtime float in the hot loop

**Scale/Scope**: One renderer, one 57x57 image, 40 angular columns; change is
localized to `pov_clock_renderer.{h,cpp}` and its tests

## Constitution Check

| Principle | Status | Notes |
|---|---|---|
| I. PIO-First LED Drive | PASS | Output remains DMA-fed to PIO; only pixel computation changes. |
| II. Timing Precision | PASS | Phase/period cadence unchanged; per-column trig precomputed; hot loop is integer-only. Hardware jitter validation remains a hardware gate. |
| III. Hardware Abstraction | PASS | Rasterizer and sampler are pure and host-testable. |
| IV. Minimal and Deterministic Memory Use | PASS | Static framebuffer + trig tables; no heap; documented delta; framebuffer is file-static (not on the main stack). |
| V. Single-Command Build and Flash | PASS | Build/flash workflow unchanged. |

**Post-design re-check**: PASS. The framebuffer and trig tables are file-static;
the renderer struct loses the per-column mask/color arrays; the sampling loop is
bounded by `min(active_led_count, frame_len)` and validated framebuffer indices.

## Project Structure

### Documentation (this feature)

```text
specs/018-round-display-rendering/
|-- spec.md
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- round-rendering-contract.md
|-- checklists/
|   `-- requirements.md
|-- validation.md
`-- tasks.md
```

### Source Code (repository root)

```text
pov_clock_renderer.h       # drop column mask/color arrays; keep phase state + text
pov_clock_renderer.cpp     # Cartesian rasterizer + fixed-point polar sampler
tests/pov_adaptive_rendering_test.cpp  # tests for bounds, disc masking, colors
tools/pov_preview.py       # add 40-column polar reconstruction preview
```

**Structure Decision**: Keep the change inside the pure renderer behind the
existing `render_current(renderer, frame, frame_len, active_count)` signature so
`pov_leds.cpp` and the timing pipeline are untouched. The framebuffer and trig
tables are file-static singletons (one renderer in this app), honoring the
static-allocation rule without growing the main stack.

## Complexity Tracking

> No constitution violations requiring justification.
