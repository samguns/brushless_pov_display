# Feature Specification: Rotation Speed Overview

**Feature Branch**: `master`

**Created**: 2026-07-11

**Status**: Draft

**Input**: User description: "Display rotational speed in 'Overview' of web UI."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - View Current Rotation Speed (Priority: P1)

As an operator viewing the device web interface, I want to see the current
rotation speed on the Overview screen so that I can confirm the display is
spinning at the expected rate without using serial logs or a separate tool.

**Why this priority**: Showing the measured speed is the complete user value of
this feature and makes an existing device measurement directly observable.

**Independent Test**: Open Overview while the plate spins at a known rate and
confirm that a clearly labelled RPM value appears and agrees with the device's
latest valid rotation measurement.

**Acceptance Scenarios**:

1. **Given** the device has a valid rotation measurement, **When** an operator
   opens Overview, **Then** the page shows the latest speed as a whole-number
   value labelled in RPM.
2. **Given** the plate is stopped and the rotation measurement is stale, **When**
   an operator opens Overview, **Then** the page shows `0 RPM`.
3. **Given** no valid rotation measurement has yet been established, **When** an
   operator opens Overview, **Then** the page shows an unavailable placeholder
   rather than presenting an unverified numeric speed.

### Edge Cases

- Before two sensor events establish a measurement, the Overview must not imply
  that a valid speed is known.
- When a previously valid measurement becomes stale after rotation stops, the
  displayed value must be zero rather than the last moving speed.
- Fractional RPM values are rounded to the nearest whole RPM for a compact,
  stable dashboard reading.
- Values already bounded by the rotation measurement subsystem are displayed as
  provided; this feature does not introduce a second range policy.
- If the Hall sensor is unavailable, the page remains usable and shows the speed
  as unavailable.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The Overview screen MUST include a metric labelled `Rotation Speed`.
- **FR-002**: When a fresh, valid rotation measurement exists, the metric MUST
  show the latest speed in revolutions per minute.
- **FR-003**: A valid speed MUST be displayed as a whole number followed by the
  unit `RPM`.
- **FR-004**: When rotation has stopped and the measurement is stale, the metric
  MUST show `0 RPM`.
- **FR-005**: Before a valid measurement exists, or when rotation sensing is
  unavailable, the metric MUST show an unavailable placeholder and MUST NOT
  present an unverified numeric speed.
- **FR-006**: The displayed measurement MUST represent the device runtime state
  available when the Overview request is served.
- **FR-007**: Adding the metric MUST preserve all existing Overview metrics,
  navigation, theme behavior, and responsive layout.
- **FR-008**: The feature MUST reuse the existing rotation measurement and MUST
  NOT change sensor capture, rotation derivation, or LED-output timing behavior.

### Key Entities

- **Rotation Speed Status**: The latest operator-facing rotation state, consisting
  of speed in RPM and whether that value is valid, stopped/stale, or unavailable.
- **Overview Metric**: A labelled dashboard value rendered from the current
  Rotation Speed Status when the page is requested.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In every tested valid-rotation case, Overview shows the same RPM as
  the latest device measurement after whole-number rounding.
- **SC-002**: In every tested stopped-rotation case, Overview shows `0 RPM` and
  never retains the prior moving speed after the existing stop timeout elapses.
- **SC-003**: In every tested startup or sensor-unavailable case, Overview shows
  an unavailable state and no misleading numeric speed.
- **SC-004**: An operator can locate and interpret the rotation-speed reading,
  including its unit, within 5 seconds of opening Overview.
- **SC-005**: All existing Overview metrics and navigation remain present and
  usable on both desktop and narrow-screen layouts.

## Constitution Compliance

| Principle | Applicability | Compliance |
|---|---|---|
| I. PIO-First LED Drive | Indirect | The feature only exposes an existing measurement in the web UI and does not modify LED output. |
| II. Timing Precision | Indirect | The existing measured rotation speed is consumed without changing its timebase or derivation. |
| III. Hardware Abstraction | Applies | The UI consumes a runtime status value and remains separate from Hall GPIO and interrupt handling. |
| IV. Minimal and Deterministic Memory Use | Applies | Only fixed-size scalar status is added; no heap allocation or display-path buffer is introduced. |
| V. Single-Command Build and Flash | Applies | The existing build and flash workflow remains unchanged. |

## Static RAM Budget

- The feature may add only fixed-size scalar runtime status needed to carry RPM
  and validity to the web layer.
- No frame buffer, dynamic allocation, or interrupt-path allocation is added.
- Exact added persistent bytes will be measured or derived during implementation
  and recorded in validation artifacts.

## Assumptions

- Operators use RPM as the expected unit for rotational speed.
- Overview values represent runtime state at page-request time; automatic browser
  polling or live animation is outside this feature's scope.
- The existing Hall-sensor subsystem remains the source of truth for speed,
  freshness, supported range, and stop detection.
- Existing authentication and access behavior for Overview is unchanged.
- Debug output configuration is unchanged because this feature adds no new
  logging requirement.

