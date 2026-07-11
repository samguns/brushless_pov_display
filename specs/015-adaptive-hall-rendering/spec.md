# Feature Specification: Adaptive Hall-Synchronized Rendering

**Feature Branch**: `master`

**Created**: 2026-07-11

**Status**: Draft

**Input**: User description: "Adjust rendering timing according to measured speed from Hall sensor. Set minimum and maximum RPM limits; any rotational speed within the limit should have a proper display."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Stable Display at Any Supported Speed (Priority: P1)

As an operator, I want POV rendering timing to follow the Hall-sensor measurement
so that the clock remains recognizable and angularly stable at any steady speed
from 480 RPM through 800 RPM.

**Why this priority**: Correct speed-derived timing is the essential condition
for displaying a stable POV image rather than circles, distortion, or drift.

**Independent Test**: Run the PCB at several reference speeds including 480,
600, approximately 764, and 800 RPM; at each speed confirm normal clock mode,
a recognizable clock image, and no persistent angular drift over 100 revolutions.

**Acceptance Scenarios**:

1. **Given** calibrated clock data and a fresh, stable Hall measurement at any
   speed from 480 through 800 RPM inclusive, **When** rendering runs, **Then**
   the normal clock image is displayed with timing derived from the measured
   revolution period.
2. **Given** rotation at 480 RPM or exactly 800 RPM, **When** the measurement is
   valid, **Then** the boundary speed is accepted and normal rendering remains
   enabled.
3. **Given** steady rotation near 764 RPM (80 rad/s), **When** time calibration
   completes, **Then** the display transitions from startup status to the normal
   clock rather than a speed-too-fast fallback.

---

### User Story 2 - Follow Supported Speed Changes (Priority: P2)

As an operator, I want the display timing to adapt when motor speed changes
within the supported range so that the clock recovers promptly without a restart.

**Why this priority**: Real motors vary under load; a display that works only at
one fixed rate is not reliable in normal operation.

**Independent Test**: Change speed between multiple values inside 480-800 RPM
and verify that rendering settles to the new measured period within two complete
revolutions without accumulating long-term angular drift.

**Acceptance Scenarios**:

1. **Given** normal rendering at one supported speed, **When** speed changes to
   another supported steady speed, **Then** rendering timing settles to the new
   period within two revolutions without requiring a reboot.
2. **Given** a rendering iteration is delayed, **When** the next iteration runs,
   **Then** the display resumes at the angular position corresponding to elapsed
   time rather than permanently shifting all later columns.

---

### User Story 3 - Safe Unsupported-Speed Behavior (Priority: P3)

As an operator, I want clear bounded behavior outside the supported range or
without a valid measurement so that distorted clock output is not mistaken for
a valid display.

**Why this priority**: Fallback behavior protects readability and diagnosis, but
normal in-range rendering is the primary value.

**Independent Test**: Exercise stopped rotation, startup before a complete Hall
period, speeds below 480 RPM, and speeds above 800 RPM; confirm normal clock
rendering is withheld and returns automatically when valid in-range rotation
resumes.

**Acceptance Scenarios**:

1. **Given** speed below 480 RPM, **When** rendering state is evaluated, **Then**
   the display reports or presents a too-slow fallback instead of a clock image.
2. **Given** speed above 800 RPM, **When** rendering state is evaluated, **Then**
   the display reports or presents a too-fast fallback instead of a clock image.
3. **Given** rotation becomes stale or unavailable, **When** the stop timeout
   elapses, **Then** normal rendering stops; after fresh in-range measurement
   returns, normal rendering resumes automatically.

### Edge Cases

- Exactly 480 RPM and exactly 800 RPM are inside the supported range.
- A single Hall event is insufficient to establish a revolution period; normal
  rendering waits for a valid period measurement.
- A delayed rendering iteration may skip expired angular positions, but must not
  replay obsolete positions or shift the long-term schedule.
- Fractional measured periods must not cause cumulative truncation drift over
  repeated revolutions.
- Abrupt or noisy measurements must not cause unbounded timing values, hangs, or
  writes that overlap an unfinished LED transfer.
- Loss of calibrated time remains independent from rotation suitability; valid
  speed alone does not authorize showing an unverified clock.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST define 480 RPM as the inclusive minimum and 800 RPM
  as the inclusive maximum for normal POV clock rendering.
- **FR-002**: The system MUST use the latest fresh Hall-measured revolution period
  as the source for rendering cadence whenever rotation is supported.
- **FR-003**: The system MUST distribute the clock image's angular samples across
  each measured revolution rather than assuming a fixed motor speed.
- **FR-004**: The system MUST preserve a continuous timing schedule so delayed
  processing does not accumulate permanent angular drift.
- **FR-005**: When more than one angular position has elapsed, the system MUST
  resume at the position corresponding to current rotational timing rather than
  replay every missed position.
- **FR-006**: After speed settles to a different value within 480-800 RPM, normal
  rendering MUST settle to the new measured period within two revolutions.
- **FR-007**: Every fresh, stable speed within the inclusive range MUST be eligible
  for normal rendering; no single nominal RPM may be required.
- **FR-008**: Speeds below 480 RPM, above 800 RPM, or without a fresh valid Hall
  period MUST suppress normal clock rendering and select the corresponding
  bounded fallback state.
- **FR-009**: Normal rendering MUST resume automatically when calibrated time and
  fresh supported rotation are both restored.
- **FR-010**: Rendering MUST NOT overwrite or start a new LED-frame transfer while
  the preceding transfer is unfinished.
- **FR-011**: Timing adaptation MUST remain non-blocking and MUST NOT add dynamic
  allocation to measurement, interrupt, or display paths.
- **FR-012**: The feature MUST preserve existing clock content, colors, brightness,
  Hall measurement semantics, Wi-Fi management behavior, and fallback meanings.

### Key Entities

- **Supported Rotation Envelope**: Inclusive minimum and maximum speeds that can
  sustain a readable clock within the display's physical update budget.
- **Rotation Timing Sample**: A fresh Hall-derived revolution period, associated
  speed, validity, and freshness used to determine rendering cadence.
- **Angular Rendering Schedule**: The current image position, per-position timing,
  schedule reference, and recovery behavior when one or more positions expire.
- **Display Eligibility State**: Combined time-calibration and rotation state that
  selects normal clock rendering or a bounded fallback.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: At 480, 600, approximately 764, and 800 RPM, normal clock rendering
  is selected for every fresh stable Hall measurement after time calibration.
- **SC-002**: At every tested steady speed within 480-800 RPM, the clock remains
  recognizable with no persistent angular drift over 100 consecutive revolutions.
- **SC-003**: Following a supported speed change, the displayed image settles to
  the new rotation rate within two revolutions.
- **SC-004**: A delayed rendering iteration does not shift subsequent scheduled
  positions; after the delay, timing remains aligned to the continuous schedule.
- **SC-005**: At speeds below 480 RPM or above 800 RPM, zero normal clock frames
  are intentionally presented as valid output.
- **SC-006**: After stale or unsupported rotation returns to a fresh supported
  value, normal clock rendering resumes within two revolutions or one second,
  whichever occurs first.
- **SC-007**: A 15-minute run at a stable in-range speed completes without hangs,
  resets, overlapping transfers, or a false unsupported-speed transition.

## Constitution Compliance

| Principle | Applicability | Compliance |
|---|---|---|
| I. PIO-First LED Drive | Applies | LED bit output remains in the existing DMA-to-PIO path; CPU logic only selects prepared angular frames. |
| II. Timing Precision | Applies | Every angular interval derives from the measured revolution period, with the project jitter budget and continuous schedule preserved. |
| III. Hardware Abstraction | Applies | Hall capture, rotation eligibility, rendering schedule, and LED transport remain separable responsibilities. |
| IV. Minimal and Deterministic Memory Use | Applies | Timing state and frame storage remain fixed-size; no heap allocation is allowed in measurement or display paths. |
| V. Single-Command Build and Flash | Applies | Existing build and flash workflows remain unchanged. |

## Static RAM Budget

- Existing angular frame storage may be resized only to the supported rendering
  resolution selected during planning; no second full polar frame is permitted.
- Any added schedule or Hall phase-reference state must use fixed-size scalar
  fields and be included in the final measured static-RAM delta.
- No heap allocation, per-revolution allocation, or interrupt-time allocation is
  permitted.

## Assumptions

- The initial supported envelope is 480-800 RPM inclusive. Hardware validation
  may narrow it later but must not silently expand it beyond the proven LED
  transfer budget.
- One Hall reference event is produced per revolution and the existing Hall
  measurement correctly reports period, freshness, and RPM.
- Rotation direction is fixed for a device installation; automatic direction
  detection and motor control are outside scope.
- The compact clock image fits within the angular resolution chosen during
  planning for the complete supported range.
- A brief fallback before time calibration or before the first valid Hall period
  is expected and is not considered a rendering failure.
- Debug-output configuration remains unchanged; browser status and hardware
  observation provide validation without requiring USB during rotation.

