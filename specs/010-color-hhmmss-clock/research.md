# Research: Color HHMMSS Clock

## Decision: Remove timezone text from normal display

**Rationale**: The previous 12-character `HH:MM:SS CST` layout consumed most of
the 48 angular columns and produced a dense circular preview. Reducing to the
8-character `HH:MM:SS` format frees columns, increases separation, and keeps the
actual wall-clock value unchanged.

**Alternatives considered**:

- Keep `CST`: preserves explicit timezone but harms readability.
- Alternate `HH:MM:SS` and `CST`: adds temporal ambiguity and extra display
  state for little value.

## Decision: Assign colors by digit group, separators white

**Rationale**: Hours red, minutes green, and seconds blue match the user's
requested visual grouping. White colon separators provide readable boundaries
without being confused with any one group.

**Alternatives considered**:

- Color colons by neighboring group: can make group boundaries less clear.
- Use dim/off colons: saves light output but makes the time harder to parse.

## Decision: Store color per rendered angular column

**Rationale**: The renderer already converts text to per-column masks. Adding a
parallel per-column color table keeps rendering O(1) per active column and avoids
changing the WS2812 driver interface.

**Alternatives considered**:

- Recompute color from character index during frame emission: more bookkeeping
  in the hot render path.
- Store per-pixel color bitmap: simpler frame emission but higher RAM use.

## Decision: Generate preview from the same layout rules

**Rationale**: A local JPEG preview helps validate spacing and colors before
hardware. Matching the firmware glyphs, 48 columns, and 57 radial LEDs makes the
preview useful without requiring hardware.

**Alternatives considered**:

- Hand-drawn preview: faster but can diverge from firmware.
- Hardware-only validation: slower and less convenient for iteration.
