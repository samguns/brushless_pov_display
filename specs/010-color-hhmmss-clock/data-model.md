# Data Model: Color HHMMSS Clock

## Colored Clock Value

Represents the visible normal clock text.

**Fields**:

- `text`: fixed 8-character `HH:MM:SS` string.
- `hour`: 0-23.
- `minute`: 0-59.
- `second`: 0-59.

**Validation Rules**:

- Text length is exactly 8 characters in normal mode.
- Text never contains `C`, `S`, or `T` as a timezone label.
- Time fields continue to derive from UTC+8 local time.

## Time Component Color Map

Represents the color assignment for normal display columns.

**Fields**:

- `hour_color`: red.
- `minute_color`: green.
- `second_color`: blue.
- `separator_color`: white.

**Validation Rules**:

- Columns generated from hour digits use red.
- Columns generated from minute digits use green.
- Columns generated from second digits use blue.
- Colon columns use the separator color.

## Renderer Column State

Represents the compact angular layout.

**Fields**:

- `column_masks[48]`: per-column 5-row glyph mask.
- `column_colors[48]`: per-column GRB color for active pixels.
- `active_column`: current angular column.
- `column_interval_us`: measured-period-derived column interval.

**Validation Rules**:

- Both arrays have exactly 48 entries.
- Frame output stays within 57 LED words.
- No dynamic allocation is used in normal rendering.

## Preview Artifact

Represents the generated review image.

**Fields**:

- `path`: `specs/010-color-hhmmss-clock/color_hhmmss_preview.jpg`.
- `sample_text`: `12:34:56`.
- `views`: unwrapped frame buffer and circular projection.

**Validation Rules**:

- JPEG file exists after implementation.
- Preview uses the same text and color mapping as firmware.
