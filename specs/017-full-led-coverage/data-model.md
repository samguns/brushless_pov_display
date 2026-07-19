# Data Model: Full 57-LED POV Coverage

This feature adds no new persistent data structures. It changes how existing
render inputs are mapped to the existing frame buffer. The conceptual entities are
below.

## Entities

### Active LED Span

- **Represents**: the contiguous set of physically driven LEDs available for the
  image, indices `0 .. active_led_count - 1`.
- **Source**: `active_led_count` argument to the render function (bounded by
  `frame_len` and `POV_LED_MAX_COUNT`).
- **Invariant**: rendering writes only within `[0, min(active_led_count, frame_len))`.

### Glyph Row Mapping

- **Represents**: assignment of each active LED to one of `kFontRows` (5) glyph
  rows.
- **Rule**: `row(led) = (led * kFontRows) / active_led_count`, integer division.
- **Properties**:
  - Every LED index in the active span maps to exactly one row in `0..kFontRows-1`.
  - Rows are contiguous; row height differs by at most one LED due to rounding.
  - Monotonic: `row(led)` is non-decreasing in `led`, preserving glyph orientation.

### Rendered Column

- **Represents**: the currently active angular column's vertical pixel slice.
- **Source**: `renderer->column_masks[active_column]` (5-bit mask) and
  `renderer->column_colors[active_column]` (GRB word), both unchanged by this
  feature.
- **Rule**: `frame_words[led] = color` when `mask & (1 << row(led))`, else `0`.

## Relationships

```
active_led_count ──▶ Glyph Row Mapping (row per LED)
column mask/color ─▶ Rendered Column
Glyph Row Mapping + Rendered Column ──▶ frame_words[0 .. active_led_count-1]
```

## Non-changes

- Column count (`POV_CLOCK_COLUMNS = 40`), glyph font, `POV_CLOCK_TEXT_*`, colors,
  brightness scaling, rotation/phase state, and the WS2812 transport are unchanged.
- `POV_LED_MAX_COUNT = 57` and `POV_CLOCK_LED_ROWS = 57` are unchanged; the value
  57 is no longer used to compute a centered offset.
