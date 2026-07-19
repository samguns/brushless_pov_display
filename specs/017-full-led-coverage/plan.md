# Implementation Plan: Full 57-LED POV Coverage

**Branch**: `017-full-led-coverage` | **Date**: 2026-07-12 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/017-full-led-coverage/spec.md`

## Summary

Today the compact clock font (5 glyph rows scaled 6x = 30 LEDs) is rendered as a
centered band, leaving LEDs 0-12 and 43-56 permanently dark (27 of 57 LEDs
unused). This feature replaces the fixed centered-band vertical placement with an
active-count-aware mapping that distributes the fixed glyph rows across the entire
active LED span, so every driven LED can participate in the image. Column layout,
glyph shapes, colors, brightness, and Hall/phase timing are unchanged; only the
vertical LED coverage changes.

## Technical Context

**Language/Version**: C11 and C++17; Pico SDK 2.2.0

**Primary Dependencies**: Compact clock renderer (`pov_clock_renderer`), clock
state (`pov_clock`), WS2812 DMA-to-PIO driver, existing static radial frame buffer

**Storage**: Existing fixed-size renderer state and single 57-word radial frame
buffer; no new buffers, no persistent storage, no heap

**Testing**: Host unit tests with the existing synthetic harness
(`tests/pov_adaptive_rendering_test.cpp`), compiled directly with g++; firmware
build via `ninja -C build`; on-blade visual confirmation of full-height image

**Target Platform**: Raspberry Pi Pico W (RP2040)

**Project Type**: Embedded real-time POV firmware

**Performance Goals**: No change to column/phase timing; full-span fill adds only
a constant-time per-LED row lookup; WS2812 transfers never overlap

**Constraints**: 57 serial WS2812 LEDs; 40 angular columns; 5 glyph rows; render
path must remain non-blocking with no heap; rendering must never write beyond the
active LED count or frame buffer length

**Scale/Scope**: One renderer, one clock image, one active radial column frame;
change is localized to vertical row-to-LED mapping in the renderer

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. PIO-First LED Drive | PASS | WS2812 bits remain DMA-fed to PIO; only CPU-side frame-buffer fill changes. |
| II. Timing Precision | PASS | Column-strobe and phase timing are untouched; the change only remaps which LED rows a lit column fills. |
| III. Hardware Abstraction | PASS | The mapping lives in the pure renderer and stays host-testable without hardware. |
| IV. Minimal and Deterministic Memory Use | PASS | Existing static frame buffer reused; no new buffers, no heap; only fixed scalar locals. |
| V. Single-Command Build and Flash | PASS | `ninja -C build` and flash workflows unchanged. |

**Post-design re-check**: PASS. The design removes the fixed `kTextTop` /
`kGlyphScaleY` band placement in favor of an integer division mapping
`row = led * kFontRows / active_count`, bounded by `min(active_count, frame_len)`.
No PIO, DMA, GPIO, or build changes; no allocation added.

## Project Structure

### Documentation (this feature)

```text
specs/017-full-led-coverage/
|-- spec.md
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- full-led-coverage-contract.md
|-- checklists/
|   `-- requirements.md
|-- validation.md
`-- tasks.md
```

### Source Code (repository root)

```text
pov_clock_renderer.cpp     # replace centered-band vertical placement with
                           # full-span row-to-LED mapping in render_current;
                           # keep render_status full-span (already is)
pov_clock_renderer.h       # (no API change expected; mapping is internal)
tests/pov_adaptive_rendering_test.cpp  # add full-coverage mapping assertions
```

**Structure Decision**: Keep the change inside the pure renderer. The public
renderer API (`pov_clock_renderer_render_current`) already receives
`active_led_count` and `frame_len`; the vertical mapping becomes a function of
those inputs instead of the compile-time `kTextTop`/`kGlyphScaleY` constants. This
preserves the existing hardware-abstraction boundary and host testability.

## Complexity Tracking

> No constitution violations. No entries required.
