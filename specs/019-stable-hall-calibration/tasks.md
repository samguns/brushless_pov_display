# Tasks: Stable Hall Speed Calibration

**Input**: Design documents from `specs/019-stable-hall-calibration/`

**Tests**: Included (synthetic host tests are the primary automated verification).

## Phase 1: Setup

- [X] T001 Record evidence sections in `specs/019-stable-hall-calibration/validation.md`.

## Phase 2: Foundational

- [X] T002 Add smoothing/hysteresis constants and ring-buffer fields to
  `pov_clock_rotation_t` in `pov_clock.h` (window, min samples, outlier/hysteresis
  percents; `period_hist`, `period_sum`, `hist_count`, `hist_head`,
  `smoothed_period_us`).

## Phase 3: US1 - Steady image (P1) MVP

- [X] T003 [US1] Implement the bounded moving average of accepted periods and drive
  `rotation.period_us`/`rpm` from the mean while keeping
  `phase_reference_us` on the real edge in `pov_clock.cpp`.
- [X] T004 [US1] Add a host test asserting >=60% variance reduction on a noisy
  constant-speed stream in `tests/pov_adaptive_rendering_test.cpp`.

## Phase 4: US2 - Prompt response (P2)

- [X] T005 [US2] Add a convergence test: sustained step to a new supported speed
  reaches the new period within the window in `tests/pov_adaptive_rendering_test.cpp`.

## Phase 5: US3 - Outlier rejection (P3)

- [X] T006 [US3] Implement outlier rejection (deviation > outlier percent once
  min-samples reached) in `pov_clock.cpp`.
- [X] T007 [US3] Add outlier-immunity test (short + long injected intervals) in
  `tests/pov_adaptive_rendering_test.cpp`.

## Phase 6: US4 - Hysteresis (P3)

- [X] T008 [US4] Implement enter/exit hysteresis for the stable decision and
  min-sample confidence gating in `pov_clock.cpp`.
- [X] T009 [US4] Update `test_sample_aware_stability` and add a near-threshold
  no-flapping test in `tests/pov_adaptive_rendering_test.cpp`.

## Phase 7: Polish

- [X] T010 Run host tests + `ninja -C build`; record results and fixed-memory delta
  in `specs/019-stable-hall-calibration/validation.md`.

## Dependencies

- T002 precedes T003-T008 (struct/constants).
- T003 (mean) is the MVP; T006 (outlier) and T008 (hysteresis) refine the same
  function; tests follow each.
- Polish follows implementation.
