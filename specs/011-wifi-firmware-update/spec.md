# Feature Specification: WiFi Firmware Update

**Feature Branch**: `011-wifi-firmware-update`

**Created**: 2026-07-11

**Status**: Draft

**Input**: User description: "I'd like to update firmware via WIFI. Current implementation can only update through USB wich is inconvinient"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Upload Firmware from the Web UI (Priority: P1)

As the device owner, while the device is connected to my WiFi network, I want to
open the device web UI and upload a firmware package directly, so that I can
update the device without putting it into USB update mode or physically moving it
to a computer.

**Why this priority**: This is the core value requested. The existing update
flow still requires USB mass-storage mode, which is inconvenient for an installed
POV display.

**Independent Test**: From the normal device web UI, select a valid firmware
package, start the update, and confirm the device installs it and returns to
normal operation without any USB interaction.

**Acceptance Scenarios**:

1. **Given** the device is connected to WiFi and reachable through its web UI,
   **When** the owner opens the firmware update screen, **Then** they can choose a
   firmware package from their browser and start the upload.
2. **Given** a valid firmware package has been selected, **When** the owner starts
   the update, **Then** the UI shows upload progress and prevents accidental
   duplicate submissions.
3. **Given** the upload and installation complete successfully, **When** the
   device restarts, **Then** it returns to normal operation on the same configured
   WiFi network.

---

### User Story 2 - Reject Invalid or Incompatible Firmware (Priority: P1)

As the device owner, if I accidentally choose the wrong file or a corrupted
firmware package, I want the device to reject it before installing anything, so
that a mistake does not make the device unusable.

**Why this priority**: A wireless update path is only acceptable if common user
mistakes are caught before the running firmware is changed.

**Independent Test**: Attempt to upload a non-firmware file, a truncated package,
and a package for a different board family; each attempt is rejected with a
clear message and the device continues running the existing firmware.

**Acceptance Scenarios**:

1. **Given** the owner selects a file that is not a firmware package, **When**
   they start the update, **Then** the device rejects the file and continues
   normal operation.
2. **Given** the owner uploads a corrupted or incomplete firmware package,
   **When** validation runs, **Then** the device rejects the package and reports a
   clear failure reason.
3. **Given** the owner uploads firmware for an incompatible board or device
   family, **When** validation runs, **Then** the device rejects the package
   before installation begins.

---

### User Story 3 - Recover Safely from Interrupted Updates (Priority: P2)

As the device owner, if the browser disconnects, WiFi drops, or power is
interrupted during an update, I want the device to remain recoverable, so that
the update process does not strand the hardware.

**Why this priority**: Wireless connections are less reliable than USB. The
device must handle interruption gracefully before this feature can be trusted.

**Independent Test**: Interrupt the upload at several points and confirm the
device either keeps running the previous firmware or enters a clearly recoverable
state with instructions for the owner.

**Acceptance Scenarios**:

1. **Given** an upload is in progress, **When** the browser disconnects before
   the full package is received, **Then** the partial package is discarded and
   the current firmware remains active.
2. **Given** WiFi drops during upload, **When** the connection cannot recover,
   **Then** the update is cancelled without changing the running firmware.
3. **Given** installation has started and cannot complete normally, **When** the
   device restarts, **Then** it boots either the previous working firmware or a
   documented recovery mode that allows the owner to install firmware.

---

### User Story 4 - Understand Update Status and Outcome (Priority: P3)

As the device owner, I want clear update status before, during, and after the
firmware update, so that I know whether it is safe to wait, retry, or recover.

**Why this priority**: Clear feedback reduces support burden and helps owners
distinguish a long-running update from a failed one.

**Independent Test**: Run successful and failed update attempts and verify the UI
shows the current stage, final result, and next action.

**Acceptance Scenarios**:

1. **Given** an update is uploading, **When** progress changes, **Then** the UI
   shows the current update stage in plain language.
2. **Given** an update succeeds, **When** the device restarts, **Then** the owner
   can confirm the new firmware version from the device UI.
3. **Given** an update fails, **When** the UI reports the failure, **Then** it
   explains whether the owner should retry via WiFi or use USB recovery.

### Edge Cases

- The owner selects a file that is too large for the device to accept safely.
- The owner uploads a valid firmware package intended for a different board
  family or hardware revision.
- The browser disconnects before upload completion.
- WiFi drops during upload or while the device is preparing to restart.
- The owner refreshes or submits the update form twice during an active update.
- The device loses power during upload, validation, or installation.
- Existing WiFi credentials or display settings are present and must survive the
  firmware update.
- The device is in AP/setup mode rather than normal WiFi client mode.
- A previous failed update left temporary update data behind.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST provide a firmware update screen reachable from the
  normal management UI while connected to a WiFi network.
- **FR-002**: The owner MUST be able to select a local firmware package in a
  browser and upload it to the device without using USB update mode.
- **FR-003**: The update flow MUST require the same level of owner authorization
  as other mutating management actions before accepting a firmware upload.
- **FR-004**: The UI MUST show a clear warning that firmware updates restart the
  device and can temporarily interrupt display and network availability.
- **FR-005**: The system MUST show upload, validation, installation, restart, and
  final-result status in plain language.
- **FR-006**: The system MUST reject files that are missing, empty, larger than
  the supported update size, not firmware packages, corrupted, or intended for an
  incompatible board/device family.
- **FR-007**: The system MUST validate the complete firmware package before any
  installation step changes the running firmware.
- **FR-008**: If upload or validation fails, the system MUST keep the current
  firmware running and provide a clear failure message.
- **FR-009**: If the upload connection is interrupted before the complete package
  is received, the system MUST discard the partial package and keep the current
  firmware running.
- **FR-010**: The system MUST prevent concurrent firmware update attempts and
  duplicate submissions while an update is active.
- **FR-011**: On successful installation, the device MUST restart automatically
  and return to normal operation using the existing WiFi credentials and display
  settings.
- **FR-012**: After restart, the management UI MUST show the current firmware
  version or update identifier so the owner can verify the result.
- **FR-013**: If a failure occurs after installation has begun, the device MUST
  remain recoverable through either the previous working firmware or a clearly
  documented recovery/update mode.
- **FR-014**: The existing USB update mode MUST remain available as a fallback
  recovery path.
- **FR-015**: Temporary update data MUST NOT permanently reduce available
  storage or corrupt stored credentials/settings after success, failure, restart,
  or power loss.
- **FR-016**: The update flow MUST be bounded so normal web management and POV
  display operation are not blocked indefinitely by a stalled upload.

### Key Entities *(include if feature involves data)*

- **Firmware Package**: The uploaded firmware file selected by the owner, with
  identity, size, compatibility metadata, and integrity status.
- **Update Session**: A single in-progress firmware update attempt, including
  authorization state, upload progress, validation result, installation state,
  and final outcome.
- **Installed Firmware Identity**: The firmware version or identifier currently
  running on the device and shown to the owner after restart.
- **Persistent Device Settings**: Existing WiFi credentials and display
  preferences that must survive firmware updates.
- **Recovery State**: A device state that allows the owner to retry or recover
  after an interrupted or failed installation.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: An owner can complete a valid firmware update over WiFi from the
  management UI without USB interaction in under 3 minutes on a stable local
  network.
- **SC-002**: In 100% of invalid-file, wrong-board, oversized, corrupted, or
  interrupted-upload attempts, the previously running firmware remains active.
- **SC-003**: After 10 consecutive successful WiFi firmware updates, the device
  reconnects using the existing WiFi credentials every time without
  re-provisioning.
- **SC-004**: The UI reports a clear success or failure outcome for every update
  attempt, with no ambiguous "unknown" result after the device becomes reachable.
- **SC-005**: A duplicate submission during an active update is rejected or
  ignored in 100% of attempts without corrupting update state.
- **SC-006**: If an update cannot complete after installation begins, the owner
  can recover the device using documented steps without opening the firmware
  source code or requiring serial debug output.

## Constitution Compliance

| Principle | Applicability | How Satisfied |
|-----------|---------------|---------------|
| I. PIO-First LED Drive | Applies indirectly | Firmware update activity must not introduce CPU-driven LED timing paths or modify the PIO/DMA display pipeline. |
| II. Timing Precision | Applies indirectly | Normal POV timing must resume after update; any temporary update pause must be explicit and not corrupt timing configuration. |
| III. Hardware Abstraction | Applies | Firmware update, storage/recovery, web management, and display behavior remain separate responsibilities with independently testable outcomes. |
| IV. Minimal & Deterministic Memory | Applies | Update state and buffers must be bounded, and persistent storage impact must be documented before implementation. |
| V. Single-Command Build & Flash | Applies | WiFi update support must be part of the normal firmware build and must not require out-of-band build artifacts beyond the firmware package selected by the owner. |

## Assumptions

- The first WiFi update version targets devices already reachable through the
  existing management UI in normal WiFi client mode.
- USB update mode remains the fallback for recovery and for initial firmware
  installation.
- Firmware upload is initiated by the owner from a browser on the same local
  network as the device.
- Existing WiFi credentials and display settings should survive the update unless
  the owner intentionally clears them through a separate flow.
- The update flow may temporarily pause display output or restart the device, but
  it must communicate that interruption before the owner starts the update.
- AP/setup-mode firmware upload is out of scope for the first version unless a
  later clarification explicitly adds it.
