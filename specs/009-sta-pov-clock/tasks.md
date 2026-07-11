# Tasks: STA POV Clock Display

**Input**: Design documents from `specs/009-sta-pov-clock/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Build validation plus manual device scenarios from quickstart.md; pure logic is kept isolated for synthetic timestamp checks during implementation.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Verify project hygiene and add the feature files to the build in a minimal way.

- [x] T001 Verify C/C++ ignore patterns in `.gitignore`
- [x] T002 [P] Review `hall_sensor.*`, `ws2812_driver.*`, and `pov_leds.cpp` integration points against `specs/009-sta-pov-clock/plan.md`
- [x] T003 Add empty module shells for `time_sync.h`, `time_sync.cpp`, `pov_clock.h`, `pov_clock.cpp`, `pov_clock_renderer.h`, and `pov_clock_renderer.cpp`
- [x] T004 Add the new feature sources to `CMakeLists.txt`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Add shared constants, state contracts, and frame helpers used by all clock stories.

- [x] T005 Define POV clock speed, column-count, CST offset, and status enums in `pov_clock.h`
- [x] T006 Implement bounded CST conversion and clock text formatting helpers in `pov_clock.cpp`
- [x] T007 Define renderer state and 57-word frame output contract in `pov_clock_renderer.h`
- [x] T008 Implement blank/status frame helpers in `pov_clock_renderer.cpp`

**Checkpoint**: Foundation ready - time, rotation, and rendering stories can build on the same state model.

---

## Phase 3: User Story 1 - Calibrate CST Time in STA Mode (Priority: P1) MVP

**Goal**: Calibrate UTC time in STA mode, convert to UTC+8, and expose a fresh CST wall-clock value.

**Independent Test**: Boot in STA mode with network access and confirm debug output reports a CST value within +/-1 second of a trusted UTC+8 reference.

- [x] T009 [US1] Define non-blocking network time calibration state and public API in `time_sync.h`
- [x] T010 [US1] Implement raw lwIP DNS and UDP NTP request lifecycle in `time_sync.cpp`
- [x] T011 [US1] Implement NTP response validation and UTC epoch extraction in `time_sync.cpp`
- [x] T012 [US1] Implement calibrated UTC-to-CST state update in `pov_clock.cpp`
- [x] T013 [US1] Initialize and step time calibration from `pov_leds.cpp`
- [x] T014 [US1] Log calibration success, failure, retry, and CST second transitions in `pov_leds.cpp`

**Checkpoint**: Time can be calibrated and advanced independently of rotation/display.

---

## Phase 4: User Story 2 - Establish a Stable POV Spin Target (Priority: P1)

**Goal**: Classify Hall-sensor speed around the 600 RPM nominal target and expose suitability for rendering.

**Independent Test**: Spin at 600 RPM and confirm the device reports suitable; vary below 480 RPM and above 800 RPM and confirm unsuitable states.

- [x] T015 [US2] Implement rotation suitability derivation from `hall_rotation_measurement_t` in `pov_clock.cpp`
- [x] T016 [US2] Track recent rotation-period changes for basic stability classification in `pov_clock.cpp`
- [x] T017 [US2] Integrate rotation suitability evaluation into the main loop in `pov_leds.cpp`
- [x] T018 [US2] Log RPM, nominal target, range, and suitability transitions in `pov_leds.cpp`

**Checkpoint**: Rotation suitability can be validated without requiring normal clock rendering.

---

## Phase 5: User Story 3 - Display Current Time with POV (Priority: P1)

**Goal**: Render the current CST time as a compact 40-column POV clock on the spinning 57-LED row.

**Independent Test**: With calibrated time and suitable rotation, observe `HH:MM:SS CST` for at least one minute and confirm the seconds change once per second.

- [x] T019 [US3] Add compact glyph table for digits, colon, space, C, S, and T in `pov_clock_renderer.cpp`
- [x] T020 [US3] Implement `HH:MM:SS CST` text layout into 40 angular columns in `pov_clock_renderer.cpp`
- [x] T021 [US3] Implement measured-period column scheduling in `pov_clock_renderer.cpp`
- [x] T022 [US3] Render one 57-word column frame from renderer state in `pov_clock_renderer.cpp`
- [x] T023 [US3] Replace Hello-demo playback with clock renderer orchestration in `pov_leds.cpp`
- [x] T024 [US3] Submit clock column frames through `ws2812_driver_submit_frame` in `pov_leds.cpp`
- [x] T025 [US3] Refresh rendered clock text exactly when the CST second changes in `pov_leds.cpp`

**Checkpoint**: The board displays current CST time when time and rotation are valid.

---

## Phase 6: User Story 4 - Recover from Time or Rotation Problems (Priority: P2)

**Goal**: Leave normal clock mode when time or rotation is invalid and present bounded non-clock status behavior.

**Independent Test**: Exercise no network time, stopped rotation, slow rotation, fast rotation, and unstable rotation; confirm no stale normal clock is shown.

- [x] T026 [US4] Implement display health state derivation in `pov_clock.cpp`
- [x] T027 [US4] Render bounded non-clock status patterns for invalid health states in `pov_clock_renderer.cpp`
- [x] T028 [US4] Switch between normal clock and status rendering in `pov_leds.cpp`
- [x] T029 [US4] Log display health transitions and delayed DMA submissions in `pov_leds.cpp`

**Checkpoint**: Invalid time or rotation conditions no longer show a normal clock image.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Validate, document memory impact, and keep the feature aligned with Spec Kit artifacts.

- [x] T030 Update static RAM notes with measured/estimated feature memory in `specs/009-sta-pov-clock/plan.md`
- [x] T031 Run `ninja -C build` and record the result in `specs/009-sta-pov-clock/quickstart.md`
- [x] T032 Validate task/spec/plan consistency against `specs/009-sta-pov-clock/contracts/pov-clock-contract.md`
- [x] T033 Refresh managed agent context files with `specs/009-sta-pov-clock/plan.md`
- [x] T034 Record hardware validation status for time calibration, 600 RPM readability, and invalid-state scenarios in `specs/009-sta-pov-clock/quickstart.md`

---

## Dependencies & Execution Order

- Phase 1 precedes all implementation.
- Phase 2 blocks all user stories.
- US1, US2, and US3 are all P1, but implementation order is US1 -> US2 -> US3 because display rendering depends on time and rotation state.
- US4 depends on US1 and US2 status inputs and integrates after normal display behavior exists.
- Polish runs after all user stories.

## Parallel Opportunities

- T002 can run in parallel with T001.
- After T005 defines shared constants, T006 and T007 can be developed independently.
- Time calibration internals in T010/T011 are isolated from renderer work, but this implementation should complete US1 before US3 to keep validation simple.
- Tasks touching `pov_leds.cpp` should run sequentially to avoid conflicting orchestration edits.

## Implementation Strategy

1. Add build-visible module shells and shared state contracts.
2. Complete US1 as the MVP: STA time calibration and CST conversion.
3. Complete US2: rotation suitability around 600 RPM.
4. Complete US3: normal POV clock rendering.
5. Complete US4: safe invalid-state behavior.
6. Build, update validation notes, and refresh agent context.

## Validation Notes

- Do not commit changes as part of this task sequence.
- Hardware scenarios from `quickstart.md` remain manual unless hardware is attached.
