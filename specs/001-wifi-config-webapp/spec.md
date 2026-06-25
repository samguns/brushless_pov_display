# Feature Specification: WiFi Configuration Web App

**Feature Branch**: `001-wifi-config-webapp`

**Created**: 2026-06-25

**Status**: Draft

**Input**: User description: "Develop a web based app for user to configure WIFI connection. It fetches wifi credentials from ROM at startup. If none were found, fallback to wifi AP mode for user to provide BSSID and credential, then validate the connection. Save BSSID and credential into ROM"

---

## Clarifications

### Session 2026-06-25

- Q: Should the user be able to type an SSID manually alongside the scan list (e.g., for hidden networks)? → A: Scan list is the primary UI; a manual SSID entry option is provided as a fallback for hidden networks that do not appear in the scan results.
- Q: Should the setup AP require a password? → A: Yes. The AP uses a fixed WPA2 password of `12345678`. This is a known default printed on/with the device; users must know it to access the configuration page.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — First-Time Setup via Captive Portal (Priority: P1)

A new user powers on the device for the first time (or after a factory reset). No WiFi credentials are stored. The device automatically starts an access point. The user connects their phone or laptop to the device's own WiFi network, opens a browser, and is presented with a configuration page where they can enter the name of their home WiFi network and its password. After submitting, the device attempts to join that network and confirms success. Credentials are saved so the device connects automatically next time.

**Why this priority**: This is the only path to get the device onto a user's network. Without it, no other network-dependent feature works.

**Independent Test**: Flash a device with no stored credentials, power it on, connect a laptop to the device AP, open the configuration page, enter valid credentials, verify the device joins the target network and the page shows a success confirmation.

**Acceptance Scenarios**:

1. **Given** no credentials are stored, **When** the device boots, **Then** it broadcasts a named WiFi access point within 10 seconds.
2. **Given** the device is in AP mode, **When** the user navigates to the configuration page from a device connected to the AP, **Then** a list of discovered nearby WiFi networks is displayed and the user can select one or enter a network name manually.
3. **Given** the user submits a network name and password for a reachable network, **When** the device attempts to connect, **Then** the connection succeeds and the page shows a success message within 30 seconds.
4. **Given** a successful connection is validated, **When** the credentials are saved, **Then** subsequent reboots connect automatically without entering AP mode.

---

### User Story 2 — Automatic Connection on Boot (Priority: P1)

A returning user powers on the device. Credentials were previously saved. The device reads them and connects to the WiFi network silently in the background without any user interaction.

**Why this priority**: The normal operating mode for every boot after first setup. Failing to auto-connect would render the device non-functional for its primary purpose.

**Independent Test**: Store valid credentials, reboot the device, verify it joins the target network within 20 seconds with no user interaction and no AP being broadcast.

**Acceptance Scenarios**:

1. **Given** valid credentials are stored, **When** the device boots, **Then** it attempts to connect to the stored network and does not start AP mode.
2. **Given** valid credentials are stored and the network is reachable, **When** the connection attempt completes, **Then** the device is online within 20 seconds of boot.

---

### User Story 3 — Invalid Credentials Recovery (Priority: P2)

A user has stored credentials that are no longer valid (wrong password, network renamed, network removed). The device fails to connect on boot and falls back to AP mode so the user can reconfigure.

**Why this priority**: Without recovery, a misconfiguration permanently bricks network access. The fallback to AP mode provides a self-service recovery path.

**Independent Test**: Store intentionally wrong credentials, reboot, verify AP mode starts within 30 seconds, connect to the AP, submit correct credentials, verify the device joins the network.

**Acceptance Scenarios**:

1. **Given** stored credentials are present but connection fails after a reasonable timeout, **When** the timeout elapses, **Then** the device starts AP mode for reconfiguration.
2. **Given** the device is in AP mode after a failed auto-connect, **When** the user submits new credentials, **Then** the device validates and saves them if the connection succeeds.

---

### User Story 4 — Invalid Credential Submission Rejected (Priority: P2)

A user mistypes a password in the configuration form. The device attempts the connection, fails, and shows a clear error message on the configuration page. No credentials are saved. The user can try again without rebooting.

**Why this priority**: Prevents saving bad credentials that would trigger User Story 3 on every subsequent boot.

**Independent Test**: Submit a known-wrong password for a visible network, verify the form shows an error and that no credentials are written to storage.

**Acceptance Scenarios**:

1. **Given** the user submits credentials for a reachable network with a wrong password, **When** the connection attempt fails, **Then** the configuration page shows an error message within 30 seconds.
2. **Given** a failed connection attempt, **When** the error is shown, **Then** no credentials are written to persistent storage.
3. **Given** a failed connection attempt, **When** the error is shown, **Then** the form remains available for the user to retry without rebooting the device.

---

### Edge Cases

- What happens when the target network is reachable but takes longer than expected to respond?
- What happens when the user's browser is redirected to a captive portal detection URL — does the configuration page appear automatically?
- What happens if the network scan returns zero results?
- What happens if the user's network is hidden (not in scan results) — can they still connect via manual entry?
- What happens if the user submits an empty network name or password?
- What happens if storage write fails (e.g., flash error)?
- What if multiple users connect to the AP simultaneously and submit different credentials?

---

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: On boot, the device MUST read stored WiFi credentials from persistent storage before attempting any network operation.
- **FR-002**: If valid credentials are found in storage, the device MUST attempt to connect to the stored network in station (client) mode without starting AP mode.
- **FR-003**: If no credentials are found in storage, the device MUST start a WPA2-secured WiFi access point with a fixed, identifiable network name (e.g., `pov-leds-setup`) and the fixed default password `12345678`.
- **FR-004**: If stored credentials are found but the connection attempt fails after 15 seconds, the device MUST fall back to AP mode for reconfiguration.
- **FR-005**: While in AP mode, the device MUST serve a web-based configuration interface accessible from any standard browser connected to the device's AP.
- **FR-006**: The configuration interface MUST display a list of nearby WiFi networks discovered by a scan, allowing the user to select their network from the list.
- **FR-006b**: The configuration interface MUST also provide a manual SSID entry option (e.g., "Join other network…") for hidden networks that do not appear in the scan list.
- **FR-006c**: Regardless of whether the SSID was selected from the list or entered manually, the user MUST provide a password before submission.
- **FR-007**: On form submission, the device MUST attempt to connect to the specified network before saving any credentials.
- **FR-008**: If the connection attempt succeeds, the device MUST persist the network name and password to persistent storage.
- **FR-009**: If the connection attempt fails, the device MUST display a clear error message and MUST NOT write credentials to persistent storage.
- **FR-010**: The configuration interface MUST allow the user to retry submission after a failed attempt without rebooting the device.
- **FR-011**: Stored credentials MUST survive power cycles (device reboot, power loss).
- **FR-012**: The device MUST allow previously stored credentials to be replaced by successfully validated new credentials.
- **FR-013**: The device MUST perform a WiFi network scan and present the results on the configuration page so the user can select their network without typing the SSID manually.
- **FR-014**: If writing credentials to persistent storage fails, the device MUST display an error page and MUST NOT reboot. The device MUST remain in AP mode so the user can retry.

### Key Entities

- **WiFi Credential Record**: Represents the stored connection information. Attributes: network name (SSID, up to 32 characters), password (up to 63 characters). Stored in persistent (non-volatile) memory.
- **AP Session**: The temporary access point the device broadcasts when unconfigured or recovering. Has a fixed network name and no authentication (open AP) to allow unauthenticated first-time access.
- **Connection Attempt**: A transient operation that uses a credential record to try joining a network. Outcomes: success (device gets a network address) or failure (timeout or authentication rejection).

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A first-time user can complete the WiFi configuration flow (AP visible → form open → credentials submitted → connection confirmed) in under 3 minutes.
- **SC-002**: The device AP becomes visible to nearby devices within 10 seconds of boot when no credentials are stored.
- **SC-003**: A valid connection attempt (correct SSID + password, network in range) produces a confirmed success or failure result within 30 seconds of form submission.
- **SC-004**: On subsequent boots with valid stored credentials, the device connects to the target network within 20 seconds without user interaction.
- **SC-005**: 100% of failed connection attempts result in no credentials being written to storage (zero false-save rate).
- **SC-006**: The configuration page is accessible from any device connected to the AP using a standard web browser with no app installation required.

---

## Constitution Compliance

| Principle | Applicability | How Satisfied |
|-----------|--------------|---------------|
| I. PIO-First LED Drive | N/A | This feature adds no LED output paths. |
| II. Timing Precision | N/A | No POV column timing involved. |
| III. Hardware Abstraction | Applies | Flash driver (`wifi_flash.c`), scan wrapper (`wifi_scan.c`), HTTP handlers (`wifi_http.c`), DNS responder (`wifi_dns.c`), and boot orchestration (`wifi_config.c`) are separate source files. Each layer is independently replaceable. |
| IV. Minimal & Deterministic Memory | Applies | Scan result array (20 entries), credential struct, and connection state are statically allocated. No `malloc` in flash-write or connection paths. RAM budget documented in [data-model.md](data-model.md). |
| V. Single-Command Build | Applies | New source files added to `CMakeLists.txt` `target_sources`. Required libraries (`pico_cyw43_arch_lwip_poll`, `pico_lwip_http`, `pico_lwip_dns`) added to `target_link_libraries`. `ninja -C build` remains the only build command. |

---

## Assumptions

- "ROM" in the feature description refers to the device's built-in non-volatile flash memory, not read-only ROM; credentials can be written and overwritten.
- "BSSID" in the feature description is interpreted as SSID (human-readable network name); see Clarifications for the recorded decision.
- The configuration web interface is served by an HTTP server running on the device itself; no internet connectivity or cloud service is required.
- The setup AP is WPA2-secured with the fixed default password `12345678`. This password is static and must be communicated to the user (e.g., printed on the device or in documentation). Security of the configuration session itself (HTTPS) is out of scope for v1.
- Only one set of WiFi credentials is stored at a time (single-network mode). Multi-network credential storage is out of scope.
- The device's WiFi hardware supports switching between AP mode and station mode (requires a mode transition after validation).
- The user's browser does not need to support any advanced web features; the configuration page must work in a basic mobile or desktop browser.
- Network scan-and-select is in scope for v1. The scan list is the primary SSID selection UI; a manual entry fallback is provided for hidden networks (networks that do not broadcast their SSID and therefore do not appear in scan results).
