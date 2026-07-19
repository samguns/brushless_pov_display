# Research: Full 57-LED POV Coverage

## Current behavior

`pov_clock_renderer.cpp` renders each angular column as a vertical slice using a
fixed centered band:

- `kFontRows = 5`, `kGlyphScaleY = 6` → glyph occupies `5 * 6 = 30` LEDs.
- `kTextTop = (POV_CLOCK_LED_ROWS - kFontRows * kGlyphScaleY) / 2 = (57 - 30) / 2 = 13`.
- `render_current` lights, for each lit glyph row `y`, the LEDs
  `[kTextTop + y*6, kTextTop + y*6 + 6)`, i.e. LED indices 13..42.
- LEDs 0..12 (13 LEDs) and 43..56 (14 LEDs) are never lit → 27 of 57 unused.

`render_status` already iterates `i` over the full active count (`i % 6 < 3`), so
status frames already span all active LEDs; no change is required there for FR-006.

## Decision: active-count-aware row mapping

Replace the fixed band with a mapping that assigns every active LED to a glyph
row:

```
row(led) = (led * kFontRows) / active_count      // integer division, 0..kFontRows-1
```

For each `led` in `[0, min(active_count, frame_len))`:
- Compute `row(led)`.
- If the active column's mask has that row's bit set, set `frame_words[led] = color`.
- Otherwise leave it cleared (the buffer is cleared first).

### Why this approach

- **Uses all active LEDs**: every LED index maps to exactly one glyph row, so no
  edge margin is reserved and the image spans index 0 through `active_count-1`.
- **Adaptive**: derived from `active_led_count` at render time (FR-003), so a
  reduced LED count still fills exactly the present LEDs (US3).
- **Bounded**: the loop upper bound `min(active_count, frame_len)` guarantees no
  out-of-range writes (FR-004).
- **Shape-preserving**: rows remain contiguous and near-equal in height; only
  rounding causes at most a one-LED height difference between rows (FR-005), which
  the spec accepts. The digit shapes (which mask bits are set per column) are
  unchanged, so content correctness holds (FR-007, SC-005).
- **Orientation preserved**: glyph row 0 (top of the glyph array) maps to the
  lowest LED indices, matching the prior `kTextTop + y*scale` ordering.

### Alternatives considered

- **Integer scale bump (`kGlyphScaleY = 11`)**: `5 * 11 = 55` LEDs, band at
  `kTextTop = 1`. Rejected: still leaves 2 dark edge LEDs, still hardcodes 57, and
  does not adapt to a reduced active count.
- **Per-row non-uniform height table (e.g. 12,11,11,11,12)**: fills all 57 exactly
  but hardcodes 57 and needs a lookup table; the division mapping achieves the same
  full coverage generically without a table.
- **Adding a second/scaled frame buffer**: rejected on the Minimal Memory
  principle; no new buffer is needed since we fill the same 57-word buffer.

## Edge-case handling

- `active_count` not a multiple of `kFontRows`: integer division distributes the
  remainder LEDs across the lower rows; every LED is still assigned a valid row.
- `active_count < kFontRows`: `row(led)` still yields a valid `0..kFontRows-1`
  index; the image compresses (some rows unrepresented) but no crash or overflow.
- Blank column (mask == 0): no LED matches a set bit, so the whole column is dark
  (FR-009).
- `frame_len < active_count`: the `min(...)` bound protects the buffer.

## Testability

The mapping is a pure function of `active_led_count` and the per-column mask, so
host tests can assert:
- LED 0 and LED `active_count-1` are lit for a column whose top/bottom rows are set.
- No dark reserved margin (every LED index maps to some row).
- No writes beyond `active_count` (e.g. a canary word after the active region).
- Blank columns produce an all-dark frame.
