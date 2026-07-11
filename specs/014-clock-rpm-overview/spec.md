# Feature Specification: Clock and RPM Overview

**Feature Branch**: `master`

**Created**: 2026-07-11

**Status**: Draft

**Input**: User description: "There's no USB connection when PCB is rotating. Implement statistics of clock data and measured Hall-sensor speed in the web UI so refreshing Overview shows the current clock and RPM."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Check Clock and Speed Remotely (Priority: P1)

As an operator who cannot use USB while the PCB is rotating, I want Overview to
show the device's current clock and measured rotational speed whenever I refresh
the page, so I can verify both values over Wi-Fi.

**Why this priority**: This replaces otherwise unavailable USB diagnostics with
the two runtime values needed to assess the rotating clock.

**Independent Test**: Establish calibrated time and valid Hall rotation, refresh
Overview, and confirm the displayed CST clock and whole-number RPM match the
device's current runtime measurements.

**Acceptance Scenarios**:

1. **Given** network time is calibrated and Hall rotation is valid, **When** the
   operator refreshes Overview, **Then** the page shows the current clock as
   `HH:MM:SS CST` and the measured speed as a whole-number `<value> RPM`.
2. **Given** time is not yet calibrated, **When** the operator refreshes
   Overview, **Then** the clock metric shows `--:--:-- CST` without presenting
   an unverified time.
3. **Given** no valid Hall measurement has been established, **When** the
   operator refreshes Overview, **Then** the speed metric shows `-- RPM`.
4. **Given** rotation was previously measured and has stopped, **When** the
   operator refreshes Overview after the stop timeout, **Then** the speed metric
   shows `0 RPM`.

### Edge Cases

- A request arriving near a one-second boundary may show either adjacent second;
  it must not be more than one second behind the current device clock snapshot.
- Loss of network connectivity after successful calibration does not invalidate
  the locally advancing clock during the current boot.
- Before calibration, the clock placeholder must remain visually distinguishable
  from midnight (`00:00:00 CST`).
- Fractional Hall speed is rounded to the nearest whole RPM.
- The page remains usable when either clock or RPM is unavailable; each metric
  reports its state independently.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Overview MUST include a metric labelled `Current Clock`.
- **FR-002**: Overview MUST retain the metric labelled `Rotation Speed`.
- **FR-003**: When calibrated time exists, Current Clock MUST show the latest
  device-local time in 24-hour `HH:MM:SS CST` format.
- **FR-004**: Before time calibration, Current Clock MUST show `--:--:-- CST`.
- **FR-005**: When a valid Hall measurement exists, Rotation Speed MUST show the
  nearest whole-number RPM followed by `RPM`.
- **FR-006**: Before a valid Hall measurement exists, Rotation Speed MUST show
  `-- RPM`; after previously measured rotation stops, it MUST show `0 RPM`.
- **FR-007**: Both values MUST represent runtime state available when the
  Overview request is served.
- **FR-008**: Clock and RPM availability MUST be represented independently so a
  missing value does not hide or invalidate the other metric.
- **FR-009**: Existing Overview metrics, navigation, theme behavior, and
  responsive layout MUST remain available.
- **FR-010**: The feature MUST NOT require USB connectivity, automatic browser
  polling, or changes to Hall capture, clock calibration, or LED timing.

### Key Entities

- **Clock Status**: Current device-local `HH:MM:SS` text plus whether network
  calibration has established trustworthy time during this boot.
- **Rotation Speed Status**: Latest whole-number RPM plus whether a valid Hall
  measurement has ever been established and whether rotation is stopped.
- **Overview Runtime Snapshot**: Independently rendered Clock Status and Rotation
  Speed Status captured from device state when a page request is served.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In every calibrated-time test, a refreshed Overview clock differs
  from the device's current CST clock by no more than one second.
- **SC-002**: In every valid-rotation test, refreshed Overview RPM equals the
  latest measured RPM after nearest-whole rounding.
- **SC-003**: In every uncalibrated-time test, Overview shows `--:--:-- CST` and
  never shows a fabricated clock value.
- **SC-004**: In every unavailable or stopped-rotation test, Overview shows
  `-- RPM` or `0 RPM` respectively and never retains a stale moving value.
- **SC-005**: An operator can identify both clock and RPM, including units/time
  zone, within 5 seconds of opening Overview.
- **SC-006**: The complete Overview response remains within its existing bounded
  response capacity and all pre-existing metrics remain visible.

## Constitution Compliance

| Principle | Applicability | Compliance |
|---|---|---|
| I. PIO-First LED Drive | Indirect | The feature observes runtime clock and speed without changing LED output. |
| II. Timing Precision | Applies | Existing calibrated time and Hall-derived RPM remain the sources of truth; their derivation is unchanged. |
| III. Hardware Abstraction | Applies | The web UI consumes published runtime values and does not access sensor or time-sync hardware directly. |
| IV. Minimal and Deterministic Memory Use | Applies | Only fixed-size scalar and clock-text status is added; no heap or display-path allocation is introduced. |
| V. Single-Command Build and Flash | Applies | Existing build and flash commands remain unchanged. |

## Static RAM Budget

- Clock publication may add only a fixed-size availability value and bounded
  `HH:MM:SS` text at each existing runtime-status boundary.
- Existing RPM status remains fixed-size.
- No new dynamic allocation, page buffer, frame buffer, or interrupt-path state
  is permitted; the final persistent-byte impact is recorded during validation.

## Assumptions

- `Current Clock` means the existing China Standard Time (UTC+8) clock used by
  the POV display.
- A manual browser refresh is sufficient; automatic polling is outside scope.
- The existing time-sync subsystem continues advancing calibrated time locally
  after initial network calibration.
- The existing Hall subsystem remains the source of truth for RPM, validity,
  supported range, and stop detection.
- Overview remains accessible through the existing STA Wi-Fi portal while the
  PCB rotates.

