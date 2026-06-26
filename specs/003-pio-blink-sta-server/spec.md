# Feature Specification: PIO Blink with STA Server

**Feature Branch**: `003-pio-blink-sta-server`

**Created**: 2026-06-26

**Status**: Draft

**Input**: User description: "Make PIO blinking runs alongside wifi STA http server"

---

## Clarifications

### Session 2026-06-26

- Q: What access control should the STA HTTP endpoint use? → A: Require a shared admin token for mutating/config endpoints while allowing read-only status access.
- Q: How should unauthorized mutating/config requests be handled? → A: Return HTTP 401 with a concise JSON error body.
- Q: How is the admin token provisioned and retained? → A: Store the token in flash alongside credentials and configure/update it through the existing provisioning flow.
- Q: How should repeated unauthorized mutating/config requests be handled? → A: Apply request throttling and return HTTP 429 when the limit is exceeded.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Concurrent Blink and Web Access (Priority: P1)

A user opens the device web page while the LED is blinking. The blink continues smoothly and the page remains responsive.

**Why this priority**: The core value is concurrent behavior. If either blink or web serving blocks the other, the feature fails.

**Independent Test**: Connect the device in STA mode, observe continuous LED blinking for at least 60 seconds while repeatedly loading the web page from a browser on the same LAN.

**Acceptance Scenarios**:

1. **Given** the device is connected in STA mode and blinking is active, **When** a user sends repeated page requests, **Then** the LED continues blinking without visible pauses.
2. **Given** blinking is active, **When** multiple web requests are served, **Then** each request returns a valid response while blink behavior remains unchanged.

---

### User Story 2 - Stable Blink During Network Events (Priority: P1)

A user experiences temporary network instability (router delay, reconnection). The device keeps blinking and recovers web access after reconnection.

**Why this priority**: Real deployments have intermittent WiFi issues; blink output must remain stable during those events.

**Independent Test**: With blinking active in STA mode, temporarily interrupt network connectivity and restore it; verify blink continuity and eventual web recovery.

**Acceptance Scenarios**:

1. **Given** blinking and STA web service are active, **When** WiFi connectivity drops briefly, **Then** LED blinking continues at the configured cadence.
2. **Given** connectivity is restored, **When** the device reconnects, **Then** the web page becomes reachable again without requiring a reboot.
3. **Given** 3 rapid disconnect/reconnect flaps within 60 seconds, **When** STA recovery logic runs, **Then** blink continuity is preserved and HTTP service remains recoverable after each flap.
4. **Given** the STA listener is restarted after a reconnect event, **When** a status request is sent, **Then** a successful response is returned without rebooting the device.

---

### User Story 3 - Preserve Existing Provisioning Behavior (Priority: P2)

A developer uses the existing AP provisioning flow on an unconfigured device. The new concurrent blink behavior does not break provisioning.

**Why this priority**: This feature must integrate with existing WiFi setup capabilities and must not regress current workflows.

**Independent Test**: Erase credentials, boot device, confirm AP setup flow still works, then provision STA credentials and verify concurrent blink + web behavior.

**Acceptance Scenarios**:

1. **Given** no saved credentials exist, **When** the device boots, **Then** AP provisioning behavior remains unchanged.
2. **Given** credentials are saved through provisioning, **When** the device enters STA mode, **Then** concurrent blinking and STA web serving are active.

---

### Edge Cases

- What happens when HTTP requests arrive continuously for an extended period?
- What happens when STA reconnect attempts are in progress while blinking is active?
- What happens if the web server is unavailable temporarily (port busy or listener restart)?
- What happens if the blink timing configuration is invalid or out of range?
- What happens during rapid STA reconnect/disconnect cycles?
- How does the system respond to repeated invalid token attempts from the same client?

---

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST run LED blinking and STA HTTP serving concurrently when in STA mode.
- **FR-002**: LED blinking MUST continue while serving STA HTTP traffic at a sustained rate of at least 1 request/second for 60 seconds from a single LAN client, with request success measured by SC-002.
- **FR-003**: HTTP request handling and reconnect processing MUST be non-blocking with respect to blink scheduling.
- **FR-004**: Blink cadence in STA mode MUST remain within +/-5% of the configured blink frequency while serving HTTP requests.
- **FR-005**: Existing AP provisioning behavior MUST remain functionally unchanged.
- **FR-006**: On transition from AP provisioning to STA mode, concurrent blink + STA HTTP behavior MUST start automatically without a reboot.
- **FR-007**: During temporary STA disconnects, blinking MUST continue while the device attempts reconnection.
- **FR-008**: After STA reconnection, HTTP service MUST recover automatically.
- **FR-009**: The device MUST report key runtime status in debug output: STA connected/disconnected, active IP, and blink active state.
- **FR-010**: The implementation MUST preserve compatibility with existing saved WiFi credentials and startup flow.
- **FR-011**: Mutating or configuration HTTP endpoints in STA mode MUST require a valid shared admin token; read-only status endpoints MAY be accessed without the token.
- **FR-012**: If a mutating or configuration request is missing or has an invalid admin token, the server MUST respond with HTTP 401 and a concise JSON error body.
- **FR-013**: The shared admin token MUST be persisted in flash with the WiFi credential record and MUST be configurable through the existing provisioning flow.
- **FR-014**: Mutating/configuration endpoint requests with missing or invalid tokens MUST be throttled, and requests exceeding the throttle limit MUST receive HTTP 429.

### Key Entities *(include if feature involves data)*

- **Blink Runtime State**: Current on/off phase, configured frequency, and last transition timestamp.
- **STA Web Service State**: Listener active/inactive status, request handling state, and service readiness after reconnect.
- **Connectivity State**: STA connected/disconnected/reconnecting status used to coordinate service recovery without interrupting blink output.
- **Admin Access Credential**: Shared admin token lifecycle state (configured, updated, persisted) used to authorize mutating/configuration endpoints.

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: During a 60-second test with repeated page loads, the LED shows no visible freeze events.
- **SC-002**: At least 95% of web page requests complete successfully while blinking remains active.
- **SC-003**: Blink frequency deviation stays within ±5% of target while serving HTTP requests.
- **SC-004**: After a temporary WiFi outage, web access recovers within 20 seconds after connectivity returns, with blinking uninterrupted.
- **SC-005**: Under a burst of invalid-token mutating requests, throttling engages and valid-token requests continue to complete successfully.

---

## Constitution Compliance

| Principle | Applicability | How Satisfied |
|-----------|--------------|---------------|
| I. PIO-First LED Drive | Applies | Blink output remains PIO-driven and is not moved to CPU bit-banging. |
| II. Timing Precision | Applies | Blink cadence is treated as timing-sensitive and measured against target tolerance. |
| III. Hardware Abstraction | Applies | Blink control and STA web serving remain in separate modules with explicit coordination boundaries. |
| IV. Minimal & Deterministic Memory | Applies | No unbounded runtime allocation is introduced in blink or request hot paths. |
| V. Single-Command Build | Applies | All changes remain in the existing build target and continue to build via `ninja -C build`. |

## Debug Output Strategy

- Development and validation builds use USB CDC stdio (`pico_enable_stdio_usb(..., 1)` and UART disabled) so FR-009 runtime logs are observable without extra hardware.
- Required FR-009 log events: STA connected/disconnected, active IP after DHCP/reconnect, and blink active state transitions.
- Release profile defaults to the same output mode unless a later release hardening task explicitly changes it with documented rationale.

---

## Assumptions

- Existing STA HTTP server functionality from prior features is available and remains the base behavior.
- Blink output refers to the current PIO-based LED blink path already used in the project.
- "Alongside" means both services are active at the same time in STA mode, not time-sliced by user interaction.
- Request load in this feature is limited to normal LAN browser usage, not stress-test traffic.
- No new user-facing configuration UI is required for blink frequency in this feature; current blink settings are reused.
- Existing provisioning flow can be extended to carry and update the shared admin token without introducing a separate configuration application.
