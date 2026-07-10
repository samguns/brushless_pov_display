# Implementation Plan: Color HHMMSS Clock

**Branch**: `010-color-hhmmss-clock` | **Date**: 2026-07-10 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/010-color-hhmmss-clock/spec.md`

## Summary

Refine the STA POV clock display from `HH:MM:SS CST` to a simpler `HH:MM:SS`
layout and add per-component color coding: hours red, minutes green, seconds
blue, and colon separators white. The implementation keeps the existing time
calibration, rotation suitability, 48-column column scheduler, and DMA -> TX FIFO
-> PIO output path. The renderer will store a color per angular column so each
digit group can be emitted with its assigned color without changing the hardware
driver.

## Technical Context

**Language/Version**: C11 / C++17 (existing project standard)

**Primary Dependencies**: Pico SDK 2.2.0; existing `pov_clock`,
`pov_clock_renderer`, `time_sync`, `hall_sensor`, and `ws2812_driver` modules;
Pillow only for local JPEG preview generation

**Storage**: N/A for persistent storage; renderer state remains bounded static or
long-lived stack state

**Testing**: Build verification with `ninja -C build`; JPEG preview inspection;
manual hardware validation for color readability when available

**Target Platform**: Raspberry Pi Pico W / Pimoroni Pico Plus 2 W running the
existing firmware super-loop

**Project Type**: Single embedded firmware target

**Performance Goals**: Preserve once-per-second time updates, measured-period
column scheduling, and 48 columns per revolution; improve readability by
removing four timezone characters and using distinct component colors

**Constraints**: No heap allocation in the render path; no CPU bit-banging;
preserve the existing UTC+8 calibration and invalid-state behavior; static RAM
delta must remain well under the 2 KB display-feature budget

**Scale/Scope**: One built-in time format (`HH:MM:SS`), one color map, one JPEG
preview artifact, no configuration UI changes

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | PASS | Only prepared pixel content changes; LED signaling remains DMA -> TX FIFO -> PIO. |
| II. Timing Precision | PASS | The existing measured-period column scheduler and once-per-second update behavior are preserved. |
| III. Hardware Abstraction | PASS | Text/color layout stays in `pov_clock_renderer`; time and hardware drivers remain separate. |
| IV. Minimal & Deterministic Memory | PASS | Adds only a bounded per-column color array; no heap allocation in display paths. |
| V. Single-Command Build & Flash | PASS | Build remains `ninja -C build`; no new firmware dependencies. |

**Post-design re-check**: PASS. Research, data model, contract, quickstart, and
task artifacts preserve all constitution gates.

## Project Structure

### Documentation (this feature)

```text
specs/010-color-hhmmss-clock/
|-- spec.md
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- color-hhmmss-contract.md
|-- checklists/
|   `-- requirements.md
|-- tasks.md
`-- color_hhmmss_preview.jpg
```

### Source Code (repository root)

```text
pov_clock.h             # update clock text length for HH:MM:SS
pov_clock.cpp           # format UTC+8 clock text without timezone label
pov_clock_renderer.h    # add per-column color state and render API
pov_clock_renderer.cpp  # build compact HH:MM:SS columns with component colors
pov_leds.cpp            # submit renderer frames without a single global clock color

time_sync.*             # existing time calibration, unchanged
hall_sensor.*           # existing rotation measurement, unchanged
ws2812_driver.*         # existing PIO/DMA transport, unchanged
```

**Structure Decision**: Modify the existing 009 clock renderer in place because
this is a visual-format refinement, not a new driver or subsystem. The preview
artifact lives with the 010 feature docs.

## Complexity Tracking

No constitution violations requiring justification.

## Static RAM Notes

The feature adds one `uint32_t column_colors[48]` array to the renderer state,
approximately 192 bytes. The previous renderer state remains bounded and the
feature stays below the 2 KB display-feature budget.

Build size after implementation:

```text
text=382400 data=0 bss=65212 dec=447612 file=build/pov_leds.elf
```

The reported BSS is unchanged from the previous build because renderer state is
held in long-lived stack objects and existing static frame storage is reused.
