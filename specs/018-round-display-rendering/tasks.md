# Tasks: Round-Display Cartesian Text Rendering

**Input**: Design documents from `specs/018-round-display-rendering/`

**Tests**: Included (host unit tests are the primary automated verification).

## Phase 1: Setup

- [X] T001 Confirm the 57x57 geometry and 40-column cap in `research.md` and record
  evidence sections in `specs/018-round-display-rendering/validation.md`.

## Phase 2: Foundational

- [X] T002 Update `pov_clock_renderer_t` in `pov_clock_renderer.h`: remove
  `column_masks`/`column_colors`; keep text, phase fields, and `text_ready`.

**Checkpoint**: Renderer state reflects the Cartesian model.

## Phase 3: User Story 1 - Upright Text (P1) MVP

- [X] T003 [US1] Implement a Cartesian rasterizer in `pov_clock_renderer.cpp` that
  draws the text upright/centered into a file-static 57x57 palette framebuffer using
  the existing font (scaled sx=2, sy=3).
- [X] T004 [US1] Precompute per-column fixed-point `cos256`/`sin256` (Q8) once at
  init in `pov_clock_renderer.cpp`.
- [X] T005 [US1] Reimplement `render_current` to sample the framebuffer via the
  polar map (`dx=i-28`, rounded projection) and output palette colors.

**Checkpoint**: Clock text renders upright (verified by reconstruction preview).

## Phase 4: User Story 2 - Fits the Disc (P2)

- [X] T006 [US2] Mask pixels outside the disc / framebuffer bounds to dark in
  `render_current`; center the text in the framebuffer during rasterization.

## Phase 5: User Story 3 - Preserve Timing/Colors/Safety (P3)

- [X] T007 [US3] Preserve per-field colors via the palette and keep
  `pov_clock_renderer_step`/phase behavior unchanged in `pov_clock_renderer.cpp`.
- [X] T008 [US3] Rewrite renderer tests in `tests/pov_adaptive_rendering_test.cpp`
  for bounds safety, disc masking, palette colors, blank-when-no-text, and
  determinism (replacing the old mask-based coverage test).

## Phase 6: Polish

- [X] T009 Add a 40-column reconstruction mode to `tools/pov_preview.py` and
  regenerate the preview.
- [X] T010 Run host tests + `ninja -C build`; record results and fixed-memory delta
  in `specs/018-round-display-rendering/validation.md`.

## Dependencies

- T002 precedes T003-T007 (struct change).
- US1 (T003-T005) is the MVP; US2/US3 refine the same function.
- Polish follows implementation.
