# Tasks: Color HHMMSS Clock

**Input**: Design documents from `specs/010-color-hhmmss-clock/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Build validation, JPEG preview inspection, and manual hardware validation when available.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Confirm current clock renderer state and feature scope before edits.

- [x] T001 Verify `.gitignore` still covers C/C++ build outputs in `.gitignore`
- [x] T002 [P] Review current `pov_clock.h`, `pov_clock.cpp`, `pov_clock_renderer.h`, `pov_clock_renderer.cpp`, and `pov_leds.cpp` against `specs/010-color-hhmmss-clock/plan.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Adjust shared clock text constants and renderer state to support color-per-column output.

- [x] T003 Update clock text length constants for `HH:MM:SS` in `pov_clock.h`
- [x] T004 Update CST clock formatting to emit `HH:MM:SS` without timezone label in `pov_clock.cpp`
- [x] T005 Add per-column color storage and render API changes in `pov_clock_renderer.h`

**Checkpoint**: Core data shapes support the shorter colored clock layout.

---

## Phase 3: User Story 1 - Read a Simpler Clock Format (Priority: P1) MVP

**Goal**: Normal clock display shows only `HH:MM:SS`.

**Independent Test**: Preview and debug output show exactly `HH:MM:SS`, with no `CST` label.

- [x] T006 [US1] Remove CST glyph handling and lay out only `HH:MM:SS` in `pov_clock_renderer.cpp`
- [x] T007 [US1] Update clock logs and renderer text refresh behavior in `pov_leds.cpp` to use the shorter text

**Checkpoint**: Time text is simplified without changing time calibration.

---

## Phase 4: User Story 2 - Distinguish Time Components by Color (Priority: P1)

**Goal**: Hours render red, minutes green, seconds blue, and separators neutral.

**Independent Test**: Generated preview shows `HH` red, `MM` green, `SS` blue, and colons white.

- [x] T008 [US2] Build per-column color mapping for hours, minutes, seconds, and colons in `pov_clock_renderer.cpp`
- [x] T009 [US2] Render current clock columns using stored per-column colors in `pov_clock_renderer.cpp`
- [x] T010 [US2] Remove the single global normal-clock color from `pov_leds.cpp`

**Checkpoint**: Normal display is color-coded by time component.

---

## Phase 5: User Story 3 - Review Before Hardware Run (Priority: P2)

**Goal**: Provide a JPEG preview of the simplified colored layout.

**Independent Test**: Open `specs/010-color-hhmmss-clock/color_hhmmss_preview.jpg` and confirm the expected format and colors.

- [x] T011 [US3] Generate `specs/010-color-hhmmss-clock/color_hhmmss_preview.jpg` using the firmware layout rules
- [x] T012 [US3] Update `specs/010-color-hhmmss-clock/quickstart.md` with preview and build validation results

**Checkpoint**: The visual change can be reviewed before hardware execution.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Validate build and Spec Kit consistency.

- [x] T013 Run `ninja -C build` and fix any compile errors
- [x] T014 Update static RAM notes in `specs/010-color-hhmmss-clock/plan.md`
- [x] T015 Validate no stale `HH:MM:SS CST` assumptions remain in 010 artifacts or clock source files
- [x] T016 Refresh managed agent context files with `specs/010-color-hhmmss-clock/plan.md`

---

## Dependencies & Execution Order

- Phase 1 precedes all code changes.
- Phase 2 blocks all user stories.
- US1 and US2 are both P1; US1 should complete before US2 because color assignment follows the new shorter text layout.
- US3 depends on US1 and US2.
- Polish runs last.

## Parallel Opportunities

- T002 can run in parallel with T001.
- Tasks touching `pov_clock_renderer.cpp` should run sequentially.
- Preview generation can proceed after renderer behavior is implemented and does not require hardware.

## Implementation Strategy

1. Update text constants and formatting.
2. Simplify renderer layout to `HH:MM:SS`.
3. Add per-column color mapping and rendering.
4. Generate the JPEG preview.
5. Build, update docs, refresh context, and stop without committing.

## Validation Notes

- No git commit is part of this procedure.
- Hardware validation remains manual unless the board and motor are available.
