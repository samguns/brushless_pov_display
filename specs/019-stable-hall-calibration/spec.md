# Feature Specification: Stable Hall Speed Calibration

**Feature Branch**: `019-stable-hall-calibration`

**Created**: 2026-07-13

**Status**: Draft

**Input**: User description: "Refine the hall sensor reading and auto-calibrating
POV display logic. Calculate rotation speed using the mean of accumulated sensor
values rather than a single reading. The ultimate goal is a stable POV display."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Steady Image at Constant Speed (Priority: P1)

As a viewer, I want the POV image to hold still when the motor spins at a constant
speed, instead of shimmering or drifting because the measured speed jumps from one
revolution to the next.

**Why this priority**: Single-revolution timing noise directly wobbles column
spacing and is the primary cause of an unstable image. Smoothing the speed
estimate is the core of this feature.

**Independent Test**: With a constant physical speed that has small per-revolution
timing noise, confirm the speed estimate used for rendering varies far less than
the raw per-revolution measurement, and the image stays visually steady over many
revolutions.

**Acceptance Scenarios**:

1. **Given** a constant rotation speed with small per-revolution jitter, **When**
   the speed estimate is computed from accumulated samples, **Then** the estimate's
   variation is substantially smaller than the raw single-revolution variation.
2. **Given** a steady speed, **When** the display renders over 100 revolutions,
   **Then** the image shows no persistent drift and no visible per-revolution
   shimmer attributable to speed noise.

---

### User Story 2 - Prompt Response to Real Speed Changes (Priority: P2)

As an operator, I want the display to follow genuine speed changes (spin-up,
slow-down, load changes) within a small number of revolutions, so smoothing does
not make the clock sluggish or wrong after the speed settles.

**Why this priority**: Over-smoothing (e.g. an unbounded lifetime average) would
lag real changes and show a wrong image while the estimate catches up.

**Independent Test**: Step the speed from one supported value to another and
confirm the smoothed estimate converges to the new speed within a bounded number
of revolutions, then the image is stable at the new speed.

**Acceptance Scenarios**:

1. **Given** a sustained change to a new supported speed, **When** samples
   accumulate, **Then** the estimate converges to the new speed within a bounded,
   documented number of revolutions.
2. **Given** the estimate has converged, **When** rendering continues, **Then** the
   image is stable at the new speed with no lingering bias from the previous speed.

---

### User Story 3 - Reject Spurious and Missed Edges (Priority: P3)

As a maintainer, I want obviously wrong sensor events (a bounce producing a tiny
interval, or a missed magnet producing a doubled interval) to be rejected before
they affect the speed estimate, so a single glitch does not destabilize the image.

**Why this priority**: Real Hall signals occasionally produce outliers; without
rejection they corrupt any average and cause a visible jump.

**Independent Test**: Inject a too-short and a too-long interval into an otherwise
steady stream and confirm the speed estimate and rendering are essentially
unaffected.

**Acceptance Scenarios**:

1. **Given** a steady stream with one interval far shorter than expected, **When**
   the estimate updates, **Then** the outlier is rejected and the estimate is
   essentially unchanged.
2. **Given** a steady stream with one interval far longer than expected (missed
   reference), **When** the estimate updates, **Then** the outlier is rejected or
   corrected and the estimate is essentially unchanged.

---

### User Story 4 - Stable Auto-Calibration Without Flapping (Priority: P3)

As a viewer, I want the display to settle into "running normally" once speed is
steady and stay there, rather than repeatedly toggling between "normal" and an
"unstable" fallback at the edge of the stability threshold.

**Why this priority**: Threshold chatter causes the image to blink in and out of
the fallback pattern, which is worse than a slightly delayed lock.

**Independent Test**: Hold a speed near the stability threshold and confirm the
display does not oscillate between normal and unstable states.

**Acceptance Scenarios**:

1. **Given** a speed hovering near the stability threshold, **When** state is
   evaluated over time, **Then** the display uses hysteresis so it does not rapidly
   toggle between normal and unstable.
2. **Given** the display has locked to normal, **When** small noise occurs, **Then**
   it remains normal until a genuinely significant, sustained deviation.

### Edge Cases

- Fewer accumulated samples than the smoothing window (startup): the estimate must
  use whatever valid samples exist and must not report a confident speed before a
  minimum number of good samples.
- A single missed reference magnet must not be averaged in as a real halving of
  speed.
- The smoothed period must remain consistent with the actual latest edge used for
  angular phase, so smoothing the speed does not introduce angular drift.
- Transition from stopped to spinning, and spinning to stopped, must still be
  detected promptly (stop detection is not delayed by smoothing).
- Smoothing must not push a genuinely in-range speed outside the supported range or
  vice versa due to lag.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST derive the rotation speed/period used for rendering
  from multiple accumulated Hall samples rather than a single revolution interval.
- **FR-002**: The smoothing MUST be bounded in memory/time (a limited window or
  decaying weight), so the estimate continues to track real speed changes rather
  than averaging over all history indefinitely.
- **FR-003**: The system MUST reject or correct outlier intervals (implausibly
  short or long relative to the current estimate) before they affect the estimate.
- **FR-004**: The angular phase reference MUST remain the actual most-recent
  accepted edge; smoothing applies to the period/speed, not to the phase anchor.
- **FR-005**: After a sustained change to a new supported speed, the estimate MUST
  converge within a bounded, documented number of revolutions.
- **FR-006**: The suitable/unstable decision MUST use hysteresis so the display does
  not rapidly toggle between normal and fallback near the threshold.
- **FR-007**: Before a minimum number of valid samples is accumulated, the system
  MUST NOT present a confident speed; it MUST use the existing startup/fallback
  behavior.
- **FR-008**: Stop detection and stopped→spinning transitions MUST remain prompt and
  MUST NOT be delayed by the smoothing window.
- **FR-009**: The feature MUST preserve the existing supported speed range, fallback
  meanings, colors, rendering pipeline, and Wi-Fi behavior.
- **FR-010**: All smoothing/calibration state MUST be fixed-size and non-blocking,
  with no dynamic allocation in the measurement, interrupt, or display paths.

### Key Entities

- **Accumulated Sample Set**: The bounded collection of recent valid revolution
  intervals used to compute the smoothed speed/period.
- **Smoothed Speed Estimate**: The filtered rotation period/RPM used for rendering
  cadence, plus a confidence/validity indicator.
- **Outlier Policy**: The rule that classifies an incoming interval as valid,
  spurious (too short), or a missed reference (too long).
- **Calibration State**: The stable/normal vs. unstable decision with hysteresis,
  driving whether the normal clock or a fallback is shown.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For a constant speed with bounded per-revolution jitter, the standard
  deviation of the rendering speed estimate is at least 60% lower than the standard
  deviation of the raw single-revolution measurement.
- **SC-002**: The image shows no persistent angular drift over 100 consecutive
  revolutions at a steady supported speed.
- **SC-003**: After a sustained supported speed change, the estimate converges to
  within 2% of the new speed within a bounded number of revolutions (target: <= 8).
- **SC-004**: A single injected outlier interval changes the smoothed estimate by no
  more than a small bounded amount (target: <= 2%).
- **SC-005**: Near the stability threshold, the display does not toggle between
  normal and unstable more than once for a sustained near-threshold speed.
- **SC-006**: Stop detection still triggers within the existing stop-timeout after
  the last edge, unaffected by smoothing.

## Constitution Compliance

| Principle | Applicability | Compliance |
|---|---|---|
| I. PIO-First LED Drive | Applies | LED output path is unchanged; this feature only refines the CPU-side speed estimate feeding the renderer. |
| II. Timing Precision | Applies | Rendering cadence still derives from measured rotation; smoothing reduces noise while the phase anchor stays on the real edge. Hardware jitter validation remains a hardware gate. |
| III. Hardware Abstraction | Applies | Sample accumulation and outlier policy remain pure, host-testable derivation logic separate from GPIO/IRQ capture. |
| IV. Minimal and Deterministic Memory Use | Applies | Fixed-size sample window and scalar state; no heap; documented RAM delta. |
| V. Single-Command Build and Flash | Applies | Build/flash workflow unchanged. |

## Static RAM Budget

- Adds a fixed-size window/accumulator of recent intervals plus scalar filter and
  hysteresis state. The window size is bounded and documented during planning; no
  heap or per-revolution allocation is permitted.

## Assumptions

- **Smoothing method**: The exact filter (bounded moving average vs. exponential
  moving average) is a planning decision. A bounded/decaying filter is assumed over
  an unbounded lifetime mean so the estimate stays responsive (FR-002, FR-005).
- **Outlier thresholds**: Plausibility bounds are relative to the current estimate
  (e.g. an interval near half or double the expected period is treated as a missed
  or spurious edge). Exact ratios are set during planning/validation.
- **Phase vs. period separation**: Angular phase continues to come from the latest
  real edge (feature 015 behavior); only the period/speed is smoothed.
- **Supported range and fallbacks** (480-800 RPM inclusive and existing status
  meanings) are unchanged; this feature improves the estimate feeding them.
- One reference magnet per revolution, as in the current configuration.
- Host tests with synthetic interval streams are the primary automated verification;
  on-blade observation confirms perceived stability.
