# Feature Specification: Web Log Viewer

**Feature Branch**: `master`

**Created**: 2026-07-19

**Status**: Draft

**Input**: User description: "Create a log page in the web UI to watch logs, because current logs are displayed in the USB serial console and no USB cable is connected while the PCB is spinning."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Watch Live Device Logs Wirelessly (Priority: P1)

As a device owner diagnosing the spinning display, I want to open a Logs page in
the existing management interface and see new device messages appear
automatically so I can observe operation without attaching a USB cable.

**Why this priority**: Wireless visibility into current runtime behavior is the
core reason for the feature and removes the unsafe or impractical dependency on a
cable connected to rotating hardware.

**Independent Test**: Run the board without USB, open Logs from another device on
the same network, trigger known clock, Hall-sensor, display-health, and Wi-Fi
events, and verify that labelled messages appear automatically in timestamp order.

**Acceptance Scenarios**:

1. **Given** the board is operating and reachable over Wi-Fi without a USB cable,
   **When** the owner opens Logs, **Then** the page displays the recent retained
   diagnostic history and identifies that it belongs to the current boot session.
2. **Given** the Logs page is open, **When** the board produces a new diagnostic
   message, **Then** the message appears automatically with its timestamp, source,
   and text without a full-page refresh.
3. **Given** messages are produced by multiple runtime areas, **When** they appear
   on Logs, **Then** they retain production order and their source labels make the
   originating area distinguishable.
4. **Given** no new messages are being produced, **When** the owner watches Logs,
   **Then** the page remains usable and does not manufacture duplicate entries.

---

### User Story 2 - Inspect Activity Around a Fault (Priority: P2)

As a device owner, I want a bounded history of messages from before I opened the
page and simple viewing controls so I can inspect the sequence around an
intermittent fault instead of seeing only messages produced after connection.

**Why this priority**: Many display faults occur before a browser can be opened;
recent history and control over scrolling make the live stream diagnostically
useful while keeping resource use predictable.

**Independent Test**: Produce more than one screen of messages before and after
opening Logs, pause the live view, inspect an older entry, resume it, and verify
that retained messages are ordered and that overwritten history is clearly
indicated.

**Acceptance Scenarios**:

1. **Given** diagnostic messages were produced before the page was opened during
   the current boot, **When** the owner opens Logs, **Then** the retained messages
   are visible from oldest retained to newest.
2. **Given** the owner pauses live following, **When** new messages arrive,
   **Then** the current reading position is preserved and the interface indicates
   that unseen messages are waiting.
3. **Given** the owner resumes live following, **When** unseen messages exist,
   **Then** they are displayed in order and the view returns to the newest entry.
4. **Given** older messages have been overwritten because the retention limit was
   reached, **When** the page requests history, **Then** the page indicates a gap
   rather than implying that the displayed history is complete.

---

### User Story 3 - Recover From a Temporary Network Loss (Priority: P3)

As a device owner, I want the Logs page to show when its connection is interrupted
and recover automatically so a brief Wi-Fi disturbance does not silently leave me
watching stale data.

**Why this priority**: The spinning assembly is observed over a wireless link, so
connection state must be explicit, but live viewing and retained history still
deliver value before automatic recovery is added.

**Independent Test**: Open Logs, temporarily interrupt browser access while the
board remains running, restore access, and verify visible connection state,
automatic recovery, ordered catch-up, and explicit indication of any lost range.

**Acceptance Scenarios**:

1. **Given** the Logs page can no longer retrieve updates, **When** the connection
   timeout is reached, **Then** it visibly reports that live updates are
   disconnected and preserves the already displayed entries.
2. **Given** connectivity returns during the same boot session, **When** the page
   reconnects, **Then** it resumes after the last received entry without
   duplicating entries.
3. **Given** the board rebooted while the page was disconnected, **When** the page
   reconnects, **Then** it identifies a new boot session and does not merge the
   new messages into the old session as though they were continuous.

### Edge Cases

- The history is empty because the board has just started or no messages have
  been retained; the page shows an empty-state message and continues watching.
- A diagnostic message is longer than the supported entry size; it is safely
  truncated with a visible marker rather than corrupting surrounding entries.
- A message contains markup, control characters, or non-ASCII text; it is shown
  as inert text and cannot alter or execute page content.
- Messages arrive faster than the browser can consume them; the newest bounded
  history remains available and any skipped sequence is explicitly indicated.
- The bounded history becomes full; the oldest entries are replaced without
  blocking normal device operation.
- The page is opened from a narrow phone-sized screen; log text remains readable
  and horizontal overflow does not break navigation or controls.
- A firmware update, reboot, or Wi-Fi reconfiguration starts while Logs is open;
  the page reports loss of live updates without interfering with the operation.
- Sensitive values occur in diagnostic context; credentials, administrative
  secrets, firmware payloads, and request bodies are never exposed in the page.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The connected-device management interface MUST provide a clearly
  labelled `Logs` destination in its primary navigation.
- **FR-002**: The Logs page MUST be usable while the board operates without a USB
  connection and MUST use the same local management access boundary as the
  existing interface.
- **FR-003**: On opening, the Logs page MUST show the recent diagnostic entries
  retained for the current boot session, ordered from oldest retained to newest.
- **FR-004**: The Logs page MUST display each entry's ordering identity, time
  relative to the current boot, source label, and message text.
- **FR-005**: The page MUST show newly produced entries automatically without
  requiring manual or full-page refresh.
- **FR-006**: Under normal supported network conditions, the page MUST preserve
  entry order and MUST NOT display the same entry more than once.
- **FR-007**: The runtime MUST make existing operational diagnostic messages from
  display, Hall-sensor, clock, time synchronization, network, update, and system
  health areas available to the retained web-visible history after logging is
  initialized.
- **FR-008**: The web-visible history MUST be bounded, MUST retain at least the
  newest 128 entries under their supported maximum entry size, and MUST replace
  the oldest entries first when full.
- **FR-009**: Each individual entry MUST have a fixed maximum size; truncated
  entries MUST carry an explicit truncation indicator.
- **FR-010**: The history MUST exist only for the current boot session and MUST
  reset on reboot; the page MUST expose enough session identity to distinguish a
  reboot from a continuous stream.
- **FR-011**: When requested entries are no longer retained, the page MUST
  explicitly indicate that one or more entries were missed or overwritten.
- **FR-012**: The page MUST provide a control to pause and resume automatic
  following without pausing device-side log capture.
- **FR-013**: While following is paused, the page MUST preserve the user's reading
  position and indicate the presence of unseen entries.
- **FR-014**: The page MUST visibly distinguish `connecting`, `live`, and
  `disconnected` update states and MUST attempt to resume live viewing after a
  temporary connection loss.
- **FR-015**: Reconnection within the same boot session MUST continue after the
  last received entry when it is still retained, without duplicates; otherwise
  it MUST show the resulting gap.
- **FR-016**: Diagnostic text MUST be rendered as inert text so message content
  cannot alter the page or execute browser behavior.
- **FR-017**: The web-visible log stream MUST exclude or redact Wi-Fi passwords,
  administrative tokens, authorization values, firmware payload contents, and
  complete request bodies before entries become retrievable.
- **FR-018**: Opening, watching, pausing, disconnecting, or reconnecting the Logs
  page MUST NOT change device configuration, clear device history, initiate a
  restart, or change display behavior.
- **FR-019**: Capturing and serving logs MUST NOT block interrupt handling, LED
  output, rotation measurement, or the normal management-interface service loop.
- **FR-020**: The Logs page MUST preserve the existing management interface's
  navigation, visual theme, responsive behavior, Overview, Settings, and firmware
  update capabilities.
- **FR-021**: If live updates cannot be retrieved, the page MUST retain already
  displayed content and present a useful status rather than showing an empty or
  falsely live view.
- **FR-022**: Web log viewing MUST be the supported in-field diagnostic path for
  this feature; production operation MUST NOT require USB or UART logging to be
  enabled.
- **FR-023**: The Logs page MUST provide a Clear control that removes the rows
  currently displayed in that browser without clearing device history or
  rewinding the displayed cursor; subsequently arriving entries MUST continue
  to appear normally.

### Key Entities

- **Log Entry**: One ordered diagnostic event from the current boot, including a
  sequence identity, boot-relative timestamp, source label, message text, and
  whether its text was truncated.
- **Boot Session**: The running interval between board starts, used to prevent
  entries from different boots from appearing as one continuous sequence.
- **Log History**: The bounded collection of newest entries available to a newly
  opened or reconnecting viewer, including the retained sequence range.
- **Viewer State**: The page's connection status, last received entry identity,
  follow/pause choice, reading position, unseen count, and any detected gap.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can reach Logs in one interaction from either Overview or
  Settings and see retained entries without connecting a USB cable.
- **SC-002**: At least 95% of new diagnostic entries become visible to a connected
  viewer within two seconds, and all become visible within five seconds under
  normal supported local-network conditions.
- **SC-003**: In ordered-stream tests covering at least 1,000 generated entries,
  the viewer displays zero duplicates and zero unexplained ordering reversals.
- **SC-004**: After more than 128 entries are generated, the newest 128 entries
  remain retrievable and the viewer explicitly reports that older entries were
  overwritten.
- **SC-005**: In 100% of pause-and-resume tests, the reading position remains
  stable while paused, unseen activity is indicated, and resuming returns the
  user to the newest available entry.
- **SC-006**: In 100% of temporary-disconnection tests, the page identifies loss
  of live updates within five seconds, preserves displayed entries, and resumes
  or reports a gap after connectivity returns.
- **SC-007**: Across tests containing representative credentials, tokens,
  authorization values, request bodies, markup, and control characters, zero
  secrets are exposed and zero message strings alter executable page behavior.
- **SC-008**: During continuous log capture and one active viewer, measured display
  timing remains within the project's existing sub-microsecond jitter budget and
  no Hall events or rendered columns are lost because of log viewing.
- **SC-009**: All existing Overview, Settings, Wi-Fi management, reboot, and
  firmware-update user journeys pass regression testing with Logs enabled.
- **SC-010**: In a usability trial, at least 90% of users can locate the page,
  identify whether it is live, pause to inspect an older entry, and resume without
  assistance on their first attempt.

## Constitution Compliance

| Principle | Applicability | Compliance |
|---|---|---|
| I. PIO-First LED Drive | Applies | Log capture and web viewing must not move LED output onto the processor or alter the existing output pipeline. |
| II. Timing Precision | Applies | Log activity must remain outside timing-critical work and preserve the existing sub-microsecond column-jitter budget. |
| III. Hardware Abstraction | Applies | Diagnostic-event capture and presentation are separated from display and sensor hardware behavior and remain independently testable. |
| IV. Minimal and Deterministic Memory Use | Applies | Current-boot history and entry sizes are fixed and bounded; oldest entries are overwritten and no heap use is permitted in capture or display paths. |
| V. Single-Command Build and Flash | Applies | Logs are part of the existing firmware and management interface and add no separate runtime or build step. |

## Static RAM Budget

- The retained history, its metadata, and capture state MUST use fixed-size
  storage and add no more than 16 KiB of persistent static RAM.
- Existing shared response storage SHOULD be reused for page and update responses;
  any unavoidable additional response storage MUST be fixed-size and included in
  the 16 KiB feature limit.
- Log capture MUST perform no heap allocation, and no log-history storage may be
  added to interrupt handlers or the timing-critical LED output path.
- Planning and validation MUST record the final persistent byte delta and verify
  it against the limit before the feature is accepted.

## Assumptions

- The Logs page is part of the existing station-mode local management interface;
  public internet access, cloud forwarding, and a new authentication system are
  outside this feature.
- One active log viewer at a time is sufficient for the initial feature, matching
  the management interface's current constrained-device usage model.
- Current runtime diagnostic categories are useful, but sensitive or excessively
  verbose content may be redacted, summarized, or omitted for safety.
- Boot-relative timestamps are sufficient because wall-clock time may not yet be
  calibrated when early messages are produced.
- The retained history is diagnostic and ephemeral; persistence across reboot,
  export/download, full-text search, severity filtering, and long-term archives
  are outside the initial feature.
- Development builds may continue to expose the same safe diagnostic events over
  an explicitly enabled console, but production use does not depend on console
  output and this feature does not enable release stdio.
