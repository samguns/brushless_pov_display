# Contract: Cartesian Polar-Sampling Renderer

Applies to `pov_clock_renderer_render_current(renderer, frame_words, frame_len,
active_led_count)` after the round-display change.

## Definitions

- `GRID = 57`, `CENTER = 28`, `COLS = 40`.
- `N = min(active_led_count, frame_len)`.
- `c = renderer->active_column % COLS`.
- For LED `i` in `[0, N)`: `dx = i - CENTER`,
  `x = CENTER + round(dx * cos256[c] / 256)`,
  `y = CENTER + round(dx * sin256[c] / 256)`.

## Guarantees

- **C1 (Cartesian output)**: `frame_words[i] = palette[fb[y*GRID + x]]` when
  `0 <= x < GRID` and `0 <= y < GRID` and `(x,y)` is inside the disc; otherwise
  `frame_words[i] = 0`.
- **C2 (disc masking)**: any LED whose `(x, y)` is outside the inscribed circle is
  dark.
- **C3 (bounds safety)**: no write at index `>= N`; no read of `fb` outside
  `[0, GRID*GRID)`.
- **C4 (blank when no text)**: if `text_ready` is false, all of `[0, N)` are 0.
- **C5 (color preservation)**: palette maps hours→red, minutes→green,
  seconds→blue, separators→gray, unchanged from prior firmware.
- **C6 (determinism)**: identical inputs (`active_column`, `active_led_count`,
  framebuffer) yield identical output.
- **C7 (phase pipeline preserved)**: `render_current` does not change how
  `active_column` is chosen; `pov_clock_renderer_step` behavior is unchanged.

## Non-goals

- Increasing angular resolution beyond the WS2812 transfer-time cap (~40
  columns/rev). Coarseness at large radius is expected.
