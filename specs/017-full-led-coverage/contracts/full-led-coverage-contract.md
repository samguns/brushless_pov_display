# Contract: Full-Span Column Rendering

Applies to `pov_clock_renderer_render_current(renderer, frame_words, frame_len,
active_led_count)` and, for span coverage, `pov_clock_renderer_render_status(...)`.

## Inputs

- `renderer`: holds the active column's 5-bit `mask` and `color`.
- `frame_words`: caller-owned buffer of length `frame_len` words.
- `active_led_count`: number of physically driven LEDs (1..`POV_LED_MAX_COUNT`).

## Definitions

- `N = min(active_led_count, frame_len)` — the writable active span length.
- `R = kFontRows = 5` — number of glyph rows.
- `row(i) = (i * R) / N` for `i` in `[0, N)` — integer division.

## Guarantees

- **C1 (full coverage)**: For any column whose `mask` bit for row `r` is set, every
  LED `i` in `[0, N)` with `row(i) == r` is set to `color`. In particular, when the
  top glyph row (bit 0) is set, LED 0 is lit; when the bottom glyph row (bit R-1)
  is set, LED `N-1` is lit.
- **C2 (no reserved margin)**: There is no fixed sub-range of `[0, N)` that is
  excluded from the mapping; every LED index maps to a valid row and can be lit by
  some column.
- **C3 (bounds safety)**: No write occurs at any index `>= N`; therefore no write
  exceeds `active_led_count` or `frame_len`.
- **C4 (blank column)**: If `mask == 0`, all of `[0, N)` are cleared (frame is
  fully dark for that column).
- **C5 (orientation)**: `row(i)` is non-decreasing in `i`; glyph row 0 occupies the
  lowest LED indices and row R-1 the highest.
- **C6 (content invariance)**: The set of glyph rows lit for a given column is
  exactly the set of `mask` bits; the feature changes only which LEDs a lit row
  fills, never which rows/columns are lit.
- **C7 (status span)**: `render_status` lights its indicator pattern across the
  full `[0, N)` span (already satisfied by the existing modulo pattern).

## Behavior at boundaries

- `active_led_count == POV_LED_MAX_COUNT (57)`: all 57 LEDs participate.
- `active_led_count < R`: mapping still yields valid rows; the image compresses.
  No out-of-range write occurs.
- `frame_len < active_led_count`: `N` clamps to `frame_len`; no overflow.
