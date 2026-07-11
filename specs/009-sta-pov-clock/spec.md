# Feature Specification: STA POV Clock Display

**Feature Branch**: `009-sta-pov-clock`

**Created**: 2026-07-10

**Status**: Draft

**Input**: User description: "When we are running in STA mode, I'd like to do the following: 1. Calibrate time and then offset to UTC+8. 2. The 57 leds are placed in a row on a round PCB, the PCB is spinning around its center. Recommend a speady speed that makes POV works (I'll make a motor spins in the speed and you could read it via hall sensor). 3. With the 57 leds are spinning, I'd like you to display current time in CST format using POV. The display changes every second which reflect a wall timer."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Calibrate CST Time in STA Mode (Priority: P1)

As the device operator, I want the board to calibrate the current time while it is running in STA mode and convert that time to UTC+8, so that the displayed clock is based on real wall time rather than manual setup.

**Why this priority**: A POV clock is only useful if the time source is valid. Display rendering and motor timing depend on having a calibrated time value to show.

**Independent Test**: Connect the device in STA mode, allow time calibration to complete, and verify that the reported local time matches a trusted UTC+8 reference within tolerance.

**Acceptance Scenarios**:

1. **Given** the device is running in STA mode with network access, **When** time calibration is requested or performed at startup, **Then** the device obtains a valid current time and converts it to UTC+8.
2. **Given** the device has calibrated time, **When** one second elapses, **Then** the local CST wall-clock value advances by exactly one second.
3. **Given** time calibration has not completed, **When** the clock display would otherwise start, **Then** the device presents a clear not-calibrated state instead of displaying an incorrect time.

---

### User Story 2 - Establish a Stable POV Spin Target (Priority: P1)

As the hardware builder, I want a recommended steady motor speed for the round PCB so that the 57-LED radial row can create a readable persistence-of-vision display.

**Why this priority**: POV readability depends on a stable rotation rate. The motor target and Hall-sensor measurement must be agreed before clock rendering can be validated.

**Independent Test**: Spin the PCB near the recommended target speed, read the Hall-sensor speed, and confirm that the device classifies the speed as suitable for POV rendering.

**Acceptance Scenarios**:

1. **Given** the PCB is spinning at the nominal target of 600 RPM, **When** the speed is measured, **Then** the device marks the rotation as suitable for POV clock display.
2. **Given** the measured speed is within the supported operating range of 480 RPM to 800 RPM, **When** POV rendering is active, **Then** the display remains readable and synchronized to the measured rotation period.
3. **Given** the measured speed is outside the supported operating range, **When** POV rendering is active, **Then** the device avoids presenting a misleading clock image and indicates that the spin speed needs adjustment.
4. **Given** the motor speed varies, **When** the speed changes gradually within the supported range, **Then** the display timing follows the measured rotation period without requiring a restart.

---

### User Story 3 - Display Current Time with POV (Priority: P1)

As a viewer, I want the spinning 57-LED row to display the current CST time using POV, updating every second like a wall timer, so that the display functions as a readable clock.

**Why this priority**: This is the primary visible outcome of the feature: a rotating LED clock that shows current time.

**Independent Test**: With calibrated time and stable rotation, observe the spinning display for at least one minute and confirm that it shows the current `HH:MM:SS CST` value and changes once per second.

**Acceptance Scenarios**:

1. **Given** time is calibrated and rotation is suitable, **When** the display starts, **Then** the visible POV image shows the current CST time in 24-hour `HH:MM:SS CST` format.
2. **Given** the display is running, **When** the wall-clock second changes, **Then** the displayed seconds value changes within one display-refresh interval.
3. **Given** the display runs across a minute boundary, **When** seconds roll from `59` to `00`, **Then** the minutes value increments correctly.
4. **Given** the display runs across an hour boundary, **When** time rolls from `23:59:59` to `00:00:00`, **Then** the hour value wraps correctly for UTC+8 local time.

---

### User Story 4 - Recover from Time or Rotation Problems (Priority: P2)

As the device operator, I want the clock to degrade safely when time calibration fails, rotation stops, or speed is unstable, so that the device does not show a confidently wrong time.

**Why this priority**: Error handling protects trust in the clock display, but it depends on the core time and rotation paths.

**Independent Test**: Exercise no network time, stopped rotation, slow rotation, fast rotation, and speed jitter, then confirm the display transitions to bounded status behavior without hanging.

**Acceptance Scenarios**:

1. **Given** STA mode is active but time calibration fails, **When** the display state is evaluated, **Then** the device reports time unavailable and does not show a stale or fabricated clock value.
2. **Given** rotation was suitable and then stops, **When** the Hall sensor reports no fresh rotation, **Then** POV clock rendering stops or switches to a non-clock status state.
3. **Given** rotation speed is unstable beyond the supported tolerance, **When** rendering is active, **Then** the display indicates that speed is unstable rather than showing distorted time.

### Edge Cases

- STA mode is active but network time cannot be reached during startup.
- Time calibration succeeds, then network connectivity is lost.
- The local time crosses a second, minute, hour, or day boundary while the display is active.
- The measured rotation period changes between consecutive revolutions.
- The Hall sensor reading becomes stale while the motor is still coasting.
- The PCB spins below the minimum usable speed or above the maximum usable speed.
- The motor reaches the target speed before time calibration completes.
- Time calibration completes while the motor is already spinning.
- The display starts near `23:59:59 CST` and wraps to the next local day.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST perform time calibration while operating in STA mode before showing a normal clock display.
- **FR-002**: The system MUST convert calibrated time to China Standard Time, defined for this feature as UTC+8 with no daylight-saving adjustment.
- **FR-003**: The system MUST maintain a local wall-clock value after calibration and advance it continuously between calibration events.
- **FR-004**: The system MUST show or expose a not-calibrated status when current time is unavailable.
- **FR-005**: The system MUST use the Hall-sensor rotation measurement as the source of current PCB spin speed and rotation period.
- **FR-006**: The system MUST define 600 RPM as the nominal recommended motor target for the POV clock.
- **FR-007**: The system MUST define 480 RPM to 800 RPM as the supported operating range for initial POV clock validation.
- **FR-008**: The system MUST classify rotation as unsuitable when speed is outside the supported operating range or when the rotation measurement is stale.
- **FR-009**: The system MUST synchronize POV display timing to the measured rotation period rather than assuming a fixed motor speed.
- **FR-010**: The system MUST render the current time on the spinning 57-LED radial row in 24-hour `HH:MM:SS CST` format.
- **FR-011**: The displayed clock value MUST update once per wall-clock second while time and rotation are valid.
- **FR-012**: The system MUST avoid presenting a normal clock image when time is uncalibrated, rotation is stale, or measured speed is unsuitable.
- **FR-013**: The system MUST keep time calibration, rotation measurement, and display content selection as separable concerns so each can be validated independently.
- **FR-014**: The system MUST preserve bounded memory behavior with no dynamic allocation in timing-critical display paths.
- **FR-015**: The system MUST document the measured static RAM impact of any additional clock glyph, frame, or rendering state before implementation is considered complete.
- **FR-016**: The system MUST preserve the existing single-command build and flash workflow.

### Key Entities *(include if feature involves data)*

- **Calibrated Time State**: Represents whether time has been calibrated, the current UTC time basis, the UTC+8 local time value, and freshness of the calibration.
- **CST Clock Value**: The user-visible time fields to display, including hour, minute, second, and the CST label.
- **Rotation Suitability State**: Represents measured speed, rotation period, freshness, whether the speed is within the supported range, and whether the display can render a valid POV image.
- **POV Clock Frame**: The time text and visual columns needed to show the current CST clock value during one or more revolutions.
- **Display Health State**: The current user-visible state: normal clock, time unavailable, rotation unavailable, speed too slow, speed too fast, or speed unstable.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After STA mode obtains network access, time calibration completes within 10 seconds in at least 95% of attempts on a reachable network.
- **SC-002**: After calibration, the displayed CST time is within plus or minus 1 second of a trusted UTC+8 reference during a 10-minute validation run.
- **SC-003**: At the nominal 600 RPM target, the POV clock is readable as `HH:MM:SS CST` from a normal viewing distance for at least 90% of observed seconds during a 2-minute test.
- **SC-004**: Within the supported 480 RPM to 800 RPM range, the display remains synchronized to rotation with no persistent angular drift over at least 100 consecutive revolutions.
- **SC-005**: The displayed seconds value changes exactly once for each wall-clock second during a continuous 5-minute run.
- **SC-006**: When rotation stops or the speed becomes unsuitable, the system leaves normal clock display within 2 revolutions or 1 second, whichever occurs first.
- **SC-007**: When time is unavailable, the system never displays a normal-looking clock value during validation.
- **SC-008**: The feature runs for at least 15 minutes with calibrated time and stable rotation without hangs, resets, or false normal-clock output during invalid conditions.

## Constitution Compliance

| Principle | Applicability | How Satisfied |
|-----------|---------------|---------------|
| I. PIO-First LED Drive | Applies | The feature depends on timing-critical LED output and requires the display path to remain PIO-driven rather than CPU bit-banged. |
| II. Timing Precision | Applies | POV timing must follow the measured rotation period, and column timing must meet the existing project jitter budget. |
| III. Hardware Abstraction | Applies | Time calibration, rotation measurement, and display rendering are specified as separate responsibilities with independent validation. |
| IV. Minimal and Deterministic Memory Use | Applies | The feature requires bounded static display state and measured RAM impact for any added clock assets or buffers. |
| V. Single-Command Build and Flash | Applies | The feature preserves the existing build and flash workflow with no extra required scripts. |

## Debug Output Strategy

- Development validation uses the project's existing debug-output strategy only when needed to observe time-calibration status, measured RPM, rotation suitability, display state, and once-per-second clock updates.
- Release builds keep stdio disabled unless a release note explicitly justifies enabling it.
- Required validation observations include calibration success or failure, current CST time, measured RPM, suitability state, display mode, and second-boundary transitions.

## Static RAM Budget

- The implementation MUST measure and document the static RAM added by clock glyph data, POV frame state, calibration state, and display-health state.
- The initial RAM budget target is no more than 2 KB of additional persistent static RAM for this feature unless planning identifies and justifies a larger requirement.
- No heap allocation is allowed in timekeeping, rotation synchronization, or display rendering paths.

## Assumptions

- STA mode means the board is connected as a Wi-Fi station and can reach a network time source when the network is healthy.
- `CST` means China Standard Time, UTC+8, not Central Standard Time.
- The first user-visible time format is `HH:MM:SS CST` in 24-hour format; date display is out of scope for this feature.
- The PCB has one radial row of 57 LEDs spinning around the PCB center, and the Hall sensor provides one fresh rotation-period measurement per revolution.
- The recommended nominal speed is 600 RPM because a 57-pixel WS2812 row needs enough per-column transfer time to render a compact full clock string without overrunning the LED update path.
- The supported speed range of 480 RPM to 800 RPM is an initial validation range; later planning or hardware tests may tighten the range if display readability requires it.
- Motor control is out of scope for this feature; the feature measures speed and reports suitability, while the builder provides the motor and sets its speed.
- Existing Wi-Fi configuration behavior outside STA-mode time calibration is unchanged by this feature.
