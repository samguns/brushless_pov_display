# Feature Specification: Round-Display Cartesian Text Rendering

**Feature Branch**: `018-round-display-rendering`

**Created**: 2026-07-12

**Status**: Draft

**Input**: User description: "The POV display is a rounded display. There are at
most 57 pixels in horizontal and vertical axis. Render text (e.g. 12:34:56) so it
appears upright on the round display."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Upright, Correctly-Shaped Clock Text (Priority: P1)

As a viewer, I want the clock time to appear as upright, correctly proportioned
digits on the round display, instead of digits smeared into curved arcs, so that
the time is actually readable.

**Why this priority**: The current polar rendering (glyph columns drawn as radial
lines) distorts flat text into arcs. Rendering the text in true Cartesian space is
the whole point of the feature.

**Independent Test**: Display a known time and confirm the reconstructed image
(sampled at the device's angular resolution) matches an upright digit layout,
not radial spokes.

**Acceptance Scenarios**:

1. **Given** a calibrated time and supported rotation, **When** the clock renders,
   **Then** the digits appear upright and horizontally arranged across the disc.
2. **Given** the same time value as before this feature, **When** rendered, **Then**
   the digit identities are unchanged (still shows the same numbers), only now
   spatially correct.

---

### User Story 2 - Fits the Round 57-Pixel Display (Priority: P2)

As a viewer, I want the image confined to the circular display area of at most 57
pixels across, so nothing is clipped incorrectly and the layout is centered.

**Why this priority**: The display is physically a disc; content outside the disc
cannot be shown and must be handled cleanly.

**Independent Test**: Confirm the image is centered within the 57-pixel-diameter
circle and pixels mapped outside the disc are dark.

**Acceptance Scenarios**:

1. **Given** the round display, **When** rendering any column, **Then** every LED
   whose mapped Cartesian point lies outside the disc is unlit.
2. **Given** the clock text, **When** rendered, **Then** the text is centered
   within the circle.

---

### User Story 3 - Preserve Timing, Colors, and Safety (Priority: P3)

As a maintainer, I want the new rendering to keep the existing per-field colors,
phase/rotation timing, memory discipline, and transfer safety, so the visual fix
does not regress the rest of the system.

**Why this priority**: The rendering change must not break the proven timing,
color, and transport behavior.

**Independent Test**: Confirm hours/minutes/seconds colors, phase-locked column
selection, no heap use, and no writes past the active LED count.

**Acceptance Scenarios**:

1. **Given** the clock text, **When** rendered, **Then** hours are red, minutes
   green, seconds blue, and separators gray (unchanged).
2. **Given** any column and active LED count, **When** rendering, **Then** no LED
   index at or beyond the active count is written.

### Edge Cases

- A Cartesian pixel mapped outside the disc (radius beyond center) must be dark.
- The device's angular resolution is limited by LED transfer time; text is coarser
  at large radius. This is expected and not a correctness failure.
- Rounding in the Cartesian-to-LED mapping must never index outside the framebuffer
  or the LED buffer.
- Before text is set (no time yet), rendering must produce a blank (all-dark) frame.
- Reducing the active LED count must not cause out-of-range framebuffer sampling.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST build the clock image in a Cartesian (x, y) grid of
  up to 57x57 pixels with the text laid out upright and horizontally.
- **FR-002**: The system MUST map each LED (radius along the spinning line) and each
  angular column to the corresponding Cartesian pixel and output that pixel's color.
- **FR-003**: Pixels whose mapped position lies outside the circular display area
  MUST be rendered dark.
- **FR-004**: The clock text MUST be centered within the circular display.
- **FR-005**: Per-field colors MUST be preserved: hours red, minutes green, seconds
  blue, separators gray.
- **FR-006**: The angular column selection MUST continue to derive from the existing
  rotation phase/period pipeline; this feature changes only how a column's pixels
  are computed.
- **FR-007**: Rendering MUST never write to an LED index at or beyond the active LED
  count, nor sample outside the Cartesian framebuffer bounds.
- **FR-008**: Rendering MUST remain non-blocking with no dynamic allocation; any
  trigonometric tables MUST be precomputed once (not per column) and stored in
  fixed-size storage.
- **FR-009**: When no text is set, rendering MUST produce an all-dark frame.

### Key Entities

- **Cartesian Framebuffer**: A fixed 57x57 grid of per-pixel color/palette values
  holding the upright rendered image.
- **Polar Sampling Map**: The fixed geometric relationship (per angular column) that
  converts an LED's signed radius into a Cartesian sample point.
- **Display Disc**: The inscribed circle (diameter 57) bounding the visible area.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For a given time, the reconstructed image (sampling all columns)
  shows upright digits matching the intended layout, verified against a reference.
- **SC-002**: 100% of LEDs whose mapped Cartesian point is outside the disc are
  dark.
- **SC-003**: For every column (0..N-1) and active LED count (1..57), rendering
  performs zero out-of-range framebuffer or LED-buffer accesses.
- **SC-004**: Per-field colors match the prior convention exactly.
- **SC-005**: No heap allocation is introduced in the render path; trig tables are
  computed once at initialization.

## Constitution Compliance

| Principle | Applicability | Compliance |
|---|---|---|
| I. PIO-First LED Drive | Applies | LED output stays in the DMA-to-PIO path; only CPU-side pixel computation changes. |
| II. Timing Precision | Applies | Column/phase timing pipeline is unchanged; the per-column sample map is precomputed, adding only integer math per LED. |
| III. Hardware Abstraction | Applies | Cartesian rasterization and polar sampling live in the pure renderer, host-testable without hardware. |
| IV. Minimal and Deterministic Memory Use | Applies | Fixed-size static framebuffer and trig tables; no heap; documented RAM delta. |
| V. Single-Command Build and Flash | Applies | `ninja -C build` workflow unchanged. |

## Static RAM Budget

- Adds one fixed 57x57 palette framebuffer (~3.2 KB) and two fixed per-column
  fixed-point trig tables (~160 bytes), allocated statically (file scope), not on
  the stack and not on the heap. Replaces the prior per-column mask/color arrays.

## Assumptions

- **Geometry**: The 57 LEDs span the full diameter of the disc; the center LED is
  at index 28 and each LED i has signed radius (i - 28). The disc diameter is 57
  pixels, matching "at most 57 pixels in horizontal and vertical axis". (If the
  strip is actually a radius, the mapping constant can be adjusted.)
- **Angular resolution**: Remains 40 columns per revolution because WS2812 transfer
  time for 57 LEDs (~1.81 ms) caps columns/revolution to roughly 40 across the
  supported 480-800 RPM envelope. Text is therefore coarser at large radius.
- Font, per-field colors, and rotation/phase timing from prior features are reused.
- This feature supersedes feature 017's radial full-span mapping model; full disc
  coverage is achieved via the Cartesian layout instead.
