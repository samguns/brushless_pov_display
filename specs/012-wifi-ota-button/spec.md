# Feature Specification: WiFi OTA Button

**Feature Branch**: `012-wifi-ota-button`

**Created**: 2026-07-11

**Status**: Draft

**Input**: "Rename the browser-upload action to ‘Update Firmware (WIFI)’, add visible upload progress, and make the update screen more polished."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Choose an Update Method (Priority: P1)

As a device owner, I want the System section to show separate USB and WiFi update actions with unambiguous names, so that I can choose browser upload without losing the established USB recovery workflow.

**Why this priority**: The WiFi action must be discoverable and must not replace the trusted USB fallback.

**Independent Test**: Open Settings on a connected device and verify both actions are visible; USB still opens its existing confirmation flow and WiFi opens the OTA upload screen.

**Acceptance Scenarios**:

1. **Given** the owner opens Settings, **When** they view System, **Then** they see both **Update Firmware (USB)** and **Update Firmware (WIFI)** with distinct descriptions.
2. **Given** the owner selects the USB action, **When** they confirm it, **Then** existing USB boot-mode behavior is unchanged.
3. **Given** the owner selects **Update Firmware (WIFI)**, **When** the screen opens, **Then** it presents a clear, polished upload experience that explains the compatible package requirement and restart behavior.

### User Story 2 - Upload a Firmware Package (Priority: P1)

As a device owner, I want to choose a firmware package in my browser and upload it through WiFi, so that I can update an installed device without moving it to a computer by USB.

**Why this priority**: This is the requested value of the new action.

**Independent Test**: Select a valid compatible package, start the upload, and verify the device reports progress, restarts, and returns to its normal management UI.

**Acceptance Scenarios**:

1. **Given** the WiFi update screen is open, **When** the owner selects a compatible package and starts upload, **Then** the UI shows upload and validation progress, including a visual progress indicator and percentage.
2. **Given** an upload is active, **When** the owner tries to submit again, **Then** the duplicate action is prevented.
3. **Given** validation succeeds, **When** the device restarts, **Then** it reconnects with existing settings and the UI shows the installed identity.

### User Story 3 - Receive Safe Failure Guidance (Priority: P2)

As a device owner, I want a failed WiFi upload to leave the USB option available and explain what to do next, so that I can recover confidently.

**Independent Test**: Submit an invalid package and confirm the UI gives a clear failure reason, keeps the device running, and still offers USB update/recovery.

## Edge Cases

- The selected file is empty, incomplete, too large, or incompatible.
- The browser closes or WiFi drops during upload.
- The owner clicks either action repeatedly.
- A device has not yet received the one-time OTA-capable bootloader migration.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The System section MUST retain the existing USB firmware-update action.
- **FR-002**: The System section MUST add a distinct action named **Update Firmware (WIFI)** with a clear browser-upload purpose.
- **FR-003**: The WiFi update screen MUST let the owner select and submit one local compatible firmware package.
- **FR-004**: The WiFi update screen MUST warn that a successful update restarts the device and may interrupt display/network availability.
- **FR-005**: The WiFi update screen MUST show plain-language receiving, validating, restarting, success, and failure status.
- **FR-010**: While an upload is active, the WiFi update screen MUST show a visual progress bar and a numeric completion percentage based on uploaded data.
- **FR-011**: The WiFi update screen MUST use a polished, visually coherent layout with clear hierarchy for package selection, update status, warnings, and recovery guidance.
- **FR-006**: The system MUST prevent duplicate WiFi upload submissions while one is active.
- **FR-007**: Invalid or interrupted uploads MUST leave the running firmware available and preserve the USB action.
- **FR-008**: After a successful WiFi update, the management UI MUST show the installed firmware identity.
- **FR-009**: The WiFi OTA action MUST tell owners of non-OTA-capable devices to use the existing USB action for the required one-time migration.

### Key Entities *(include if feature involves data)*

- **Update Method**: The owner's selected USB or WiFi update route.
- **Upload Session**: The in-progress browser upload, its progress, status, and error guidance.
- **Firmware Identity**: The installed update identifier shown after restart.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of connected-device Settings pages visibly offer both update methods.
- **SC-002**: An owner can reach the WiFi upload screen from Settings in no more than two interactions.
- **SC-003**: During a valid upload, the UI reports a non-ambiguous status change and updated visible percentage at least once every five seconds until restart or failure.
- **SC-004**: In all invalid or interrupted-upload trials, the current management UI and USB update route remain usable.
- **SC-005**: After a successful update, the returned management UI identifies the installed firmware in every trial.

## Assumptions

- WiFi update is available only while the device is in normal connected management mode.
- The existing WiFi update engine and compatible package format are used by the new UI action.
- USB update remains the recovery route and the initial migration route for devices not yet OTA-capable.
- WiFi update uses the same open local-management access model as the existing USB update route.
