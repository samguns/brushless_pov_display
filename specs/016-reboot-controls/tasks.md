# Tasks: Reboot Controls

**Input**: Design documents from `specs/016-reboot-controls/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md,
contracts/reboot-controls.md, quickstart.md

**Tests**: Focused host tests and hardware/browser validation are included because
the implementation plan requires deterministic page/contract coverage and actual
reset-mode, timing, persistence, and OTA-interlock evidence.

**Organization**: Tasks are grouped by user story so normal reboot is deliverable
as the MVP and blink-frequency removal remains independently testable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel because it changes different files and has no
  dependency on another incomplete task.
- **[Story]**: Maps the task to User Story 1 or User Story 2.
- Every task names the exact file or files it changes or validates.

## Phase 1: Setup (Shared Baseline)

**Purpose**: Capture the current build and memory baseline before changing portal
state or removing telemetry.

- [X] T001 Build the current firmware, capture `arm-none-eabi-size` output and relevant static symbols, and record the pre-change baseline in specs/016-reboot-controls/validation.md

---

## Phase 2: Foundational (Existing Infrastructure)

**Purpose**: Confirm the existing raw-lwIP response streamer, FOTA state machine,
watchdog link, and fixed 16 KiB page buffer are the shared foundation.

No new shared infrastructure is required. User Story 1 extends the established
deferred reboot path, and User Story 2 removes an existing publication chain.

**Checkpoint**: Baseline recorded; both story phases may begin. Coordinate edits
to `wifi_sta_http.*` and `wifi_sta_web.*` if stories are implemented concurrently.

---

## Phase 3: User Story 1 - Reboot the Board Remotely (Priority: P1) MVP

**Goal**: Let an owner confirm one normal board reboot from Settings, receive an
acknowledgement before disconnect, preserve saved settings, and prevent reboot
during an active or committed OTA transition.

**Independent Test**: Open Settings, enter and cancel the confirmation without a
restart, then confirm once and verify acknowledgement, normal restart within five
seconds, reconnection with saved Wi-Fi/brightness, duplicate suppression, and
OTA receive/validate/ready conflicts.

### Tests for User Story 1

- [X] T002 [P] [US1] Add failing host assertions for enabled/disabled Settings reboot controls plus confirmation, accepted, conflict, and undersized-buffer pages in tests/wifi_reboot_controls_test.cpp

### Implementation for User Story 1

- [X] T003 [P] [US1] Add the receiving/validating/ready manual-reboot interlock predicate and terminal-state behavior in wifi_config/wifi_firmware_update.h and wifi_config/wifi_firmware_update.c
- [X] T004 [P] [US1] Add Settings reboot availability rendering and normal confirmation, accepted, and conflict page builders in wifi_config/wifi_sta_web.h and wifi_config/wifi_sta_web.c
- [X] T005 [US1] Replace the USB-only pending boolean with mutually exclusive none/normal/USB targets, implement exact GET/POST `/reboot` responses, and execute deferred normal reset with `watchdog_reboot` in wifi_config/wifi_sta_http.h and wifi_config/wifi_sta_http.c
- [X] T006 [US1] Propagate the OTA interlock to every Settings builder call and enforce `409 Conflict` for stale, duplicate, OTA-busy, or already-pending reboot requests in wifi_config/wifi_sta_http.c
- [X] T007 [US1] Compile and run tests/wifi_reboot_controls_test.cpp, build with `ninja -C build`, and record page sizes and User Story 1 results in specs/016-reboot-controls/validation.md
- [ ] T008 [US1] Execute the cancel, accepted, browser-disconnect, duplicate-POST, normal-mode, five-second timing, OTA-interlock, and saved-settings hardware scenarios from specs/016-reboot-controls/quickstart.md and record evidence in specs/016-reboot-controls/validation.md

**Checkpoint**: Manual reboot works independently, preserves settings, never
enters USB/OTA modes, and remains unavailable during unsafe update states.

---

## Phase 4: User Story 2 - See Only Useful Overview Status (Priority: P2)

**Goal**: Remove synthetic blink frequency from runtime publication, Overview,
and `/status` while preserving Blink Active/Idle and every other existing metric.

**Independent Test**: Render and refresh Overview and fetch `/status`; verify
there is no Blink Frequency label, Hz value, placeholder, parameter, or JSON
member while Blink Active/Idle, clock, RPM, network, address, firmware actions,
theme, and layout retain their current behavior.

### Tests for User Story 2

- [X] T009 [P] [US2] Add failing Overview and status-JSON assertions that retain Blink Active/Idle and existing metrics while rejecting `Blink Frequency`, `frequency_hz`, and Hz placeholders in tests/wifi_reboot_controls_test.cpp

### Implementation for User Story 2

- [X] T010 [P] [US2] Change Blink publication to active-only and remove the runtime frequency field/getter while preserving readiness logging in pov_leds.cpp, wifi_config/wifi_config.h, and wifi_config/wifi_config.c
- [X] T011 [US2] Remove retained blink-frequency state and parameters from runtime-status, Overview, and `/status` calls in wifi_config/wifi_sta_http.h and wifi_config/wifi_sta_http.c
- [X] T012 [US2] Remove the Blink Frequency card and `frequency_hz` JSON member while preserving Blink Active/Idle and other builder output in wifi_config/wifi_sta_web.h and wifi_config/wifi_sta_web.c
- [X] T013 [US2] Run tests/wifi_reboot_controls_test.cpp and `ninja -C build`, inspect `/` and `/status`, and record User Story 2 regression results in specs/016-reboot-controls/validation.md

**Checkpoint**: Blink frequency is absent end-to-end and all protected Overview
and firmware-update behavior remains usable.

---

## Phase 5: Polish & Cross-Cutting Validation

**Purpose**: Prove both story slices coexist without timing, memory, update, or
documentation regressions.

- [X] T014 [P] Update route, status, and public API comments to match the delivered normal reboot and active-only Blink contracts in wifi_config/wifi_sta_http.h, wifi_config/wifi_sta_web.h, and wifi_config/wifi_config.h
- [ ] T015 Run the complete specs/016-reboot-controls/quickstart.md matrix, re-run USB BOOTSEL and Wi-Fi OTA happy paths, and consolidate final evidence in specs/016-reboot-controls/validation.md
- [X] T016 Verify no production-code `blink_frequency_hz`, `blink_hz`, `frequency_hz`, or `Blink Frequency` references remain, confirm static RAM does not increase from the T001 baseline, run `git diff --check`, and record the release gate in specs/016-reboot-controls/validation.md

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1**: Starts immediately and establishes the comparison baseline.
- **Phase 2**: Uses existing infrastructure; completes when the baseline and
  coordination note are acknowledged.
- **User Story 1**: Starts after Phase 1; it is the MVP and does not require User
  Story 2.
- **User Story 2**: Starts after Phase 1 and is independently testable. It does
  not require User Story 1, but concurrent edits to HTTP/web files must be
  coordinated.
- **Polish**: Starts after all desired story phases are complete.

### User Story Dependencies

```text
T001 baseline
|-- US1: T002 + T003 + T004 -> T005 -> T006 -> T007 -> T008
`-- US2: T009 + T010 -> T011 -> T012 -> T013

US1 + US2 -> T014 + T015 -> T016
```

- **US1 (P1)**: Independently delivers safe remote normal reboot.
- **US2 (P2)**: Independently removes obsolete frequency while retaining useful
  readiness and other status.

### Within Each User Story

- Write the marked host assertions first and confirm they fail for the missing
  contract behavior.
- Complete independent update-state/runtime and presentation tasks before route
  integration.
- Complete route/state propagation before host and hardware validation.
- Record measurable evidence at each checkpoint rather than deferring all
  validation to the final phase.

### Parallel Opportunities

- **US1**: T002, T003, and T004 touch tests, FOTA state, and web presentation and
  can run in parallel. T005 follows T003/T004.
- **US2**: T009 and T010 can run in parallel. After T010, HTTP cleanup T011 can
  proceed; coordinate T012 if US1 is still editing `wifi_sta_web.*`.
- **Cross-cutting**: T014 can run alongside hardware/quickstart execution T015
  after both stories stabilize.

## Parallel Example: User Story 1

```text
Task T002: Add failing reboot page-builder assertions in tests/wifi_reboot_controls_test.cpp
Task T003: Add OTA in-progress predicate in wifi_config/wifi_firmware_update.{c,h}
Task T004: Add reboot UI builders in wifi_config/wifi_sta_web.{c,h}
```

## Parallel Example: User Story 2

```text
Task T009: Add failing blink-frequency removal assertions in tests/wifi_reboot_controls_test.cpp
Task T010: Remove application/runtime frequency publication in pov_leds.cpp and wifi_config/wifi_config.{c,h}
```

## Implementation Strategy

### MVP First: User Story 1

1. Record the baseline with T001.
2. Implement T002-T006 for confirmed, deferred, interlocked normal reboot.
3. Validate host/build behavior with T007.
4. Stop and prove actual reset, persistence, and edge cases with T008.

### Incremental Delivery

1. Deliver US1 as the independently demonstrable remote-reboot MVP.
2. Deliver US2 as an independently testable status cleanup.
3. Run cross-cutting comments, complete quickstart regression, memory comparison,
   USB/OTA regression, and release checks.

### Parallel Team Strategy

1. One owner captures T001 and coordinates shared-file sequencing.
2. After baseline, one developer can work on US1 while another begins US2 tests
   and runtime cleanup.
3. Avoid simultaneous uncoordinated edits to `wifi_sta_http.*` and
   `wifi_sta_web.*`; merge completed story checkpoints before final validation.

## Notes

- `[P]` marks only tasks with different files and no incomplete prerequisite.
- `[US1]` and `[US2]` provide direct traceability to the specification stories.
- Historical specifications may still document old blink-frequency behavior;
  T016 applies its removal scan to production code and current feature tests.
- No task may add heap use, another page buffer, a new persistence record, or a
  reset path that bypasses the established deferred response flush.
