# Tasks: Full 57-LED POV Coverage

**Input**: Design documents from `specs/017-full-led-coverage/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md,
contracts/full-led-coverage-contract.md, quickstart.md

**Tests**: Included. The spec relies on host unit tests as the primary automated
verification of the mapping contract.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 / US2 / US3

## Phase 1: Setup

- [X] T001 Create build/host-test/memory/hardware evidence sections in `specs/017-full-led-coverage/validation.md` (done during planning; verify current)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Confirm the rendering contract can express full-span coverage using
existing inputs.

- [X] T002 Confirm `pov_clock_renderer_render_current` already receives
  `active_led_count` and `frame_len` in `pov_clock_renderer.h` (no API change
  required); document the internal mapping `row(led) = led*kFontRows/active_count`
  as the design of record in code comments only where non-obvious.

**Checkpoint**: The renderer signature supports an active-count-derived vertical
mapping with no public API change.

---

## Phase 3: User Story 1 - Maximum-Height Clock Image (Priority: P1) MVP

**Goal**: The normal clock image fills the entire active LED span.

**Independent Test**: A column with the top and bottom glyph rows set lights LED 0
and LED `active_count-1`; no reserved dark margin remains; full 57-LED count maps
across all LEDs.

- [X] T003 [US1] Add full-coverage assertions to `tests/pov_adaptive_rendering_test.cpp`:
  for `active_count = 57`, a synthetic column mask with bit 0 and bit 4 set lights
  LED 0 and LED 56; assert no permanently dark reserved margin (every LED index
  maps to a row); assert a mid-row-only mask lights an interior band.
- [X] T004 [US1] Replace the centered-band placement (`kTextTop`, `kGlyphScaleY`)
  in `pov_clock_renderer_render_current` with the full-span mapping
  `row(led) = (led * kFontRows) / N`, `N = min(active_led_count, frame_len)`, in
  `pov_clock_renderer.cpp`; keep the buffer clear-first behavior and per-column
  color unchanged.

**Checkpoint**: The clock image spans all active LEDs and the P1 test passes.

---

## Phase 4: User Story 2 - Consistent Coverage for Status and Text (Priority: P2)

**Goal**: Status indicators and static text patterns also span the full LED span.

**Independent Test**: Status render covers the full active span; the static text
path (which reuses `render_current`) inherits full coverage.

- [X] T005 [US2] Add an assertion in `tests/pov_adaptive_rendering_test.cpp` that
  `pov_clock_renderer_render_status` lights LEDs across the full active span
  (at least one lit LED in the lower third and one in the upper third for 57 LEDs).
- [X] T006 [US2] Verify `render_status` already iterates the full active count and
  requires no change; if any centered assumption exists, correct it in
  `pov_clock_renderer.cpp`.

**Checkpoint**: All display modes use the full LED span consistently.

---

## Phase 5: User Story 3 - Correct Coverage at Any Active LED Count (Priority: P3)

**Goal**: Coverage is computed from the actual active LED count with no overflow.

**Independent Test**: A reduced active count fills exactly the active LEDs; a
canary word after the active region stays zero; blank column renders all-dark.

- [X] T007 [US3] Add bounds/adaptivity assertions to
  `tests/pov_adaptive_rendering_test.cpp`: with `active_count` smaller than 57 and a
  larger `frame_len`, a canary at index `active_count` remains 0 after rendering;
  a blank (mask 0) column yields an all-zero frame; `active_count < kFontRows`
  performs no out-of-range write.
- [X] T008 [US3] Ensure the render loop upper bound is `min(active_led_count,
  frame_len)` and every write index is `< N` in `pov_clock_renderer.cpp`.

**Checkpoint**: Rendering is bounds-safe and adaptive for every active count 1..57.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T009 Compile and run the host test
  (`g++ -std=c++17 -I. tests/pov_adaptive_rendering_test.cpp pov_clock.cpp pov_clock_renderer.cpp -o build/pov_render_test`)
  and `ninja -C build`; record outputs in `specs/017-full-led-coverage/validation.md`.
- [X] T010 Record fixed-memory delta (expected 0 new bytes) and confirm no heap in
  the render path in `specs/017-full-led-coverage/validation.md`.
- [X] T011 Run `git diff --check` for whitespace and note remaining hardware
  observation gates (full-height image, status span) in
  `specs/017-full-led-coverage/validation.md`.

---

## Dependencies & Execution Order

- Phase 1 and Phase 2 have no code dependencies.
- US1 (Phase 3) is the MVP and must precede US2/US3 verification since both reuse
  the `render_current` mapping.
- US3 bounds work (T008) refines the same function edited in T004.
- Polish (Phase 6) follows all implementation.

## Parallel Opportunities

- Test authoring tasks (T003, T005, T007) touch the same test file and should be
  sequential to avoid conflicts.
- T004 and T008 modify the same function in `pov_clock_renderer.cpp`; do T004 then
  fold T008 into the same edit.

## Implementation Strategy

1. Add the P1 coverage tests (fail first).
2. Implement the full-span mapping (T004) to make P1 pass.
3. Confirm status/text coverage (US2) and bounds/adaptivity (US3).
4. Build, run host tests, record validation and memory.
