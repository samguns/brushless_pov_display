# Tasks: Web Log Viewer

**Input**: Design documents from `specs/020-web-log-viewer/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md,
contracts/log-viewer-http.md, quickstart.md

**Tests**: Required by the plan and success criteria. Write focused host tests
before the corresponding implementation, then run firmware/static checks.

**Organization**: Tasks are grouped by user story so each increment remains
independently testable. No task authorizes a git commit, push, issue, or PR.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel with other marked tasks that touch different files
- **[Story]**: User story from spec.md (`US1`, `US2`, `US3`)
- Every task names its concrete file path

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Register the bounded logging module and preserve explicit release
console behavior before feature implementation.

- [X] T001 Add `pov_log.c`, explicit `pico_rand` linkage, and the `POV_LOG_CONSOLE` definition tied to `POV_DEMO_DEV_USB_STDIO` in `CMakeLists.txt`
- [X] T002 [P] Record the pre-feature 71,912-byte BSS baseline and planned <=16 KiB delta checks in `specs/020-web-log-viewer/validation.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Build the safe, current-boot fixed-ring service shared by all three
stories and migrate existing first-party producers.

**CRITICAL**: No web story work begins until the logger is host-tested, bounded,
initialized before Wi-Fi, and free of credential-bearing producer calls.

- [X] T003 [P] Add failing fixed-ring tests for empty state, 1,000 appends, newest-128 overwrite, sequence order, timestamps, source labels, size assertions, session reset, UTF-8-safe truncation, and credential redaction in `tests/pov_log_test.cpp`
- [X] T004 Define the C/C++ logging contract, fixed source enum, exact 120-byte entry, <=15,392-byte store, snapshot/read APIs, and test clock seam in `pov_log.h`
- [X] T005 Implement initialization, bounded formatting, sanitization, UTF-8-safe truncation, overwrite-oldest ring behavior, session/sequence snapshots, single-entry reads, and optional safe console mirroring in `pov_log.c`
- [X] T006 Initialize `pov_log` with a nonzero random boot ID before Wi-Fi and replace application `LOG_*` macros with structured sources in `pov_leds.cpp`
- [X] T007 Migrate safe operational producers, remove complete AP POST-body logging, avoid poll/ISR/per-chunk floods, and add bounded firmware-update transition events in `dhcpserver.c` and `wifi_config/wifi_config.c`, `wifi_config/wifi_flash.c`, `wifi_config/wifi_scan.c`, `wifi_config/wifi_http.c`, `wifi_config/wifi_dns.c`, `wifi_config/wifi_sta_http.c`, and `wifi_config/wifi_firmware_update.c`
- [X] T008 Run `tests/pov_log_test.cpp` and a firmware compile to prove the foundational logger before starting web story work, recording results in `specs/020-web-log-viewer/validation.md`

**Checkpoint**: Safe bounded history captures early and runtime events with USB
disabled, independently of the web UI.

---

## Phase 3: User Story 1 - Watch Live Device Logs Wirelessly (Priority: P1) MVP

**Goal**: Open Logs from the management portal and see retained/current-boot
entries plus new events automatically, in order, without USB.

**Independent Test**: Build page and JSON responses from synthetic entries,
verify navigation/fields/inert rendering/order, then load `/logs` against the
firmware with USB disabled and observe automatic one-second updates.

### Tests for User Story 1

- [X] T009 [P] [US1] Add failing page/JSON tests for Logs navigation, active state, empty state, retained fields, strict order, 16-entry response bound, JSON escaping, `textContent`, one-second sequential polling, and undersized buffers in `tests/wifi_log_viewer_test.cpp`

### Implementation for User Story 1

- [X] T010 [US1] Add the Logs navigation item, responsive console styles, self-contained Logs page, live status badge, bounded row rendering, and automatic short-poll client across `wifi_config/wifi_sta_web.c` and `wifi_config/wifi_log_web.h/.c`
- [X] T011 [US1] Implement bounded log-batch JSON serialization, fixed source projection, message/control/HTML escaping, complete-or-error buffer handling, and at-most-16-entry responses in `wifi_config/wifi_log_web.h/.c`
- [X] T012 [US1] Add exact `GET /logs` and `GET /logs/updates` routing, strict query parsing, shared-buffer response generation, correct Content-Length, `no-store`/`nosniff` headers, and no successful-poll access logging in `wifi_config/wifi_sta_http.h` and `wifi_config/wifi_sta_http.c`
- [X] T013 [US1] Extend existing Overview/Settings/reboot assertions for the inherited Logs link and portal regressions in `tests/wifi_reboot_controls_test.cpp`
- [X] T014 [US1] Run focused web/history tests plus development and USB-disabled firmware builds for the P1 path and record evidence in `specs/020-web-log-viewer/validation.md`

**Checkpoint**: The P1 page is reachable in one action, renders retained history,
and follows new entries without a USB connection or persistent TCP stream.

---

## Phase 4: User Story 2 - Inspect Activity Around a Fault (Priority: P2)

**Goal**: Pause at an older row, see unseen activity, resume ordered catch-up, and
receive an explicit marker when device or browser retention dropped old entries.

**Independent Test**: Fill/overwrite history, pause the synthetic page, generate
new entries, and verify fixed scroll/cursor, metadata-only polling, unseen count,
exact gap, ordered resume, and the 128-row browser cap.

### Tests for User Story 2

- [X] T015 [P] [US2] Extend cursor tests for oldest/newest metadata, initial overwritten history, stale same-session cursor, exact gaps, `limit=0`, `more`, pagination, and zero duplicates in `tests/wifi_log_viewer_test.cpp`

### Implementation for User Story 2

- [X] T016 [US2] Implement initial/same-session/stale-cursor/metadata-only batch semantics and exact missing ranges per the HTTP contract in `wifi_config/wifi_log_web.c`
- [X] T017 [US2] Implement Pause/Resume, fixed displayed cursor and scroll, metadata probes, bounded unseen indication, visible gap/truncation markers, immediate ordered catch-up, and the 128-row DOM cap in `wifi_config/wifi_log_web.c`
- [X] T018 [US2] Run overwrite, pause/resume, responsive-layout, and existing portal regression tests and record P2 evidence in `specs/020-web-log-viewer/validation.md`

**Checkpoint**: Fault history remains honest and bounded before, during, and
after a paused inspection.

---

## Phase 5: User Story 3 - Recover From a Temporary Network Loss (Priority: P3)

**Goal**: Preserve displayed entries through disconnect, automatically retry,
resume the same session without duplicates, and visibly start a new session
after device reboot.

**Independent Test**: Drive the browser state machine through timeout, same-boot
recovery, stale-cursor recovery, and boot mismatch; verify connection labels,
bounded retry, preserved rows, catch-up, and restart separation.

### Tests for User Story 3

- [X] T019 [P] [US3] Extend tests for malformed/future cursors, session mismatch, empty new session, timeout labels, single in-flight request, 1/2/4/5-second retry, preserved DOM, and restart marker behavior in `tests/wifi_log_viewer_test.cpp`

### Implementation for User Story 3

- [X] T020 [US3] Complete session-mismatch and invalid-cursor response semantics, including new-session oldest entry and gap behavior, in `wifi_config/wifi_log_web.c` and `wifi_config/wifi_sta_http.c`
- [X] T021 [US3] Implement 4-second request abort, Connecting/Live/Disconnected transitions, capped retry, same-session resume, boot-change row reset, and explicit restart marker in `wifi_config/wifi_log_web.c`
- [X] T022 [US3] Run disconnect/reconnect/session tests and record P3 evidence plus any hardware-only pending scenarios in `specs/020-web-log-viewer/validation.md`

**Checkpoint**: The viewer never silently presents stale data or merges different
boots after network interruption.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Prove security, memory, timing boundaries, and complete portal
regression across all stories.

- [X] T023 [P] Add worst-case secret-marker, JSON/control/markup/non-ASCII, maximum-batch, and read-only GET regression assertions in `tests/pov_log_test.cpp` and `tests/wifi_log_viewer_test.cpp`
- [X] T024 Audit all in-scope first-party `printf`/log call sites for credentials, authorization, bodies, payloads, ISR/per-column use, and recursive update polling, documenting the result in `specs/020-web-log-viewer/validation.md`
- [X] T025 Run all focused and existing host tests, `ninja -C build`, a USB-disabled release build, `git diff --check`, and response-size checks, recording commands/results in `specs/020-web-log-viewer/validation.md`
- [X] T026 Measure final ELF BSS and sorted symbols, verify logger persistent state <=15,392 bytes and total feature delta <=16 KiB, and record exact values in `specs/020-web-log-viewer/validation.md`
- [X] T027 Review `specs/020-web-log-viewer/quickstart.md` against the implemented routes/commands and record unavailable physical timing, no-USB spinning, and browser hardware gates honestly in `specs/020-web-log-viewer/validation.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 Setup**: Starts immediately.
- **Phase 2 Foundational**: Depends on T001; blocks all user stories.
- **US1 / Phase 3**: Depends on completed bounded logger and producer migration.
- **US2 / Phase 4**: Builds on US1 batch/page transport but remains independently
  testable with synthetic retained history.
- **US3 / Phase 5**: Builds on the cursor/session contract from US1/US2.
- **Phase 6 Polish**: Runs after all stories.

### User Story Dependencies

- **US1 (P1)**: Requires only the foundational logger; this is the MVP.
- **US2 (P2)**: Reuses the US1 page/API and adds pause/gap behavior.
- **US3 (P3)**: Reuses the US1 transport and US2 cursor semantics, then adds
  connection/session recovery.

### Parallel Opportunities

- T002 and T003 touch independent documentation/test files while T001 is prepared.
- T009 can be authored after the `pov_log` public contract is stable while other
  producer migrations are completed.
- T015 and T019 are independent test additions once the base web test exists.
- T023 security tests can be prepared independently of validation documentation.

## Parallel Example: User Story 1

```text
Task T009: Add failing Logs page/JSON contract tests in tests/wifi_log_viewer_test.cpp
Task T013: Extend existing navigation/regression tests in tests/wifi_reboot_controls_test.cpp
```

## Implementation Strategy

### MVP First

1. Complete Setup and Foundational phases.
2. Complete US1 tests and implementation.
3. Validate live bounded history with the USB-disabled build.
4. Continue to pause/gap and reconnect behavior only after the MVP is stable.

### Incremental Delivery

1. Fixed safe logger -> independently host-tested history.
2. US1 -> retained and live wireless logs.
3. US2 -> fault inspection without losing reading position.
4. US3 -> honest recovery across Wi-Fi loss and reboot.
5. Cross-cutting security, memory, build, and hardware-gate documentation.

## Notes

- Mark completed tasks `[X]` only after their named validation succeeds.
- Tests are intentionally written before story implementation.
- Do not log successful `/logs/updates` requests.
- Do not add logs to Hall interrupts, PIO/DMA callbacks, per-column rendering, or
  OTA payload chunks.
- Hardware-only acceptance gates may be recorded as pending when the required
  rotating board, network disruption setup, or logic analyzer is unavailable;
  never claim unexecuted physical evidence.
- Do not commit, push, create issues, or open a PR as part of this workflow.

## Phase 7: Convergence

- [X] T028 Add visible boot-session and uptime metadata, an explicit empty-history state, and per-entry truncation markers to `wifi_config/wifi_log_web.c` per FR-009, FR-010, and contract: GET /logs (partial)
- [X] T029 Add saved dark/light theme bootstrap and an accessible theme toggle to `wifi_config/wifi_log_web.c` per FR-020 and contract: GET /logs (partial)
- [X] T030 Remove the unrequested browser-only Clear view control and its assertion from `wifi_config/wifi_log_web.c` and `tests/wifi_log_viewer_test.cpp` per scoped Viewer State controls (unrequested)


## Phase 8: Convergence

- [X] T031 Validate response session identity and strictly increasing entry sequences before rendering, reject duplicate/reversed batches as disconnected, and format entry uptime as `HH:MM:SS.mmm` in `wifi_config/wifi_log_web.c` and `tests/wifi_log_viewer_test.cpp` per contract: Rendering and ordering (partial)
- [X] T032 Keep displayed cursor and scroll unchanged for same-session metadata/gap responses while paused, show a bounded gap/unseen status without inserting rows, and trigger immediate sequential catch-up on Resume in `wifi_config/wifi_log_web.c` and `tests/wifi_log_viewer_test.cpp` per FR-013 and contract: Pause and resume (partial)

## Phase 11: Clear Display

- [X] T035 Replace the Logs-page Theme button with a client-only Clear control
  that removes displayed rows without changing retained device history, cursor,
  polling, or pause state; cover the generated behavior in
  `tests/wifi_log_viewer_test.cpp` per FR-023.


## Phase 9: Convergence

- [X] T033 Compute truncation after removing legacy trailing CR/LF and add exact 100-byte boundary coverage in `pov_log.c` and `tests/pov_log_test.cpp` per FR-009 and T023 (partial)


## Phase 10: Convergence

- [X] T034 Replace per-byte `snprintf` calls in JSON string escaping with direct bounded writer copies and retain complete-or-error tests in `wifi_config/wifi_log_web.c` per FR-019 and plan: cooperative HTTP timing (partial)

