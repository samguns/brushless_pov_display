# Data Model: Round-Display Cartesian Text Rendering

## Entities

### Cartesian Framebuffer (`fb`)

- **Represents**: the upright rendered image on the disc.
- **Shape**: `uint8_t fb[GRID * GRID]`, `GRID = POV_CLOCK_LED_ROWS = 57`, indexed
  `fb[y * GRID + x]`.
- **Values**: palette index 0..4 (0 off, 1 red, 2 green, 3 blue, 4 gray).
- **Lifetime**: file-static; rebuilt on `set_text`.

### Palette

- **Represents**: mapping of palette index → GRB color word.
- **Values**: `{0, red=(0x20<<8), green=(0x20<<16), blue=0x20, gray=0x181818}`,
  matching prior firmware colors.

### Polar Sampling Map

- **Represents**: per-column fixed-point trig used to project LED radius to (x, y).
- **Shape**: `int16_t cos256[COLS]`, `int16_t sin256[COLS]`, `COLS =
  POV_CLOCK_COLUMNS = 40`.
- **Lifetime**: file-static, computed once at init.

### Renderer State (`pov_clock_renderer_t`)

- Keeps: `text`, `active_column`, phase fields (`rotation_period_us`,
  `phase_reference_us`, `column_interval_us`, `phase_locked`), `text_ready`.
- Removes: `column_masks[COLS]`, `column_colors[COLS]` (replaced by `fb`).

## Relationships

```
set_text ──▶ rasterize ──▶ fb[57x57] (palette)
init ──▶ cos256/sin256 (once)
active_column + cos256/sin256 + fb ──▶ frame_words[0 .. active_count-1]
palette index ──▶ GRB color word
```

## Constants (unchanged)

- `POV_CLOCK_COLUMNS = 40`, `POV_CLOCK_LED_ROWS = 57`, `POV_LED_MAX_COUNT = 57`.
- `CENTER = 28` (derived, new): center LED index / disc center in the 57x57 grid.
