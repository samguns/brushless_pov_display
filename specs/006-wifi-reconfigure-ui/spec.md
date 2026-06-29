# Feature Specification: Wi-Fi Reconfiguration UI

**Feature Branch**: `006-wifi-reconfigure-ui`

**Created**: 2026-06-29

**Status**: Draft

**Input**: User description: "Current implementation has no way to configure WIFI AP. It inconvinient if the only way to specify a SSID is when credentials loaded fail and turns into STA mode. So I'd like to add a UI inferface to change SSID and corresponding credential."

## Clarifications

### Session 2026-06-29

- Q: Should the reconfiguration UI let the owner pick a network from a scan while in STA mode? → A: Yes — supplement manual SSID entry with a scanned, selectable list of nearby networks.
- Q: How should the network scan be triggered in the STA reconfiguration UI? → A: Explicit "Scan" button — manual entry is shown by default and a scan runs only when the owner requests it (avoids disrupting the live connection on every page view).
- Q: Should changing the Wi-Fi network require authorization? → A: No — the change endpoint is open (no admin token), consistent with the firmware-update endpoint. Accepted tradeoff: any client with network access can change the configured network.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Change Wi-Fi Network from the Device UI (Priority: P1)

As the device owner, while the device is connected to my network, I want to open
its web page and change the Wi-Fi network it joins (SSID and password) so that I
can move it to a different network without having to make it fail and fall back
into setup/AP mode.

**Why this priority**: This is the core capability the user is asking for and the
minimum that delivers value. Today the SSID can only be set during boot-time AP
provisioning (after stored credentials fail to load), which is the inconvenient
path being replaced.

**Independent Test**: While connected in normal (STA) mode, open the device UI,
enter a different valid network's SSID and password, submit, and confirm the
device joins the new network and continues to be reachable there.

**Acceptance Scenarios**:

1. **Given** the device is connected to a network and reachable via its web UI,
   **When** the owner submits a new valid SSID and password, **Then** the device
   connects to the new network and reports success.
2. **Given** new credentials were applied successfully, **When** the device
   restarts, **Then** it automatically reconnects to the newly configured network.
3. **Given** the owner is on the device UI, **When** they open the configuration
   option, **Then** they can change the network without the device first entering
   AP/setup mode.

---

### User Story 2 - Safe Handling of Bad Credentials (Priority: P2)

As the device owner, if I enter a wrong password or an unreachable network, I
want the device to stay reachable and tell me it failed, so that a typo does not
strand the device or force a full re-provisioning.

**Why this priority**: Without safe failure handling, the new capability could
leave the device unreachable, which is worse than the current behavior. It
depends on US1 existing but protects its value.

**Independent Test**: Submit deliberately wrong credentials and confirm the
device does not become unreachable — it returns to a reachable state and surfaces
a clear failure message.

**Acceptance Scenarios**:

1. **Given** the device is connected, **When** the owner submits credentials that
   fail to connect, **Then** the device retains its previously working
   configuration and remains reachable.
2. **Given** a failed change attempt, **When** the result is shown, **Then** the
   UI clearly indicates the change did not take effect and why (at a high level).
3. **Given** a failed attempt, **When** the device restarts, **Then** it uses the
   last known working credentials, not the rejected ones.

---

### User Story 3 - Guided, Validated Input (Priority: P3)

As the device owner, I want the configuration form to show my current network,
mask the password, and reject obviously invalid input, so that I can make changes
confidently and avoid mistakes.

**Why this priority**: Improves usability and reduces failed attempts, but the
feature is still usable without it once US1/US2 exist.

**Independent Test**: Open the configuration form and confirm the current SSID is
shown, the password field is masked, and invalid entries (empty SSID, over-length
values) are rejected before any connection attempt.

**Acceptance Scenarios**:

1. **Given** the configuration form, **When** it is displayed, **Then** the
   currently configured SSID is shown for context and the password entry is
   masked.
2. **Given** an empty SSID or an over-length SSID/password, **When** the owner
   submits, **Then** the input is rejected with a clear message and no
   disconnect/reconnect is attempted.
3. **Given** a valid entry, **When** the owner submits, **Then** the form
   provides immediate feedback that the change is being applied.

### User Story 4 - Pick Network from a Scanned List (Priority: P2)

As the device owner, I want the reconfiguration UI to show a list of nearby Wi-Fi
networks I can select, so that I don't have to type the SSID exactly and can pick
the right network at a glance.

**Why this priority**: It meaningfully reduces typos and friction (the main
inconvenience being addressed), and the owner explicitly requested it. It builds
on US1 (manual entry remains the fallback for hidden networks).

**Independent Test**: Open the reconfiguration UI, trigger/observe a scan, and
confirm nearby networks are listed and selecting one fills in its SSID so only
the password must be entered.

**Acceptance Scenarios**:

1. **Given** the reconfiguration UI, **When** the owner activates the "Scan"
   control and it completes, **Then** nearby networks are listed (with their
   security/lock indication) for selection.
2. **Given** the scanned list, **When** the owner selects a network, **Then** its
   SSID is used for the change and the owner only needs to enter the password.
3. **Given** a hidden network not shown in the list, **When** the owner needs it,
   **Then** they can still enter the SSID manually (US1 path remains available).
4. **Given** the scan finds no networks, **When** the result is shown, **Then**
   the UI indicates none were found and manual entry is still available.

### Edge Cases

- **Scan disrupts the live connection**: Because the radio is single-mode,
  scanning from STA mode may briefly perturb the active connection; this must not
  drop the device permanently or corrupt state.
- **Hidden / non-broadcast SSID**: Not shown in the scan list; the owner must
  still be able to enter it manually.
- **Duplicate SSIDs in scan**: Listed without breaking selection (owner picks one
  by name; identical names are de-duplicated/strongest-signal shown).
- **Wrong password / unreachable network**: Device must remain reachable and
  report failure (US2).
- **Empty or over-length SSID/password**: Rejected before any connection attempt.
- **Same SSID, new password**: Treated as a normal credential change.
- **Connection drop during switch**: Because the radio cannot serve the old and
  new network simultaneously, the current connection is briefly dropped to test
  the new one; the owner is informed the page may need to be reopened on the new
  network.
- **Power loss mid-change**: Stored credentials must never be left in a corrupt or
  half-written state; the device boots with either the old or the new complete set.
- **Repeated/abusive attempts**: Repeated failed change attempts must not
  destabilize the device or leave it unreachable.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST provide a UI, available while connected in normal
  (STA) operation, to enter a new Wi-Fi SSID and password.
- **FR-002**: The owner MUST be able to change the network without the device
  first failing credential load or entering AP/setup mode.
- **FR-003**: The system MUST validate submitted input (non-empty SSID, SSID and
  password within supported length limits) before attempting any network change.
- **FR-004**: The system MUST attempt to connect using the newly submitted
  credentials within a bounded time window.
- **FR-005**: On a successful connection, the system MUST persist the new
  credentials to non-volatile storage so they survive a restart.
- **FR-006**: On a failed connection, the system MUST retain the previously
  working credentials and return the device to a reachable state.
- **FR-007**: The UI MUST present clear success or failure feedback for each
  change attempt.
- **FR-008**: The UI MUST display the currently configured SSID for context.
- **FR-009**: The system MUST allow credential changes without authorization; the
  change endpoint is open (no admin token required), consistent with the
  firmware-update endpoint. (Accepted tradeoff: any client with network access can
  change the configured network.)
- **FR-010**: Persisted credentials MUST be used automatically on subsequent boots.
- **FR-011**: Credential storage MUST be written so that an interruption never
  leaves a partially written/corrupt credential record.
- **FR-012**: The system MUST NOT display the stored password in plaintext in the
  UI or status output.
- **FR-013**: The reconfiguration flow MUST keep the device responsive, limiting
  any pause in normal operation to the bounded connection-attempt window.
- **FR-014**: The UI MUST provide an explicit control to trigger a Wi-Fi scan and
  then present the discovered nearby networks (each with SSID and a secured/open
  indication); scanning MUST run only when requested, not automatically on page
  load.
- **FR-015**: Selecting a scanned network MUST populate its SSID for the change so
  the owner only needs to provide the password.
- **FR-016**: Manual SSID entry MUST remain available alongside the scanned list
  so hidden/non-broadcast networks can still be configured.
- **FR-017**: Scanning MUST NOT permanently drop the device's connectivity or
  corrupt stored credentials, even though it may briefly perturb the active link.

### Key Entities *(include if feature involves data)*

- **Credential Change Request**: The owner-submitted new SSID, new password, and
  authorization needed to apply a change.
- **Stored Credentials**: The persisted SSID/password the device uses to connect,
  including the currently active set and the candidate set under test.
- **Reconfiguration Result**: The outcome of a change attempt — applied,
  rejected (validation), or failed (could not connect) — with a high-level reason
  for display.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: An owner can change the device's Wi-Fi network entirely from the UI,
  without triggering AP/setup mode, in under 60 seconds for a reachable network.
- **SC-002**: After a successful change, the device automatically uses the new
  network on its next restart in 100% of cases.
- **SC-003**: When submitted credentials fail to connect, the device remains
  reachable (via its previous network or its existing fallback behavior) in 100%
  of failed attempts.
- **SC-004**: Every change attempt results in a clear success or failure message
  in the UI (no silent or ambiguous outcomes).
- **SC-005**: Invalid inputs (empty SSID, over-length SSID/password) are rejected
  before any disconnect/reconnect in 100% of cases.
- **SC-006**: The stored password is never shown in plaintext in the UI or status
  responses.
- **SC-007**: From the reconfiguration UI, an owner can select a nearby network
  from a scanned list and complete a successful change by entering only the
  password, without typing the SSID, for in-range broadcast networks.

## Constitution Compliance

| Principle | Applicability | How Satisfied |
|-----------|---------------|---------------|
| I. PIO-First LED Drive | Not applicable | This is a connectivity/UI feature; it does not change the LED output path. |
| II. Timing Precision | Not applicable | No column/LED timing is affected; any connection timeout is a coarse, bounded value. |
| III. Hardware Abstraction | Applies | Web/UI handling, connection control, and credential storage remain separated responsibilities within the existing Wi-Fi modules. |
| IV. Minimal and Deterministic Memory Use | Applies | Reuses fixed-size credential buffers and existing static request buffers; no heap in the credential-write path. |
| V. Single-Command Build and Flash | Applies | Builds with the existing `ninja -C build` flow; no new build steps. |

## Debug Output Strategy

- Development validation reuses the project's existing USB stdio logging to
  observe change requests, validation results, connection attempts, and
  success/failure outcomes.
- Release builds keep stdio disabled unless a release note justifies enabling it.
- Logs must avoid printing the password in plaintext.

## Assumptions

- The reconfiguration UI is surfaced in the existing STA-mode status web page (the
  page already shown while the device is connected).
- Credential changes are unauthenticated (open), consistent with the
  firmware-update endpoint; no admin token is required. Accepted tradeoff: any
  client with network access can change the configured network.
- Credential storage reuses the existing reserved-flash credential record; writes
  are done so a complete old-or-new record is always present.
- The wireless hardware cannot serve the old and new network at the same time, so
  applying a change briefly drops the current connection to validate the new one;
  on failure it reconnects using the previous credentials.
- Target networks are WPA2/PSK (SSID up to 32 characters, password 8–63
  characters); open and enterprise networks are out of scope for this feature.
- In-UI network scanning/selection IS supported in STA mode and supplements
  manual SSID entry; the owner can still type an SSID manually for hidden
  networks. STA scanning reuses the scan capability already used by the boot-time
  AP setup flow.
- A brief pause of normal device operation during the bounded connection attempt
  is acceptable for this infrequent, owner-initiated action.
- The target board and existing Wi-Fi modules from features 001–003 are the
  integration point.
