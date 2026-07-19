# Implementation Plan: Stable Hall Speed Calibration

**Branch**: `019-stable-hall-calibration` | **Date**: 2026-07-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/019-stable-hall-calibration/spec.md`

## Summary

Replace the single-revolution speed estimate with a bounded moving average of
recent revolution periods, with outlier rejection and hysteresis on the
stable/unstable decision. Smoothing is added inside `pov_clock_rotation_update`
(which already dedups by Hall sample generation): each new accepted sample updates
a fixed ring buffer; the mean drives the rendering period/RPM, while the angular
phase reference stays the real latest Hall edge. This reduces column-timing jitter
(steadier image) without lagging real speed changes or flapping at the threshold.

## Technical Context

**Language/Version**: C11 and C++17; Pico SDK 2.2.0

**Primary Dependencies**: Hall capture (`hall_sensor`), rotation eligibility
(`pov_clock`), phase-aware renderer (`pov_clock_renderer`), WS2812 driver

**Storage**: Fixed-size ring buffer of recent periods + scalar filter/hysteresis
state added to `pov_clock_rotation_t`; no heap, no persistent storage

**Testing**: Host tests with synthetic interval streams
(`tests/pov_adaptive_rendering_test.cpp`), g++; `ninja -C build`; on-blade
observation for perceived stability

**Target Platform**: Raspberry Pi Pico W (RP2040, Cortex-M0+)

**Performance Goals**: >=60% reduction in speed-estimate variance vs raw; converge
to a new supported speed within <=8 revolutions; single outlier changes estimate
<=2%; no threshold flapping

**Constraints**: Bounded window (no unbounded mean); integer-only, non-blocking,
no heap in measurement/render paths; phase anchor stays the real edge; stop
detection unaffected

**Scale/Scope**: One Hall channel, one rotation domain; change localized to
`pov_clock.{h,cpp}` and its tests

## Constitution Check

| Principle | Status | Notes |
|---|---|---|
| I. PIO-First LED Drive | PASS | LED output path unchanged; only the CPU speed estimate changes. |
| II. Timing Precision | PASS | Cadence still derives from measured rotation; smoothing reduces noise; phase stays on the real edge. Hardware jitter validation remains a hardware gate. |
| III. Hardware Abstraction | PASS | Smoothing/outlier/hysteresis are pure derivation in `pov_clock`, host-testable; ISR/GPIO capture unchanged. |
| IV. Minimal and Deterministic Memory Use | PASS | Fixed ring buffer (8 x uint32) + scalars in the rotation struct; no heap; documented delta. |
| V. Single-Command Build and Flash | PASS | Build/flash workflow unchanged. |

**Post-design re-check**: PASS. Smoothing adds a bounded ring and scalars; no new
source files, PIO, DMA, or GPIO changes.

## Project Structure

### Documentation (this feature)

```text
specs/019-stable-hall-calibration/
|-- spec.md
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   `-- speed-calibration-contract.md
|-- checklists/
|   `-- requirements.md
|-- validation.md
`-- tasks.md
```

### Source Code (repository root)

```text
pov_clock.h    # add smoothing/hysteresis constants + ring buffer fields to rotation state
pov_clock.cpp  # bounded moving average, outlier rejection, hysteresis in rotation_update
tests/pov_adaptive_rendering_test.cpp  # update stability test; add smoothing tests
```

**Structure Decision**: Add smoothing inside `pov_clock_rotation_update` rather than
a new module. It already receives one measurement per Hall generation and owns
rotation state, so the ring buffer and hysteresis live naturally there and stay
host-testable. The Hall ISR/capture and the renderer are untouched; the renderer
keeps consuming `rotation.period_us` (now smoothed) and `rotation.phase_reference_us`
(still the real edge).

## Key Parameters (planning defaults, tunable in validation)

- Window size: 8 revolutions (bounded moving average). ~sqrt(8) ≈ 2.8x std
  reduction (>=60%); converges within ~8 revolutions.
- Minimum confident samples: 3.
- Outlier reject band: raw deviates > 40% from current mean (once >= min samples)
  → rejected (covers missed magnet ~2x and bounce ~0.5x).
- Hysteresis: enter stable when latest accepted sample within 10% of mean; exit
  stable only when it deviates > 25%.

## Complexity Tracking

> No constitution violations requiring justification.
