# Feature Specification: Reboot Controls

**Feature Branch**: `master`

**Created**: 2026-07-12

**Status**: Draft

**Input**: User description: "Remove the useless blink frequency and add a reboot button that lets a user reboot the board manually."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Reboot the Board Remotely (Priority: P1)

As a device owner using the Wi-Fi management interface, I want a manual reboot
action so I can recover or restart the board without physical access or a USB
connection.

**Why this priority**: Manual restart is the new user-facing capability and is
especially valuable while the rotating board cannot be reached safely.

**Independent Test**: Open the management interface, choose Reboot, confirm the
action, and verify the board restarts normally while retaining its settings.

**Acceptance Scenarios**:

1. **Given** the owner is viewing the System section, **When** they inspect the
   available actions, **Then** a clearly labelled `Reboot` action explains that
   it restarts the board in normal operating mode.
2. **Given** the owner selects Reboot, **When** they have not yet confirmed,
   **Then** the interface warns about the temporary interruption and allows the
   owner to cancel without restarting the board.
3. **Given** the owner confirms Reboot, **When** the request is accepted, **Then**
   the interface acknowledges the restart before the board becomes unavailable.
4. **Given** a confirmed reboot, **When** the board starts again, **Then** it
   returns to normal operation with its saved Wi-Fi and display settings intact.

---

### User Story 2 - See Only Useful Overview Status (Priority: P2)

As a device owner, I want the obsolete blink-frequency status removed so that
Overview contains only information that helps me operate or diagnose the board.

**Why this priority**: Removing unused status reduces confusion, but it does not
add the recovery capability provided by manual reboot.

**Independent Test**: Open and refresh Overview and verify no blink-frequency
metric, label, value, or placeholder appears while all other status remains.

**Acceptance Scenarios**:

1. **Given** the owner opens Overview, **When** the page is rendered, **Then** no
   blink-frequency status is displayed.
2. **Given** the board changes display or health state, **When** the owner
   refreshes Overview, **Then** blink frequency remains absent and the other
   existing metrics continue to report their current values.

### Edge Cases

- The owner cancels the reboot confirmation; the board continues operating.
- The owner presses Reboot or confirms repeatedly; at most one reboot sequence
  is initiated.
- The browser disconnects after confirmation; the accepted reboot still
  completes without requiring the browser to remain connected.
- A firmware upload or validation is active; manual reboot is unavailable until
  the update operation reaches a safe terminal state.
- The board takes longer than usual to reconnect; the page does not claim that
  configuration was erased or reboot failed solely because reconnection is slow.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The management interface MUST provide a clearly labelled `Reboot`
  action in the System section.
- **FR-002**: The Reboot action MUST explain that it restarts the board in normal
  operating mode and temporarily interrupts display and network availability.
- **FR-003**: The system MUST require explicit user confirmation before accepting
  a manual reboot request.
- **FR-004**: Cancelling confirmation MUST leave the board running and MUST cause
  no configuration or runtime-state change.
- **FR-005**: After confirmation, the system MUST provide a visible acceptance or
  restarting message before intentionally ending the current connection.
- **FR-006**: One confirmed user action MUST initiate at most one reboot sequence,
  including when requests or button presses are repeated.
- **FR-007**: A manual reboot MUST return the board to normal operating mode; it
  MUST NOT enter firmware-update, USB storage, recovery, or factory-reset mode.
- **FR-008**: A manual reboot MUST preserve settings that normally survive a
  power cycle, including saved Wi-Fi credentials and display preferences.
- **FR-009**: The Reboot action MUST be unavailable while a firmware upload or
  validation operation is active, and the interface MUST explain why.
- **FR-010**: Overview MUST NOT display a blink-frequency metric, label, value,
  unit, placeholder, or equivalent blink-frequency status.
- **FR-011**: Blink-frequency status MUST NOT be included in user-facing Overview
  status data after this feature is delivered.
- **FR-012**: Removing blink-frequency status MUST NOT remove or change the
  meaning of existing network, address, clock, rotation-speed, firmware, or
  display-health information.
- **FR-013**: Existing USB and Wi-Fi firmware-update actions, responsive layout,
  navigation, and theme behavior MUST remain available.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can locate Reboot and reach its confirmation in no more than
  two interactions after opening Settings.
- **SC-002**: In 100% of confirmed-reboot trials, the board begins restarting
  within five seconds of accepting the action.
- **SC-003**: In 100% of cancelled-reboot trials, the board remains reachable and
  continues operating without a restart.
- **SC-004**: In 100% of repeated-action trials, only one reboot occurs for a
  single confirmation sequence.
- **SC-005**: After every completed manual-reboot trial, saved Wi-Fi credentials
  and display preferences match their pre-reboot values.
- **SC-006**: Across Overview page, refresh, and status-response tests, zero
  user-visible blink-frequency labels or values remain.
- **SC-007**: All pre-existing non-blink Overview metrics and both firmware-update
  actions remain usable in regression testing.

## Constitution Compliance

| Principle | Applicability | Compliance |
|---|---|---|
| I. PIO-First LED Drive | Indirect | Management controls and status change; normal LED transport remains unchanged. |
| II. Timing Precision | Indirect | Reboot handling and status cleanup add no work to the timing-critical display path. |
| III. Hardware Abstraction | Applies | The presentation requests a system operation without coupling to display hardware. |
| IV. Minimal and Deterministic Memory Use | Applies | Only bounded control state is allowed and obsolete status is removed; no heap or frame-buffer growth. |
| V. Single-Command Build and Flash | Applies | Existing build and flash workflows remain unchanged. |

## Static RAM Budget

- Reboot coordination may add only fixed-size scalar state needed to accept,
  acknowledge, and de-duplicate a request.
- Removing blink-frequency publication eliminates or reuses its fixed-size state
  rather than replacing it with another persistent metric.
- No dynamic allocation, frame-buffer growth, or interrupt-path storage is
  permitted; final persistent-byte impact is recorded during validation.

## Assumptions

- The intended action is a normal software reboot, not USB boot mode, firmware
  update mode, factory reset, or settings reset.
- The existing local management access model also governs Reboot; adding a new
  authentication or authorization system is outside this feature.
- `Blink frequency` means the current user-visible Overview metric and its
  published status data. Removal does not alter normal POV display output.
- The user reconnects by reopening or refreshing the management address after
  the board completes its normal startup sequence.
