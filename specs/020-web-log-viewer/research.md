# Phase 0 Research: Web Log Viewer

**Feature**: 020-web-log-viewer

The feature specification contains no unresolved clarification markers. This
research resolves the implementation choices that affect timing, memory,
security, and integration with the constrained raw-lwIP portal.

## RES-001 - Capture through an explicit logging API

**Decision**: Add a pure-C `pov_log.h/.c` module with a fixed source enum and an
auditable `pov_logf(source, ...)`-style API. Migrate first-party operational log
calls to this API. Capture is independent of stdio; an optional compile-time
development mirror prints the already sanitized stored entry to USB.

**Rationale**: USB and UART are disabled in production, so stdout cannot be the
source of the in-field history. Explicit calls preserve source identity, work
from both C and C++, and let security review prove which values enter the ring.
The module can be host-tested without lwIP or display hardware.

**Alternatives considered**:

- Tee or intercept global stdout: rejected because release stdio is disabled,
  source identity is unreliable, and arbitrary dependency output or secret
  request bodies could be retained.
- Store history inside `wifi_sta_http`: rejected because producers exist before
  the STA portal starts and logging is not an HTTP responsibility.
- Keep separate console and web messages: rejected because the two diagnostic
  paths would drift and duplicate formatting work.

## RES-002 - Fixed ring layout and memory budget

**Decision**: Retain 128 entries in a statically allocated overwrite-oldest
ring. Each entry is exactly 120 bytes: 64-bit boot-relative milliseconds,
32-bit sequence, 8-bit source, flags and stored length, and 101 text bytes (up
to 100 visible bytes plus NUL, with alignment padding). Ring plus session,
cursor, indices, initialization, and clock-provider state is capped at 15,392
target bytes. Compile-time size assertions enforce both entry size and the
16 KiB feature ceiling.

**Rationale**: This is large enough to retain roughly one minute at the current
normal Hall + clock rate while fitting the RP2040 budget with about 992 bytes of
headroom. Fixed enums avoid repeating source strings in RAM; labels remain in
flash. Formatting scratch remains on the caller stack and HTTP reuses its
existing 16 KiB response buffer.

**Alternatives considered**:

- Heap-backed vector/list: rejected by deterministic-memory requirements.
- Variable-length byte ring: saves space for short lines but makes overwrite,
  corruption recovery, and cursor access substantially more complex.
- Store source labels per entry: rejected because repeated strings consume the
  memory needed for the 128-entry requirement.
- Return all 128 entries at once: rejected because worst-case JSON escaping can
  exceed the 16 KiB response buffer.

## RES-003 - Boot session, ordering, and truncation

**Decision**: Initialize the logger before Wi-Fi setup using a nonzero 64-bit
random boot identifier from Pico SDK `pico_rand`. Sequence numbers start at 1;
timestamps are unsigned 64-bit milliseconds since this boot. The ring exposes a
snapshot plus sequence-based single-entry reads. A sequence wrap starts a new
logical session and clears history. Messages retain valid UTF-8; invalid input
is visibly replaced, and truncation backs up to a code-point boundary, appends a
marker, and sets a flag.

**Rationale**: Sequence alone cannot distinguish reboot from a long disconnect.
A random hex session identifier gives the browser an explicit boundary without
flash writes or dependence on wall-clock calibration. Sequence + session makes
de-duplication, gap detection, and ordered pagination deterministic.

**Alternatives considered**:

- Uptime and sequence reset only: rejected because a reconnect after reboot can
  be mistaken for the prior session.
- Persist a boot counter in flash: rejected due to wear and unnecessary writes.
- Wall-clock timestamps: rejected because time synchronization can occur after
  early boot messages.

## RES-004 - Short polling and bounded batches

**Decision**: Serve `GET /logs` plus short polling at
`GET /logs/updates`. The client issues only one request at a time, waits 1 second
when caught up, and immediately requests the next page while backlog remains.
Responses contain at most 16 entries; `limit=0` returns metadata only. The HTTP
handler serializes into the existing static STA page buffer before its current
asynchronous TCP send begins.

**Rationale**: The raw-lwIP server permits exactly one active PCB and closes each
response. Short requests release that slot so Overview, Settings, reboot, and
updates stay accessible. Sixteen entries fit comfortably even if every stored
byte expands during JSON escaping and bound serialization work per callback.
One-second polling meets the two-second normal visibility target.

**Alternatives considered**:

- Server-Sent Events or WebSockets: rejected because a persistent socket would
  monopolize the only client slot and require new backpressure/lifetime state.
- Long polling: rejected because it still blocks all other portal traffic while
  waiting and gives little benefit at a one-second cadence.
- Unbounded or 128-entry response: rejected due to response-buffer and timing
  risk.

## RES-005 - Cursor, gap, pause, and reconnect behavior

**Decision**: Requests carry optional session + last displayed sequence. Each
response includes current session, uptime, oldest/newest sequence, next cursor,
more/session-changed flags, an optional exact missing range, and ordered entries.
While paused, the browser continues metadata-only polling, keeps its displayed
cursor and scroll position fixed, and derives an unseen count. Resume retrieves
bounded pages from the displayed cursor. The DOM retains at most 128 rows.

**Rationale**: The device already owns a bounded pending history, so pause does
not need another queue. A stale cursor is detectable and produces an honest gap
marker. Session mismatch clears old rows and adds a restart marker, preventing
two boots from being presented as continuous.

**Alternatives considered**:

- Stop all requests while paused: workable, but the user cannot see unseen
  activity or connection/session changes until resuming.
- Fetch and retain an unbounded paused queue in JavaScript: rejected because it
  merely moves the unbounded-memory problem into the browser.
- Add a viewer lease on the device: rejected; one active viewer is sufficient,
  but sequential requests from another browser need no new exclusivity state.

## RES-006 - Security and inert presentation

**Decision**: Use safe-by-construction producer calls and remove the AP server's
existing complete POST-body print before migration. Never retain passwords,
admin/auth tokens, authorization headers, firmware payload data, or complete
request bodies. Apply a bounded case-insensitive defense-in-depth scrub for
known credential key forms before ring commit. JSON escapes quotes, slashes,
controls and HTML-significant bytes; the browser creates nodes and assigns
`textContent`, never entry-derived `innerHTML`.

**Rationale**: Redaction on retrieval is too late because secrets would already
reside in RAM and could leak through another consumer. Call-site allowlisting is
auditable; central scrubbing protects against common accidental regressions.
Correct JSON encoding and inert DOM insertion jointly address malformed or
hostile diagnostic text.

**Alternatives considered**:

- Capture every current `printf` unchanged: rejected because
  `wifi_http.c` currently prints a credential-bearing request body.
- Redact only in the web serializer: rejected because retained state remains
  sensitive.
- Require a new authentication scheme: deferred; the feature intentionally uses
  the same local-network read boundary as the existing portal and returns no
  secrets.

## RES-007 - Console and release behavior

**Decision**: Tie a `POV_LOG_CONSOLE` compile definition to the existing
`POV_DEMO_DEV_USB_STDIO` option. Development builds may mirror safe entries;
release builds compile the mirror out while web capture stays enabled. UART
remains disabled. Do not log successful log-page polling requests.

**Rationale**: Developers keep familiar bench diagnostics without making a
cable a runtime dependency. Compiling the release mirror out prevents USB
format/output cost and satisfies the constitution's release-stdio rule. Omitting
poll access logs avoids a self-sustaining stream.

**Alternatives considered**:

- Enable USB stdio in production: rejected because the spinning board has no
  cable and the constitution requires justification for release stdio.
- Remove console output entirely: rejected because it remains useful during
  controlled bench validation.

## RES-008 - Validation and timing evidence

**Decision**: Add pure host tests for ring and web contract behavior, retain
manual g++ test commands used by the repository, and validate both development
and USB-disabled firmware builds. Record `arm-none-eabi-size` and sorted symbol
output against the current 71,912-byte BSS baseline. On hardware, compare Hall
input and WS2812 column starts for at least 100 revolutions with capture alone
and with one browser polling continuously.

**Rationale**: Host tests can prove ordering, overwrite, cursor, size, escaping,
and redaction deterministically, but cannot prove physical jitter. A logic-
analyzer comparison is required for the sub-microsecond timing and zero-missing-
column acceptance criteria.

**Alternatives considered**:

- Browser-only testing: rejected because it cannot exhaustively force ring and
  escaping boundaries.
- Host-only testing: rejected because it cannot measure PIO/DMA/display timing
  under real Wi-Fi activity.
