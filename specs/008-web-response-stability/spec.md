# Feature Specification: Web Response Stability

**Feature Branch**: `008-web-response-stability`

**Created**: 2026-07-08

**Status**: Draft

**Input**: User description: "The web does not work as stable as I expect, for it usually does not respond. Fix this issue. You can continue all remaining speckit procedures autonomously except git commit."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Portal responds after idle browser sockets (Priority: P1)

As a device owner opening the management portal in a normal desktop or mobile browser, I need the page to load even when the browser opens extra idle connections before the actual request, so I can reliably inspect and manage the display.

**Why this priority**: A portal that frequently does not respond blocks every management task.

**Independent Test**: Open a TCP connection to the portal without sending a request, wait past the idle window, then load `GET /`; the Overview page responds normally.

**Acceptance Scenarios**:

1. **Given** an idle browser/preconnect socket is open, **When** no request bytes arrive within the timeout window, **Then** the device releases that socket.
2. **Given** the idle socket has been released, **When** the user requests the portal page, **Then** the request receives a complete response.

---

### User Story 2 - Portal recovers from stalled transfers (Priority: P2)

As a user on a weak Wi-Fi link, I need the portal to recover from a client that stops receiving or acknowledging a response, so later requests are not blocked indefinitely.

**Why this priority**: Stalled connections can occupy the single-client portal and make it appear permanently unavailable.

**Independent Test**: Start a page request and stop reading before the response completes; after the progress timeout, a new `GET /status` request succeeds.

**Acceptance Scenarios**:

1. **Given** a response is in progress, **When** no receive, send, or acknowledgment progress occurs within the timeout window, **Then** the device closes the stalled client.
2. **Given** a stalled client was closed, **When** a new client connects, **Then** the portal accepts and serves the new request.

---

### User Story 3 - Portal remains safe for existing actions (Priority: P3)

As a maintainer, I need the reliability fix to preserve existing portal behavior, so status, settings, Wi-Fi changes, brightness changes, and firmware update confirmation continue to work.

**Why this priority**: Stability must not regress the management workflows that already exist.

**Independent Test**: Build the firmware and exercise the existing portal routes; responses remain complete and mutating actions still stage their deferred work.

**Acceptance Scenarios**:

1. **Given** a normal request is actively sending or receiving, **When** progress continues, **Then** the timeout does not interrupt it.
2. **Given** a mutating action has sent its response, **When** deferred work is pending, **Then** the existing deferred action still runs after the reply has been flushed.

### Edge Cases

- Browsers may open a connection and send no bytes at all.
- Browsers may request small auxiliary paths such as `/favicon.ico` while another request is active.
- A client may disconnect or reset during a multi-chunk response.
- A scan request may take several seconds; active polling during that operation must continue to keep the Wi-Fi stack moving.
- A second connection may arrive while a previous connection is being closed or timed out.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The portal MUST release a connected client that sends no complete request within a bounded idle window.
- **FR-002**: The portal MUST release a connected client whose response transfer makes no progress within a bounded progress window.
- **FR-003**: The portal MUST accept and respond to a later client after an idle or stalled client is released.
- **FR-004**: The portal MUST keep active requests alive while bytes are being received, response data is being queued, or response acknowledgments are arriving.
- **FR-005**: The portal MUST clear all per-client request and response state whenever the client is closed, reset, aborted, or timed out.
- **FR-006**: The portal MUST preserve the existing single-client memory model and avoid dynamic allocation.
- **FR-007**: Existing status, settings, scan, brightness, Wi-Fi reconfiguration, and firmware update workflows MUST continue to return complete responses.

### Key Entities

- **Portal Client Session**: The currently accepted TCP client, including request accumulation, response streaming state, and last progress time.
- **Request Buffer**: Fixed-size accumulated HTTP request bytes for the current session.
- **Response Stream**: Fixed-size static page buffer plus send counters used to stream larger HTML responses.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After a silent client connects, a subsequent portal page request succeeds within 10 seconds.
- **SC-002**: After a stalled response client stops making progress, a subsequent status request succeeds within 15 seconds.
- **SC-003**: At least 20 consecutive normal loads of `/` and `/settings` complete without requiring a device reboot.
- **SC-004**: The firmware build completes successfully with no new dynamic memory dependency.

## Assumptions

- The portal continues to support one active client at a time to preserve deterministic memory use.
- Browser preconnect and auxiliary requests are considered normal client behavior and should not be able to wedge the portal.
- Hardware validation remains manual because this is embedded firmware without an existing host-side lwIP test harness.
