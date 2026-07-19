# Feature Specification: Full 57-LED POV Coverage

**Feature Branch**: `017-full-led-coverage`

**Created**: 2026-07-12

**Status**: Draft

**Input**: User description: "Use all 57 leds for POV display."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Maximum-Height Clock Image (Priority: P1)

As a viewer of the spinning display, I want the clock digits to span the full
radial extent of the blade so that the numbers are as large and readable as the
hardware allows, instead of appearing as a small band in the middle with dark
gaps at the inner and outer edges.

**Why this priority**: The whole point of the feature is to stop wasting nearly
half of the available LEDs. A taller image is the single largest readability and
brightness improvement the display can make without new hardware.

**Independent Test**: Spin the blade at a supported speed with calibrated time
and confirm that lit pixels of the clock glyphs reach from the innermost active
LED to the outermost active LED, with no permanently dark border rows above or
below the digits.

**Acceptance Scenarios**:

1. **Given** a calibrated clock and a supported steady rotation, **When** the
   normal clock image is rendered, **Then** the rendered glyphs occupy the full
   set of active LEDs from the first through the last, leaving no unused edge
   rows reserved as dark margin.
2. **Given** the display is showing a two-line-tall glyph row pattern, **When**
   a viewer observes the spinning image, **Then** each glyph row is proportionally
   taller than in the previous centered-band rendering while preserving the glyph
   shapes and proportions.
3. **Given** the same clock content as before this feature, **When** rendered,
   **Then** the digit shapes remain recognizable (no distortion that changes which
   digit is shown), only larger.

---

### User Story 2 - Consistent Coverage for Status and Text Patterns (Priority: P2)

As an operator, I want the status/health indicators and any static text patterns
to also use the full LED span so that every display mode looks consistent and no
mode reintroduces the dark inner/outer bands.

**Why this priority**: Status frames and text patterns share the same physical
strip. If only the clock is upgraded, the display would look inconsistent when it
falls back to a status indicator or shows test text.

**Independent Test**: Force each display mode (normal clock, each health/status
fallback, and any static text pattern) and confirm each one lights pixels across
the full active LED span rather than a centered subset.

**Acceptance Scenarios**:

1. **Given** any non-normal health state, **When** the status indicator is
   rendered, **Then** the indicator pattern is distributed across the full active
   LED span.
2. **Given** a static text pattern is displayed, **When** it is rendered, **Then**
   its glyphs use the same full-span vertical mapping as the clock.

---

### User Story 3 - Correct Coverage at Any Active LED Count (Priority: P3)

As a maintainer, I want full-span coverage to be computed from the actual number
of active LEDs so that if the configured LED count is ever reduced below 57, the
image still fills exactly the LEDs that are present without indexing past the end
of the strip.

**Why this priority**: Robustness. The active LED count is a runtime/config value;
the rendering must never assume a hardcoded 57 in a way that overflows the buffer
or leaves gaps when fewer LEDs are active.

**Independent Test**: Configure the active LED count to a value smaller than the
maximum and confirm the rendered image fills exactly the active LEDs with no
out-of-range writes and no dark reserved margin.

**Acceptance Scenarios**:

1. **Given** an active LED count equal to the maximum (57), **When** rendering,
   **Then** all 57 LEDs participate in the image mapping.
2. **Given** an active LED count smaller than the maximum, **When** rendering,
   **Then** the image maps across exactly the active LEDs and never writes beyond
   the active count.

### Edge Cases

- An active LED count that does not divide evenly by the glyph row count must
  still map every LED to a glyph row without leaving unmapped gaps or writing past
  the last LED.
- Rows near the inner and outer edges must remain within the LED buffer; no write
  may exceed the active LED count or the frame buffer length.
- Column blanking (a glyph column with no lit pixels) must still produce a fully
  dark column across all active LEDs, not a partially lit one.
- Reducing the active LED count below the glyph row count must degrade gracefully
  (image compresses) rather than crash or produce out-of-range writes.
- The change must not alter which columns/glyphs are shown, only their vertical
  extent, so the clock content stays correct.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The normal clock image MUST map its glyph rows across the entire set
  of active LEDs so that the first and last active LEDs both participate in the
  rendered image when the corresponding glyph pixels are lit.
- **FR-002**: The system MUST NOT reserve permanently dark margin rows at the inner
  or outer edges of the active LED span during normal clock rendering.
- **FR-003**: Vertical mapping of glyph rows to LEDs MUST be derived from the
  active LED count at render time, not from a fixed assumption of 57 LEDs.
- **FR-004**: Rendering MUST never write to an LED index at or beyond the active
  LED count, nor beyond the frame buffer length, for any active LED count from 1
  through the maximum.
- **FR-005**: Glyph proportions MUST be preserved such that each glyph row occupies
  a contiguous, near-equal share of the active LED span (differences limited to
  unavoidable rounding when the LED count is not an exact multiple of the row
  count).
- **FR-006**: Status/health indicator rendering MUST also cover the full active LED
  span rather than a centered subset.
- **FR-007**: The feature MUST preserve existing clock content, glyph shapes,
  per-field colors, brightness handling, and rotation/phase timing behavior; only
  the vertical LED coverage changes.
- **FR-008**: Rendering MUST remain non-blocking and MUST NOT introduce dynamic
  memory allocation in the display path.
- **FR-009**: A column with no lit glyph pixels MUST render as fully dark across
  all active LEDs.

### Key Entities

- **Active LED Span**: The contiguous set of physically driven LEDs (from index 0
  through active-count minus one) available to carry the rendered image.
- **Glyph Row Mapping**: The relationship that assigns each active LED to one glyph
  row, distributing the fixed number of glyph rows across the full active span.
- **Rendered Column**: The per-angular-position vertical slice whose lit pixels are
  expanded across the active LED span according to the glyph row mapping.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: During normal clock rendering, lit pixels reach both the innermost
  (index 0) and outermost (index active-count minus one) active LEDs whenever the
  corresponding top and bottom glyph rows contain lit pixels.
- **SC-002**: Zero LEDs within the active span are permanently reserved as dark
  margin; every active LED can be lit by some column/row of the image.
- **SC-003**: The rendered clock digit height increases by at least 80% versus the
  previous centered-band rendering (from 30 of 57 LEDs to substantially all 57).
- **SC-004**: For every active LED count from 1 through 57, rendering performs zero
  writes beyond the active count or the frame buffer length.
- **SC-005**: Clock content correctness is unchanged: for a given time value, the
  same digits are displayed as before the feature (verified by comparing lit
  columns), only scaled taller.
- **SC-006**: A sustained run at a supported speed shows the full-height image with
  no crashes, out-of-range writes, or overlapping LED transfers.

## Constitution Compliance

| Principle | Applicability | Compliance |
|---|---|---|
| I. PIO-First LED Drive | Applies | LED bit output stays in the existing DMA-to-PIO path; only the CPU-side frame-buffer fill logic changes. |
| II. Timing Precision | Applies | Column/phase timing is unchanged; the feature only alters which LED rows a column's pixels fill. |
| III. Hardware Abstraction | Applies | The vertical mapping lives in display/rendering logic and remains testable without hardware via the existing host tests. |
| IV. Minimal and Deterministic Memory Use | Applies | Frame buffers remain statically allocated at the existing size; no new buffers or heap use are introduced. |
| V. Single-Command Build and Flash | Applies | Existing build and flash workflows are unchanged. |

## Static RAM Budget

- No new frame buffers are added. The existing static frame buffer (one 32-bit
  word per LED, 57 LEDs) is reused unchanged.
- Any added mapping helper state must use fixed-size scalar locals only; no
  additional persistent buffers are permitted.

## Assumptions

- "All 57 LEDs" means the full radial LED span used by the POV image, i.e. the
  rendered image should cover the complete active LED strip rather than a centered
  subset. Exact edge LEDs may be governed by rounding when the LED count is not a
  multiple of the glyph row count.
- The physical LED strip has 57 addressable pixels arranged radially, consistent
  with the existing `POV_LED_MAX_COUNT` / `POV_CLOCK_LED_ROWS` values.
- Glyph content, font definition, column layout, colors, and rotation timing from
  prior features remain the source of truth; this feature only rescales vertical
  coverage.
- Minor rounding differences in per-row LED height (e.g. some rows one LED taller
  than others) are acceptable and do not count as distortion.
- Host-side unit tests remain the primary automated verification; on-blade visual
  confirmation validates the readability outcome.
