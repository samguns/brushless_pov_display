# Tasks: WS2812 POV Hello Demo

**Input**: Design documents from `/specs/004-ws2812-pov-hello/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/pov-demo-contract.md, quickstart.md

**Tests**: Manual hardware validation scenarios are required by spec and quickstart; no new automated test harness is mandated.

**Organization**: Tasks are grouped by user story so each story can be implemented and validated independently.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare feature-level scaffolding, constants, and validation logs.

- [X] T001 Create validation log scaffold in `specs/004-ws2812-pov-hello/validation.md`
- [X] T002 Define shared feature constants and compile-time bounds in `pov_demo.h` and `ws2812_driver.h`
- [X] T003 [P] Add structured runtime log tags for driver init, transitions, bounded errors, startup latency, and timing-source selection in `pov_leds.cpp`
- [X] T004 Define development debug-output mode (USB stdio enabled for validation, release guard path) in `CMakeLists.txt`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Replace baseline blink path with WS2812-capable output and core playback state model.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T005 Replace single-pin blink PIO program with WS2812 PIO signaling program in `blink.pio`
- [X] T006 Implement/adjust PIO init helper API for WS2812 output stream setup in `blink.pio`
- [X] T007 Create hardware abstraction interface for WS2812 output and bounds-safe frame writes in `ws2812_driver.h`
- [X] T008 [P] Implement WS2812 driver initialization and health state handling in `ws2812_driver.cpp`
- [X] T009 Implement DMA channel configuration and lifecycle handling for LED transfer in `ws2812_driver.cpp`
- [X] T010 Implement DMA -> TX FIFO -> PIO frame transmission path in `ws2812_driver.cpp`
- [X] T011 Create POV demo interface and playback state model in `pov_demo.h`
- [X] T012 [P] Implement POV demo module (Hello metadata, scheduler state, render entry points) in `pov_demo.cpp`
- [X] T013 Implement runtime timing parameter derivation using `clock_get_hz(clk_sys)` in `ws2812_driver.cpp`
- [X] T014 Add timing-budget instrumentation/logging for cadence verification in `pov_leds.cpp`
- [X] T015 Wire driver and demo modules into orchestration loop in `pov_leds.cpp`
- [X] T016 Verify build integration for new modules, generated PIO header usage, and `hardware_dma` linkage in `CMakeLists.txt`

**Checkpoint**: WS2812 output path is available and main loop can push bounded frames.

---

## Phase 3: User Story 1 - WS2812 Output Path Readiness (Priority: P1) 🎯 MVP

**Goal**: Ensure reliable addressable output operation for strips up to 57 LEDs.

**Independent Test**: Boot on hardware with configured strip count <=57 and confirm stable output updates with no lockups.

### Implementation for User Story 1

- [X] T017 [US1] Implement output-path readiness transitions tied to DMA + PIO driver availability in `ws2812_driver.cpp`
- [X] T018 [US1] Enforce active strip count bounds (1..57) with explicit clamp-to-57 behavior in `ws2812_driver.cpp`
- [X] T019 [P] [US1] Emit init success/failure and active LED count observability logs in `pov_leds.cpp`
- [X] T020 [US1] Add bounded fallback behavior when output initialization fails while keeping loop responsive in `pov_leds.cpp`
- [ ] T021 [US1] Record Scenario 1 evidence (readiness and stable updates) in `specs/004-ws2812-pov-hello/validation.md`

**Checkpoint**: Output path is independently functional and bounded.

---

## Phase 4: User Story 2 - POV Hello Playback (Priority: P1)

**Goal**: Display H, e, l, l, o character-by-character at 1.0 second per character and loop continuously.

**Independent Test**: Observe ordered Hello playback with per-character duration in tolerance over multiple cycles.

### Implementation for User Story 2

- [X] T022 [US2] Implement immutable Hello sequence metadata and current-index playback state in `pov_demo.cpp`
- [X] T023 [US2] Implement monotonic timestamp-based transition scheduler at 1000 ms cadence in `pov_demo.cpp`
- [X] T024 [US2] Implement character-to-frame rendering path for H, e, l, l, o in `pov_demo.cpp`
- [X] T025 [P] [US2] Implement auto-start behavior after output readiness and continuous loop wrap from o to H in `pov_demo.cpp`
- [X] T026 [P] [US2] Keep character color/brightness consistent across full playback cycle in `pov_demo.cpp`
- [X] T027 [US2] Log character transition events with timestamp and index metadata in `pov_leds.cpp`
- [ ] T028 [US2] Record Scenario 2/3 evidence (ordering, cadence, clean looping) in `specs/004-ws2812-pov-hello/validation.md`

**Checkpoint**: Hello playback is independently functional and measurable.

---

## Phase 5: User Story 3 - Safe Bounds and Fallback Behavior (Priority: P2)

**Goal**: Preserve responsiveness and safe behavior under invalid LED counts and recoverable output failures.

**Independent Test**: Exercise 0, 1, 57, and >57 count cases and validate bounded handling and recoverable logging.

### Implementation for User Story 3

- [X] T029 [US3] Implement explicit invalid-count handling path for zero/non-operational requests in `ws2812_driver.cpp`
- [X] T030 [US3] Ensure all frame-write loops are bounded by active count and never exceed 57 in `ws2812_driver.cpp`
- [X] T031 [US3] Implement recoverable bounded-error reporting for runtime output unavailability in `pov_leds.cpp`
- [X] T032 [P] [US3] Add health-state exposure/logging for bounded errors and resumed operation in `pov_leds.cpp`
- [ ] T033 [US3] Record Scenario 4 evidence (bounds and fallback behavior) in `specs/004-ws2812-pov-hello/validation.md`

**Checkpoint**: Safe bounds and fallback behavior are independently validated.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final contract alignment, documentation sync, and end-to-end verification.

- [X] T034 [P] Align implemented behavior with contract definitions in `specs/004-ws2812-pov-hello/contracts/pov-demo-contract.md`
- [X] T035 [P] Sync quickstart validation steps with final observed behavior in `specs/004-ws2812-pov-hello/quickstart.md`
- [X] T036 Update static RAM budget with measured byte totals in `specs/004-ws2812-pov-hello/spec.md`
- [X] T037 Run full build verification (`ninja -C build`) and record output in `specs/004-ws2812-pov-hello/validation.md`
- [ ] T038 Measure and record readiness-to-first-H startup latency (SC-005 <= 2s) in `specs/004-ws2812-pov-hello/validation.md`
- [X] T039 Record DMA-backed frame transfer evidence for SC-007 in `specs/004-ws2812-pov-hello/validation.md`
- [X] T040 Record timing-source evidence (`clock_get_hz(clk_sys)`, no hardcoded clock literals) for SC-008 in `specs/004-ws2812-pov-hello/validation.md`
- [ ] T041 Execute 5-minute stability soak and summarize outcomes vs SC-001..SC-008 with residual risks in `specs/004-ws2812-pov-hello/validation.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- Setup (Phase 1): no dependencies.
- Foundational (Phase 2): depends on Setup; blocks all user stories.
- User Story phases (Phase 3-5): depend on Foundational completion.
- Polish (Phase 6): depends on completion of user story phases.

### User Story Dependencies

- US1 (P1): starts immediately after Foundational and forms MVP baseline.
- US2 (P1): depends on US1 output-path readiness to render sequence.
- US3 (P2): depends on US1 bounded configuration and US2 runtime loop context.

### Recommended Delivery Order

1. Phase 1 -> Phase 2
2. US1 (MVP) validation
3. US2 validation
4. US3 validation
5. Phase 6 polish and final verification

---

## Parallel Opportunities

- Phase 1: T003 can run in parallel with T001-T002.
- Phase 2: T008 and T012 can run in parallel after interfaces in T007 and T011 exist.
- US1: T019 can run in parallel with T018 after readiness init structure is in place.
- US2: T025 and T026 can run in parallel after scheduler and frame path (T023-T024).
- US3: T032 can run in parallel with T031 after bounded-error model is defined.
- Polish: T034 and T035 can run in parallel.

---

## Parallel Example: User Story 2

```bash
# After scheduler and frame renderer are in place:
Task T025: Implement auto-start and continuous looping behavior in pov_demo.cpp
Task T026: Implement consistent color/brightness behavior in pov_demo.cpp
```

## Parallel Example: User Story 3

```bash
# After bounded-error model is established:
Task T031: Implement recoverable bounded-error reporting in pov_leds.cpp
Task T032: Add health-state observability logs in pov_leds.cpp
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Complete Setup and Foundational phases.
2. Deliver US1 and validate hardware output-path readiness.
3. Use US1 as baseline before adding Hello playback logic.

### Incremental Delivery

1. Add US2 Hello playback and validate timing/ordering/looping.
2. Add US3 bounded handling and fallback behavior.
3. Complete polish and stability verification.

### Format Validation

All tasks follow required checklist format:
- Checkbox prefix (`- [ ]`)
- Sequential task IDs (`T001` to `T041`)
- `[P]` markers used only for parallelizable tasks
- `[US#]` labels applied to user story tasks
- Explicit file paths included in every task description
