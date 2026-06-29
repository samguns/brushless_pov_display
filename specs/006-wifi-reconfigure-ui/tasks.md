# Tasks: Wi-Fi Reconfiguration UI

**Input**: Design documents from `/specs/006-wifi-reconfigure-ui/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/http-api.md, quickstart.md

**Tests**: Manual hardware validation scenarios are required by the spec and quickstart; no new automated test harness is mandated.

**Organization**: Tasks are grouped by user story so each story can be implemented and validated independently. User stories are ordered by priority: US1 (P1), US2 (P2), US4 (P2), US3 (P3).

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Feature scaffolding and observability.

- [X] T001 Create validation log scaffold in `specs/006-wifi-reconfigure-ui/validation.md`
- [X] T002 [P] Add reconfiguration log tags (page served, scan start/finish, validation reject, apply attempt, persist, revert) without printing the password in `wifi_config/wifi_sta_http.c` and `wifi_config/wifi_config.c`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared API, page, and routing plumbing every user story depends on.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T003 Declare the credential-apply entry point and `ReconfigOutcome` status (`APPLIED`/`REJECTED_VALIDATION`/`FAILED_CONNECT`) in `wifi_config/wifi_config.h`
- [X] T004 [P] Declare the reconfiguration page and result page builder functions in `wifi_config/wifi_sta_web.h`
- [X] T005 Implement the reconfiguration page builder (current SSID display, manual SSID field, masked/empty password field, "Scan" button, submit) in `wifi_config/wifi_sta_web.c`
- [X] T006 Add the `GET /wifi` route that serves the reconfiguration page in `wifi_config/wifi_sta_http.c`
- [X] T007 Wire `POST /config` to URL-decode and parse `ssid`/`password` and dispatch to the apply entry point, replacing the existing stub and removing the admin-token gate in `wifi_config/wifi_sta_http.c`

**Checkpoint**: The reconfiguration page is reachable and submissions route to the apply path.

---

## Phase 3: User Story 1 - Change Wi-Fi Network from the Device UI (Priority: P1) 🎯 MVP

**Goal**: Owner changes the network (manual SSID + password) from the UI and the device connects to and persists the new network.

**Independent Test**: From STA mode, submit a different valid network's SSID/password and confirm the device joins it and reconnects to it after reboot.

### Implementation for User Story 1

- [X] T008 [US1] Implement the credential-apply core: test-connect to the new SSID/password within a bounded timeout and, on success, persist via `save_credentials` (preserving the existing `admin_token`) and refresh runtime SSID/IP in `wifi_config/wifi_config.c`
- [X] T009 [US1] Restart the STA management portal on the new network after a successful change in `wifi_config/wifi_config.c`
- [X] T010 [US1] Implement the success result page (device now on the new network; reconnect client to new network/IP) in `wifi_config/wifi_sta_web.c`
- [X] T011 [US1] Return the success page from `POST /config` on `APPLIED` in `wifi_config/wifi_sta_http.c`
- [X] T012 [US1] Add a "Change Wi-Fi" link to the status page in `wifi_config/wifi_sta_web.c`
- [ ] T013 [US1] Record Scenario 1/2 evidence (discoverability + manual change persists across reboot) in `specs/006-wifi-reconfigure-ui/validation.md`

**Checkpoint**: Manual network change works end-to-end and persists.

---

## Phase 4: User Story 2 - Safe Handling of Bad Credentials (Priority: P2)

**Goal**: A failed change leaves the device reachable on its previous network and reports the failure.

**Independent Test**: Submit a wrong password; confirm the device stays reachable on the previous network and boots on it after a power cycle.

### Implementation for User Story 2

- [X] T014 [US2] Implement revert-on-failure: back up the current working credentials, and on a failed connect reconnect using the backup and restore reachability in `wifi_config/wifi_config.c`
- [X] T015 [US2] Implement the failure result page (change did not take effect, high-level reason) in `wifi_config/wifi_sta_web.c`
- [X] T016 [US2] Return the failure page from `POST /config` on `FAILED_CONNECT` in `wifi_config/wifi_sta_http.c`
- [ ] T017 [US2] Record Scenario 4 evidence (bad credentials revert; boots on previous network) in `specs/006-wifi-reconfigure-ui/validation.md`

**Checkpoint**: Bad credentials never strand the device.

---

## Phase 5: User Story 4 - Pick Network from a Scanned List (Priority: P2)

**Goal**: An explicit "Scan" lists nearby networks; selecting one fills the SSID so only the password is needed.

**Independent Test**: Press "Scan", confirm nearby networks list with secured/open indication, select one, and complete a change entering only the password.

### Implementation for User Story 4

- [X] T018 [US4] Implement `GET /wifi?scan=1` handling: start a scan and bounded-poll to completion while servicing `cyw43_arch_poll()`, then collect results in `wifi_config/wifi_sta_http.c`
- [X] T019 [US4] Render the scanned network list (SSID + secured/open indication, select-to-fill into the SSID field) within the reconfiguration page in `wifi_config/wifi_sta_web.c`
- [X] T020 [P] [US4] Handle the no-networks-found case and ensure manual entry remains available in `wifi_config/wifi_sta_web.c`
- [X] T021 [US4] Ensure scanning does not permanently drop connectivity (recover/restore the link after the scan) in `wifi_config/wifi_sta_http.c`
- [ ] T022 [US4] Record Scenario 3 evidence (scan list, select-to-fill, link recovers) in `specs/006-wifi-reconfigure-ui/validation.md`

**Checkpoint**: Scan-and-select works without stranding the connection.

---

## Phase 6: User Story 3 - Guided, Validated Input (Priority: P3)

**Goal**: Inputs are validated before any radio action, current SSID is shown, and the password is never exposed.

**Independent Test**: Submit empty/over-length values (rejected before any reconnect) and confirm current SSID is shown and password fields are masked/never prefilled.

### Implementation for User Story 3

- [X] T023 [US3] Implement input validation (non-empty SSID ≤ 32 chars; password 8–63 chars) before any disconnect/connect, returning `REJECTED_VALIDATION` in `wifi_config/wifi_sta_http.c`
- [X] T024 [US3] Implement the validation-error result page with a clear message in `wifi_config/wifi_sta_web.c`
- [X] T025 [P] [US3] Ensure the current SSID is shown and the password field is masked and never prefilled across the reconfiguration/result pages in `wifi_config/wifi_sta_web.c`
- [X] T026 [US3] Ensure the password is never rendered or logged in plaintext (status, reconfiguration, result pages, and logs) in `wifi_config/wifi_sta_http.c` and `wifi_config/wifi_config.c`
- [ ] T027 [US3] Record Scenario 5 + confidentiality evidence (validation rejects, no plaintext password) in `specs/006-wifi-reconfigure-ui/validation.md`

**Checkpoint**: Input is validated and credentials stay confidential.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Contract/doc alignment and end-to-end verification.

- [X] T028 [P] Align implemented behavior with the contract in `specs/006-wifi-reconfigure-ui/contracts/http-api.md`
- [X] T029 [P] Sync quickstart validation steps with final observed behavior in `specs/006-wifi-reconfigure-ui/quickstart.md`
- [X] T030 Verify the page buffer holds the reconfiguration page with a full 20-network scan list without truncation (trim row markup if needed) in `wifi_config/wifi_sta_http.c`
- [X] T031 Run full build verification (`ninja -C build`) and record output in `specs/006-wifi-reconfigure-ui/validation.md`
- [ ] T032 Execute an end-to-end manual validation pass across two networks and summarize outcomes vs SC-001..SC-007 with residual risks in `specs/006-wifi-reconfigure-ui/validation.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- Setup (Phase 1): no dependencies.
- Foundational (Phase 2): depends on Setup; blocks all user stories.
- User Story phases (Phases 3-6): depend on Foundational completion.
- Polish (Phase 7): depends on the user story phases.

### User Story Dependencies

- US1 (P1): starts after Foundational; forms the MVP (manual change end-to-end).
- US2 (P2): depends on the US1 apply path (adds revert-on-failure).
- US4 (P2): depends on the Foundational page/route (adds scan + select); independent of US2.
- US3 (P3): depends on the `POST /config` path (adds validation + confidentiality polish).

### Within Each User Story

- API/page declarations before implementations; apply/connection logic before
  result-page wiring; integration before validation-evidence recording.

---

## Parallel Opportunities

- Phase 1: T002 can run in parallel with T001.
- Phase 2: T004 can run in parallel with T003 (different files) before T005-T007.
- US4: T020 can run in parallel with T019 after the scan path (T018) exists.
- US3: T025 can run in parallel with T023/T024 after the page structure exists.
- Polish: T028 and T029 can run in parallel.
- After Foundational, US2 and US4 can be developed in parallel by different people
  (US2 touches connection/result logic; US4 touches scan/list rendering).

---

## Parallel Example: User Story 4

```bash
# After the scan handler (T018) is in place:
Task T019: Render the scanned network list in wifi_config/wifi_sta_web.c
Task T020: Handle no-networks-found + keep manual entry in wifi_config/wifi_sta_web.c
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Complete Setup and Foundational phases.
2. Deliver US1 and validate a manual network change that persists across reboot.
3. Use US1 as the baseline before adding resilience, scanning, and validation.

### Incremental Delivery

1. Add US2 (revert-on-failure) so bad credentials never strand the device.
2. Add US4 (scan + select) for convenience.
3. Add US3 (validation + confidentiality) polish.
4. Complete polish and end-to-end verification.

### Format Validation

All tasks follow the required checklist format:
- Checkbox prefix (`- [ ]`)
- Sequential task IDs (`T001` to `T032`)
- `[P]` markers only on parallelizable tasks
- `[US#]` labels on user story tasks
- Explicit file paths in every task description
