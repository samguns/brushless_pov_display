# Tasks: Hall Sensor Rotation Speed

**Input**: Design documents from `/specs/005-hall-sensor-speed/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/hall-sensor-contract.md, quickstart.md

**Tests**: Manual hardware validation scenarios are required by the spec and quickstart; no new automated test harness is mandated.

**Organization**: Tasks are grouped by user story so each story can be implemented and validated independently.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare feature scaffolding, constants, build wiring, and logging.

- [X] T001 Create validation log scaffold in `specs/005-hall-sensor-speed/validation.md`
- [X] T002 Define shared feature constants and compile-time bounds (default pin GP15, debounce interval, magnets-per-revolution, stop timeout, min/max supported speed) in `hall_sensor.h`
- [X] T003 Add `hall_sensor.cpp` to the executable and link `hardware_gpio`, `hardware_irq`, and `pico_sync` in `CMakeLists.txt`
- [X] T004 [P] Add structured runtime log tags for sensor init, periodic reading, stop/stale, and noise/out-of-range events in `pov_leds.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish the driver data model, GPIO/interrupt capture, and atomic read scaffolding that every user story depends on.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T005 Define driver data structures (`HallSensorConfig`, `RotationCapture`, `RotationMeasurement`, `HallSensorState`) and public API declarations in `hall_sensor.h`
- [X] T006 Implement init-defaults and config normalization (magnets-per-rev >= 1; default pin/active level/pull/timeouts) in `hall_sensor.cpp`
- [X] T007 Implement GPIO input configuration (pull and active level) and per-GPIO raw IRQ handler registration that coexists with the CYW43/RM2 GPIO usage in `hall_sensor.cpp`
- [X] T008 Implement the minimal edge interrupt handler: 64-bit microsecond timestamp capture, debounce lockout, and interval/count update in `hall_sensor.cpp`
- [X] T009 Implement an atomic critical-section snapshot of `RotationCapture` for the reader in `hall_sensor.cpp`
- [X] T010 [P] Implement `hall_sensor_deinit` (disable interrupt, release handler, reset state) in `hall_sensor.cpp`
- [X] T011 Wire Hall-sensor initialization into the main loop on GP15 after other peripheral init in `pov_leds.cpp`
- [X] T012 Verify build integration (`ninja -C build`) with the new module and hardware library linkage in `CMakeLists.txt`

**Checkpoint**: Edge capture is active and the main loop can obtain a consistent snapshot of rotation state.

---

## Phase 3: User Story 1 - Read Spinning Speed (Priority: P1) 🎯 MVP

**Goal**: Report spinning speed (RPM and Hz) and rotation period from magnet passes on GP15.

**Independent Test**: Spin the plate at a known steady rate within range and confirm the reported speed matches the reference within ±2%.

### Implementation for User Story 1

- [X] T013 [US1] Implement the pure speed/period derivation function (`period_us = interval_us * magnets_per_rev`; `rpm = 60_000_000 / period_us`; `hz = 1_000_000 / period_us`) in `hall_sensor.cpp`
- [X] T014 [US1] Implement valid-after-two-edges logic so `RotationMeasurement.valid` stays false until at least two edges are captured in `hall_sensor.cpp`
- [X] T015 [US1] Implement `hall_sensor_read(state, now_us) -> RotationMeasurement` combining the atomic snapshot with derivation in `hall_sensor.cpp`
- [X] T016 [P] [US1] Add convenience getters for latest RPM, Hz, and period in `hall_sensor.cpp`
- [X] T017 [US1] Log measured speed, period, and validity from the main loop in `pov_leds.cpp`
- [ ] T018 [US1] Record Scenario 2 evidence (steady-speed accuracy) in `specs/005-hall-sensor-speed/validation.md`

**Checkpoint**: Spinning speed is measurable and accurate, independently demonstrable.

---

## Phase 4: User Story 2 - Continuous Non-Blocking Speed Access (Priority: P2)

**Goal**: Provide a non-blocking, continuously updated reading other modules can query at any time.

**Independent Test**: Query the latest speed/period repeatedly from the loop while spinning; values update and querying never blocks other work.

### Implementation for User Story 2

- [X] T019 [US2] Ensure the read path is non-blocking and O(1) and call `hall_sensor_read` every super-loop iteration in `pov_leds.cpp`
- [X] T020 [US2] Expose freshness/validity (`valid`/`stale` and `last_update_us`) through the read result and getters in `hall_sensor.cpp`
- [X] T021 [P] [US2] Add rate-limited periodic speed logging to avoid log flooding in `pov_leds.cpp`
- [X] T022 [US2] Confirm measurement coexists with Wi-Fi polling and WS2812 output without introducing blocking delays in `pov_leds.cpp`
- [ ] T023 [US2] Record Scenario 1/3 and non-blocking evidence in `specs/005-hall-sensor-speed/validation.md`

**Checkpoint**: Latest speed/period is continuously queryable without blocking the loop.

---

## Phase 5: User Story 3 - Robust Behavior at Boundaries (Priority: P3)

**Goal**: Keep readings safe and bounded when rotation stops, is noisy, or is out of range.

**Independent Test**: Exercise stopped rotation, slow/fast passes, and threshold noise; confirm zero-on-stop, one-count-per-pass, and bounded out-of-range behavior.

### Implementation for User Story 3

- [X] T024 [US3] Implement stop/stale detection (`now_us - last_edge_us > stop_timeout_us` → `rpm = hz = 0`, `stale = true`) in `hall_sensor.cpp`
- [X] T025 [US3] Implement out-of-range bounding for intervals faster than max or slower than min supported speed in `hall_sensor.cpp`
- [X] T026 [P] [US3] Emit stop/stale, resumed-rotation, and out-of-range/noise-rejection transition logs in `pov_leds.cpp`
- [X] T027 [US3] Verify timestamp-wraparound correctness of the 64-bit timebase derivation in `hall_sensor.cpp`
- [ ] T028 [US3] Record Scenario 1 (bench), Scenario 4 (stop), and Scenario 5 (count integrity/stability) evidence in `specs/005-hall-sensor-speed/validation.md`

**Checkpoint**: Boundary and fault behavior is safe, bounded, and validated.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final contract alignment, documentation sync, and end-to-end verification.

- [X] T029 [P] Align implemented behavior with contract definitions in `specs/005-hall-sensor-speed/contracts/hall-sensor-contract.md`
- [X] T030 [P] Sync quickstart validation steps with final observed behavior in `specs/005-hall-sensor-speed/quickstart.md`
- [X] T031 Update the static RAM budget with measured byte totals in `specs/005-hall-sensor-speed/spec.md`
- [X] T032 Run full build verification (`ninja -C build`) and record output in `specs/005-hall-sensor-speed/validation.md`
- [ ] T033 Execute a 5-minute stability soak and summarize outcomes vs SC-001..SC-007 with residual risks in `specs/005-hall-sensor-speed/validation.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- Setup (Phase 1): no dependencies.
- Foundational (Phase 2): depends on Setup; blocks all user stories.
- User Story phases (Phase 3-5): depend on Foundational completion.
- Polish (Phase 6): depends on completion of the user story phases.

### User Story Dependencies

- US1 (P1): starts immediately after Foundational and forms the MVP baseline.
- US2 (P2): depends on US1's read/derivation path to expose continuous access.
- US3 (P3): depends on US1 measurement and US2 runtime loop context.

### Within Each User Story

- Data/derivation before read API; read API before main-loop integration.
- Integration before validation-evidence recording.

---

## Parallel Opportunities

- Phase 1: T004 can run in parallel with T001-T003.
- Phase 2: T010 can run in parallel with T006-T009 after the API in T005 exists.
- US1: T016 can run in parallel with T017 after the read path (T015) exists.
- US2: T021 can run in parallel with T020 after the read result exposes validity.
- US3: T026 can run in parallel with T024-T025 after the bounded-result model is defined.
- Polish: T029 and T030 can run in parallel.

---

## Parallel Example: User Story 1

```bash
# After the read path (T015) is in place:
Task T016: Add convenience getters for RPM/Hz/period in hall_sensor.cpp
Task T017: Log measured speed/period/validity in pov_leds.cpp
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Complete Setup and Foundational phases.
2. Deliver US1 and validate steady-speed accuracy on hardware.
3. Use US1 as the baseline before adding continuous access and robustness.

### Incremental Delivery

1. Add US2 non-blocking continuous access and validate updates/responsiveness.
2. Add US3 stop/stale, out-of-range, and noise robustness.
3. Complete polish and stability verification.

### Format Validation

All tasks follow the required checklist format:
- Checkbox prefix (`- [ ]`)
- Sequential task IDs (`T001` to `T033`)
- `[P]` markers only on parallelizable tasks
- `[US#]` labels on user story tasks
- Explicit file paths in every task description
