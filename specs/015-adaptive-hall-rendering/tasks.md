# Tasks: Adaptive Hall-Synchronized Rendering

**Input**: Design documents from `specs/015-adaptive-hall-rendering/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md,
contracts/adaptive-rendering-contract.md, quickstart.md

## Phase 1: Setup

- [X] T001 Create build, host-test, memory, and hardware evidence sections in `specs/015-adaptive-hall-rendering/validation.md`
- [X] T002 Create a focused synthetic timing test harness in `tests/pov_adaptive_rendering_test.cpp`

---

## Phase 2: Foundational

- [X] T003 Add Hall phase-reference and sample-generation fields to the measurement contract in `hall_sensor.h`
- [X] T004 [P] Add phase/sample fields to rotation state and inclusive 480-800 RPM/40-column constants in `pov_clock.h`
- [X] T005 [P] Replace free-running renderer schedule fields/API with phase-aware inputs in `pov_clock_renderer.h`
- [X] T006 [P] Add host-testable WS2812 duration arithmetic in `ws2812_timing.h` and transfer-ready fields/API in `ws2812_driver.h`

**Checkpoint**: All public contracts can represent Hall phase, physical samples,
phase-aware scheduling, and true transport readiness.

---

## Phase 3: User Story 1 - Stable Display at Any Supported Speed (Priority: P1) MVP

**Goal**: Render a phase-locked clock at every steady speed from 480 through 800 RPM.

**Independent Test**: Synthetic measurements at 480, 600, 764, and 800 RPM are
suitable; Hall phase maps exact revolution fractions to the expected columns and
returns to column zero after 100 revolutions without cumulative drift.

- [X] T007 [US1] Extend synthetic tests for inclusive boundaries, Hall timestamp/generation propagation, and 100-revolution rational phase mapping in `tests/pov_adaptive_rendering_test.cpp`
- [X] T008 [US1] Populate accepted edge timestamp and edge generation from the atomic Hall snapshot in `hall_sensor.cpp`
- [X] T009 [US1] Copy fresh Hall phase/generation into suitable rotation state in `pov_clock.cpp`
- [X] T010 [US1] Implement rational Hall-edge-anchored phase-to-column mapping with 64-bit arithmetic in `pov_clock_renderer.cpp`
- [X] T011 [US1] Pass phase reference and presentation look-ahead through normal rendering in `pov_leds.cpp`

**Checkpoint**: Stable steady-speed rendering is phase-locked and independently testable.

---

## Phase 4: User Story 2 - Follow Supported Speed Changes (Priority: P2)

**Goal**: Re-anchor on physical samples and recover from supported speed changes or delayed iterations.

**Independent Test**: Repeated reads of one Hall generation do not falsely change
stability; a new generation re-anchors phase, an in-range period change settles
within two revolutions, and a multi-column delay emits only the current column.

- [X] T012 [US2] Add repeated-generation, new-edge re-anchor, period-change, and delayed-column tests in `tests/pov_adaptive_rendering_test.cpp`
- [X] T013 [US2] Make rotation stability history advance only on new Hall sample generations in `pov_clock.cpp`
- [X] T014 [US2] Re-anchor renderer state on new Hall reference/period and skip expired columns without replay in `pov_clock_renderer.cpp`

**Checkpoint**: Supported speed changes and loop delays cannot create persistent drift.

---

## Phase 5: User Story 3 - Safe Unsupported-Speed Behavior (Priority: P3)

**Goal**: Preserve safe fallbacks and prevent overlap through complete LED presentation.

**Independent Test**: 479/801 RPM and stale samples select existing fallbacks;
57-pixel transport remains busy through wire plus latch time; submissions before
ready are rejected and normal output recovers automatically when valid.

- [X] T015 [US3] Add below/above-range, stale/recovery, and frame-duration boundary tests in `tests/pov_adaptive_rendering_test.cpp`
- [X] T016 [US3] Track wire-plus-latch transfer completion and include it in busy/submission checks in `ws2812_driver.cpp`
- [X] T017 [US3] Preserve existing fallback transitions and drop phase-expired frames while transport is busy in `pov_leds.cpp`

**Checkpoint**: Unsupported states remain bounded and no LED transfer overlaps.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T018 Run the focused host test and `ninja -C build`, then record outputs in `specs/015-adaptive-hall-rendering/validation.md`
- [X] T019 Record final fixed-memory structure/symbol sizes and constitution timing obligations in `specs/015-adaptive-hall-rendering/validation.md`
- [X] T020 Reconcile feature 009 rendering documentation and complete software-verifiable quickstart scenarios in `specs/015-adaptive-hall-rendering/validation.md`
- [X] T021 Record pending hardware evidence for 480/600/764/800 RPM, jitter, speed transitions, and 15-minute soak in `specs/015-adaptive-hall-rendering/validation.md`

---

## Dependencies & Execution Order

- Phase 1 has no dependencies.
- Phase 2 follows T002; T004-T006 can proceed in parallel after field naming is stable.
- User Story 1 depends on Phase 2 and is the MVP.
- User Story 2 depends on phase/generation propagation from User Story 1.
- User Story 3 depends on phase-aware main integration and the transport contract.
- Polish follows all software implementation tasks.

## Parallel Examples

```text
Foundation:
- Hall measurement contract in hall_sensor.h
- Rotation contract in pov_clock.h
- Renderer contract in pov_clock_renderer.h
- Transport contract in ws2812_driver.h

After contracts:
- Hall snapshot propagation in hall_sensor.cpp
- Pure renderer phase arithmetic in pov_clock_renderer.cpp
- Driver transfer-ready timing in ws2812_driver.cpp
```

## Implementation Strategy

1. Establish testable phase/sample/transport contracts.
2. Deliver steady in-range phase locking as the P1 MVP.
3. Add generation-aware stability and delayed-loop recovery.
4. Enforce full presentation readiness and fallback recovery.
5. Build, measure fixed memory, and leave hardware-only gates explicit.

## Format Validation

All tasks use checkboxes, sequential IDs, appropriate story labels, and exact
file paths. Test tasks precede their corresponding implementation tasks.
