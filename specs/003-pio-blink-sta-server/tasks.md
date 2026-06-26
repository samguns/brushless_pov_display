# Tasks: PIO Blink Concurrent with STA HTTP Server

**Input**: Design documents from `/specs/003-pio-blink-sta-server/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/http-api.md, quickstart.md

**Tests**: Manual hardware validation is required by spec and quickstart; no new automated test harness is required.

**Organization**: Tasks are grouped by user story so each story can be implemented and validated independently.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare shared constants, validation scaffold, and runtime logging conventions.

- [ ] T001 Create validation evidence log scaffold in `specs/003-pio-blink-sta-server/validation.md`
- [ ] T002 Define reconnect/auth-throttle constants in `wifi_config/wifi_sta_http.h`
- [ ] T003 [P] Add structured FR-009 debug log macros for connected/disconnected, active IP, and blink state in `wifi_config/wifi_config.c`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Implement non-blocking runtime architecture and shared persistence/auth primitives.

**CRITICAL**: No user story implementation should begin until this phase is complete.

- [ ] T004 Add non-blocking runtime API declarations (`wifi_config_sta_runtime_init`, `wifi_config_runtime_step`) in `wifi_config/wifi_config.h`
- [ ] T005 Implement runtime state machine transitions (CONNECTED, DISCONNECTED, RECONNECTING, AP_FALLBACK) in `wifi_config/wifi_config.c`
- [ ] T006 Implement superloop orchestration (lwIP poll + wifi runtime step + blink step) in `pov_leds.cpp`
- [ ] T007 [P] Extend credential schema for persisted admin token and versioning in `wifi_config/wifi_flash.h`
- [ ] T008 Implement backward-compatible admin token load/save migration in `wifi_config/wifi_flash.c`
- [ ] T009 [P] Implement shared token-check and invalid-attempt throttle helpers in `wifi_config/wifi_sta_http.c`
- [ ] T010 Ensure build wiring for STA runtime modules and includes is correct in `CMakeLists.txt`

**Checkpoint**: Foundation complete; user stories can be implemented and validated independently.

---

## Phase 3: User Story 1 - Concurrent Blink and Web Access (Priority: P1)

**Goal**: Keep PIO blinking stable while serving repeated STA web requests.

**Independent Test**: In STA mode, issue repeated `GET /` and `GET /status` for 60 seconds and confirm no visible blink freezes.

### Implementation for User Story 1

- [ ] T011 [US1] Define explicit blink runtime state fields (`enabled`, `frequency_hz`, `next_toggle_us`) in `pov_leds.cpp`
- [ ] T012 [US1] Use runtime clock-derived blink calculations and frequency bounds validation in `pov_leds.cpp`
- [ ] T013 [US1] Keep `GET /` and `GET /status` handling non-blocking under repeated request load in `wifi_config/wifi_sta_http.c`
- [ ] T014 [P] [US1] Render connectivity and blink-active status data for STA pages/JSON in `wifi_config/wifi_sta_web.c`
- [ ] T015 [US1] Record Scenario 1 results and request success ratio evidence in `specs/003-pio-blink-sta-server/validation.md`

**Checkpoint**: User Story 1 is functional and independently testable.

---

## Phase 4: User Story 2 - Stable Blink During Network Events (Priority: P1)

**Goal**: Preserve blink continuity through WiFi drops/reconnects and recover HTTP automatically.

**Independent Test**: Drop and restore WiFi while running STA service; verify uninterrupted blinking and HTTP recovery within 20 seconds after connectivity returns.

### Implementation for User Story 2

- [ ] T016 [US2] Implement reconnect transition handling and link-state polling in `wifi_config/wifi_config.c`
- [ ] T017 [P] [US2] Expose runtime state/accessors for connectivity and portal readiness in `wifi_config/wifi_config.h`
- [ ] T018 [US2] Restart or resume STA listener on reconnect success without reboot in `wifi_config/wifi_sta_http.c`
- [ ] T019 [US2] Ensure reconnect attempts do not block blink scheduling path in `pov_leds.cpp`
- [ ] T020 [US2] Record Scenario 2 evidence including 3 reconnect flaps in 60 seconds and listener restart recovery in `specs/003-pio-blink-sta-server/validation.md`

**Checkpoint**: User Story 2 is functional and independently testable.

---

## Phase 5: User Story 3 - Preserve Existing Provisioning Behavior (Priority: P2)

**Goal**: Keep AP provisioning behavior intact while supporting admin-token lifecycle and automatic transition to concurrent STA runtime.

**Independent Test**: Clear credentials, provision SSID/password/token through AP flow, reboot to STA mode, verify concurrent blink + HTTP behavior.

### Implementation for User Story 3

- [ ] T021 [US3] Parse and validate admin token input in provisioning POST handling in `wifi_config/wifi_http.c`
- [ ] T022 [P] [US3] Add/update admin token form fields and explanatory copy in `wifi_config/wifi_web.c`
- [ ] T023 [US3] Persist token updates through existing credential save flow in `wifi_config/wifi_flash.c`
- [ ] T024 [US3] Ensure provisioning completion transitions into STA runtime init path in `wifi_config/wifi_config.c`
- [ ] T025 [US3] Record Scenario 4 regression evidence for AP provisioning compatibility in `specs/003-pio-blink-sta-server/validation.md`

**Checkpoint**: User Story 3 is functional and independently testable.

---

## Phase 6: Security Contract Compliance (Auth + Throttling)

**Purpose**: Satisfy FR-011 to FR-014 endpoint behavior for mutating operations.

- [ ] T026 Enforce token requirement on mutating endpoints (`POST /config`, `POST /update`) with 401 JSON on missing/invalid token in `wifi_config/wifi_sta_http.c`
- [ ] T027 [P] Implement invalid-attempt throttle window and 429 JSON responses in `wifi_config/wifi_sta_http.c`
- [ ] T028 Preserve unauthenticated access for read-only endpoints (`GET /`, `GET /status`) in `wifi_config/wifi_sta_http.c`
- [ ] T029 Record Scenario 3 auth/throttle evidence including valid-token success during throttle events in `specs/003-pio-blink-sta-server/validation.md`

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final alignment, verification, and delivery summary.

- [ ] T030 [P] Align implemented status/auth behavior with contract details in `specs/003-pio-blink-sta-server/contracts/http-api.md`
- [ ] T031 [P] Align validation instructions and expected outcomes with implemented behavior in `specs/003-pio-blink-sta-server/quickstart.md`
- [ ] T032 Run build verification (`ninja -C build`) and capture output evidence in `specs/003-pio-blink-sta-server/validation.md`
- [ ] T033 Summarize completion status, measurable outcomes, and residual risks in `specs/003-pio-blink-sta-server/validation.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- Phase 1 (Setup): no dependencies.
- Phase 2 (Foundational): depends on Phase 1 and blocks all user stories.
- Phase 3 (US1), Phase 4 (US2), Phase 5 (US3): depend on Phase 2.
- Phase 6 (Security): depends on token persistence and endpoint routing from Phases 2-5.
- Phase 7 (Polish): depends on completion of Phases 3-6.

### User Story Dependencies

- US1 (P1): starts immediately after Foundational; delivers MVP value.
- US2 (P1): starts after Foundational; coordinates with US1 where shared files overlap.
- US3 (P2): starts after Foundational and depends on credential/token persistence primitives.

### Recommended Story Completion Order

1. US1 (MVP)
2. US2
3. US3

---

## Parallel Opportunities

- Setup: T003 can run in parallel with T001-T002.
- Foundational: T007 and T009 can run in parallel after API shape in T004 is established.
- US1: T014 can run in parallel with T013 after T011 scaffolding.
- US2: T017 can run in parallel with T016.
- US3: T022 can run in parallel with T021.
- Security: T027 can run in parallel with T026 once mutating routes are identified.
- Polish: T030 and T031 can run in parallel.

---

## Parallel Example: User Story 1

```bash
# After blink runtime state scaffolding (T011)
Task T013: Keep / and /status request handling non-blocking in wifi_config/wifi_sta_http.c
Task T014: Render status payload/page fields in wifi_config/wifi_sta_web.c
```

## Parallel Example: User Story 2

```bash
# After foundational runtime APIs are in place
Task T016: Implement connectivity transitions in wifi_config/wifi_config.c
Task T017: Expose connectivity/portal accessors in wifi_config/wifi_config.h
```

## Parallel Example: User Story 3

```bash
# After token persistence primitives are available
Task T021: Parse/validate admin token in wifi_config/wifi_http.c
Task T022: Update AP provisioning form fields in wifi_config/wifi_web.c
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Complete Phase 1 and Phase 2.
2. Complete US1 tasks (T011-T015).
3. Validate Scenario 1 before moving to next stories.

### Incremental Delivery

1. Deliver US1 and validate.
2. Deliver US2 and validate.
3. Deliver US3 and validate.
4. Complete security compliance tasks and validate.
5. Finish polish tasks and final build verification.

### Format Validation

All tasks follow required checklist format:
- Checkbox prefix (`- [ ]`)
- Sequential task IDs (`T001` to `T033`)
- `[P]` markers used only for parallelizable tasks
- `[US#]` labels applied to user story tasks
- Explicit file paths included in every task description
