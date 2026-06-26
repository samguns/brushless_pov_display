# Feature Specification: STA Management Portal & Firmware Update

**Feature Branch**: `002-sta-portal-fw-update`

**Created**: 2026-06-25

**Status**: Draft

**Input**: User description: "1. Use a separate flash partition to store WIFI credential so we won't lose them after firmware update; 2. Make a web app when device runs in STA mode. This web app has an 'update' button. When a user clicks, turn device into firmware update mode. 3. Output IP address of STA mode"

---

## Clarifications

### Session 2026-06-25

- Q: Should the STA management web page require a password to access, given that the firmware update button exposes a powerful action on the local network? → A: No authentication required. The "Update firmware" flow shows a warning confirmation page with an explicit "Confirm" button before any action is taken. No login or password is needed.
- Q: Should the "Update firmware" button trigger immediately, or show a confirmation step (e.g., countdown or confirm dialog) to prevent accidental clicks? → A (assumed): A single confirmation click or an explicit warning page is shown before rebooting; no immediate trigger on first click.
- Q: For "separate flash partition": is the goal simply to protect credentials from being erased during a UF2 firmware update (passive protection via linker layout), or is an explicit partition table / NVS layer with write-protection required? → A: Passive linker boundary only. The linker script reserves the last N KB of flash for credentials; a linker `ASSERT` fails the build if the firmware binary overflows into that region. No runtime partition manager or NVS library is needed.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — WiFi Credentials Survive Firmware Update (Priority: P1)

A developer updates the device firmware by dragging a new UF2 file onto the USB mass-storage drive. After the update and reboot, the device automatically reconnects to the previously configured WiFi network without requiring the user to go through the AP provisioning flow again.

**Why this priority**: Without this, every firmware update requires re-provisioning WiFi — a serious usability regression for devices deployed in the field.

**Independent Test**: Provision WiFi credentials, then flash a new firmware UF2 via USB MSD. After reboot, verify the device connects to the stored network without starting the AP setup flow.

**Acceptance Scenarios**:

1. **Given** valid WiFi credentials are stored, **When** a new firmware UF2 is written to the device via USB mass storage, **Then** the credentials are still present and valid after the update completes.
2. **Given** credentials are stored, **When** the device reboots after a firmware update, **Then** it connects to the WiFi network in STA mode within 20 seconds without starting AP mode.
3. **Given** no credentials were stored before the update, **When** a firmware update is performed, **Then** the device correctly starts in AP provisioning mode after the update.

---

### User Story 2 — STA Management Page with IP Address (Priority: P1)

A user who has already provisioned WiFi opens a browser and navigates to the device's IP address. They see a simple management page that shows the device's current IP address and network information.

**Why this priority**: Without a way to reach the management page, the firmware update feature (US3) is inaccessible. IP address visibility is the prerequisite for all STA-mode interactions.

**Independent Test**: Flash device with stored credentials, wait for STA connection, read IP from serial output, navigate to `http://<IP>/` in a browser and verify the management page loads.

**Acceptance Scenarios**:

1. **Given** the device is connected in STA mode, **When** the device obtains an IP address, **Then** the IP address is printed to the serial debug output.
2. **Given** the device is in STA mode, **When** a user navigates to `http://<device-IP>/` in a browser on the same network, **Then** a management web page loads within 3 seconds.
3. **Given** the management page is loaded, **When** the user reads it, **Then** the page clearly shows the device's current IP address and network name (SSID).

---

### User Story 3 — Trigger Firmware Update Mode (Priority: P2)

A developer opens the device's management web page, clicks the "Update firmware" button, confirms the action, and the device reboots into USB mass-storage mode. The developer then drags a new UF2 file onto the drive.

**Why this priority**: Enables over-the-air-style firmware updates without requiring physical access to the BOOTSEL button. Depends on US2 (management page must exist first).

**Independent Test**: Navigate to management page in STA mode, click "Update firmware", confirm, verify the device's USB drive appears on the developer's computer, drag-drop new UF2, verify device reboots and reconnects.

**Acceptance Scenarios**:

1. **Given** the management page is open, **When** the user clicks "Update firmware", **Then** a confirmation page is shown with a "Confirm update" button, a "Cancel" button, and a visible countdown from 60 seconds.
2. **Given** the user confirms the update action, **When** the device processes the confirmation, **Then** it reboots into USB mass-storage mode within 3 seconds.
3. **Given** the device is in USB mass-storage mode, **When** the USB drive is mounted on a host computer, **Then** the drive is labelled and accepts a UF2 file drop.
4. **Given** the user cancels the confirmation, **When** they press "Cancel" or navigate back, **Then** the device remains in normal STA operation with no interruption.

---

### Edge Cases

- What happens if the management page is accessed while WiFi credentials are being erased (e.g., during a simultaneous credential clear operation)?
- What happens if the device loses WiFi during the management page session? → Resolved: silent reconnection attempt; if it fails, falls back to AP provisioning mode (FR-014).
- What happens if the user triggers firmware update mode but does not write a UF2 within a defined timeout? → The device reboots back to STA mode after 60 seconds (FR-013).
- What happens if the firmware binary grows large enough to overlap the credential storage region?
- What if two users on the same network simultaneously click "Update firmware"?

---

## Requirements *(mandatory)*

### Functional Requirements

**Credential Persistence (Feature 1)**

- **FR-001**: WiFi credentials MUST be stored in a flash region that lies beyond the maximum permitted firmware binary boundary, such that a standard UF2 firmware update (which only writes pages present in the UF2 file) does not overwrite that region.
- **FR-002**: The credential flash region MUST be defined at a fixed, documented byte offset in flash. The offset MUST be the same across firmware versions to ensure forward compatibility.
- **FR-003**: The linker script MUST include an `ASSERT` (or equivalent build-time check) that fails the build with a clear error message if the firmware binary grows beyond the permitted boundary into the credential region.
- **FR-004**: The credential storage format and offset MUST be forward-compatible: a new firmware version MUST be able to read credentials written by the previous firmware version.

**STA Management Portal (Features 2 & 3)**

- **FR-005**: When the device successfully connects in STA mode and obtains an IP address, it MUST print the IP address and connected network name to the serial debug output.
- **FR-006**: While in STA mode, the device MUST serve a web-based management page accessible at the device's IP address on port 80.
- **FR-007**: The management page MUST display the device's current IP address and the name of the connected WiFi network.
- **FR-008**: The management page MUST include an "Update firmware" button.
- **FR-009**: Clicking the "Update firmware" button MUST navigate to a dedicated confirmation page. The confirmation page MUST display a "Confirm update" button, a "Cancel" button, and a visible countdown showing the remaining seconds before the device auto-reboots to STA mode.
- **FR-010**: On the confirmation page, if the user confirms, the device MUST reboot into USB mass-storage (BOOTSEL) mode within 3 seconds.
- **FR-011**: On the confirmation page, if the user cancels, the device MUST return to the management page with no disruption to WiFi operation.
- **FR-013**: If the device enters USB mass-storage mode and no UF2 file is written within 60 seconds, the device MUST automatically reboot back to normal STA operation. The confirmation page MUST display a countdown so the user knows how much time remains.
- **FR-014**: If the WiFi connection drops while the device is serving the STA management page, the device MUST attempt to reconnect to the stored network silently. If reconnection fails within 15 seconds, the device MUST fall back to AP provisioning mode using the existing credential-recovery path.

### Key Entities

- **Credential Flash Region**: A fixed, reserved flash sector (or contiguous group of sectors) at a known offset near the end of flash, lying beyond the firmware binary's maximum allowed size boundary.
- **Firmware Boundary**: The maximum byte offset that the firmware binary is allowed to occupy in flash. Enforced at link time. The credential region starts at or above this boundary.
- **STA Management Page**: A single-page web interface served by the device while in STA mode. Contains IP display and firmware update trigger.
- **USB MSD Mode**: The RP2040 ROM's built-in USB mass-storage device mode (equivalent to holding BOOTSEL on power-up). Accepts UF2 files dropped onto the drive.

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: WiFi credentials are present and usable after 10 consecutive firmware update cycles without re-provisioning.
- **SC-002**: The device's IP address appears in serial debug output within 500 ms of obtaining a DHCP lease.
- **SC-003**: The management web page loads within 3 seconds from a browser on the same LAN.
- **SC-004**: Clicking "Update firmware" and confirming transitions the device into USB MSD mode within 3 seconds of confirmation.
- **SC-005**: A firmware binary that would overflow into the credential region causes a build failure (linker error or equivalent), not a silent runtime failure.
- **SC-006**: A firmware update performed via USB MSD does not change the content of the credential flash region (verified by reading the region before and after update).

---

## Constitution Compliance

| Principle | Applicability | How Satisfied |
|-----------|--------------|---------------|
| I. PIO-First LED Drive | N/A | This feature adds no LED output paths. |
| II. Timing Precision | N/A | No POV column timing involved. |
| III. Hardware Abstraction | Applies | Flash partition management, STA HTTP server, and USB MSD reboot are separate modules from each other and from WiFi connection logic. |
| IV. Minimal & Deterministic Memory | Applies | STA HTTP server buffers are bounded. Credential region uses a single statically-known sector. No heap in flash-write paths. |
| V. Single-Command Build | Applies | Linker script changes and new source files integrated into CMakeLists.txt; `ninja -C build` remains the only build command. |

---

## Assumptions

- "Separate flash partition" means a fixed-offset reserved region defined in the linker script, not a full partition table with a runtime partition manager. No NVS library or wear-levelling is added.
- The UF2 bootloader (RP2040 ROM) does NOT erase flash sectors that are not covered by any block in the UF2 file; credentials are preserved as long as the credential region lies beyond the last byte address in the UF2.
- "Firmware update mode" means the RP2040 ROM's built-in USB MSD (BOOTSEL) mode, triggered programmatically via `reset_usb_boot(0, 0)`.
- The STA management page is served on port 80, the same port as the AP provisioning page (but the AP page is served during AP mode only, and the STA page during STA mode only — no port conflict).
- The management page is accessible without authentication; no password is needed to view the page or confirm a firmware update.
- If WiFi drops while in STA mode, the device attempts silent reconnection. On failure after 15 seconds, it falls back to AP provisioning mode (same as the existing credential-recovery path from the WiFi configuration feature).
- If WiFi drops while in STA mode, the device attempts silent reconnection. On failure after 15 seconds, it falls back to AP provisioning mode (same as the existing credential-recovery path from the WiFi configuration feature).
- USB MSD mode has a 60-second timeout. If no UF2 is written within 60 seconds, the device reboots back to STA mode automatically. The confirmation page shows a countdown timer.
- The IP address is displayed on serial output and also on the management page; no hardware display (LED matrix, OLED) is in scope for this feature.
