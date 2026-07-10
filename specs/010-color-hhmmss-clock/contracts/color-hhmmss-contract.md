# Contract: Color HHMMSS Clock

**Feature**: 010-color-hhmmss-clock

## Normal Clock Contract

- Text format: `HH:MM:SS`.
- Text length: exactly 8 characters.
- Time basis: existing UTC+8 wall-clock value.
- No timezone label is displayed.

## Color Contract

- Hour digits (`HH`): red.
- Minute digits (`MM`): green.
- Second digits (`SS`): blue.
- Colon separators: white or neutral.

## Rendering Contract

- Existing 48-column angular layout remains in use.
- Existing 57-LED radial frame output remains in use.
- LED frame transport remains DMA -> TX FIFO -> PIO.
- Invalid status patterns remain distinct from normal colored clock output.

## Preview Contract

- Output: `specs/010-color-hhmmss-clock/color_hhmmss_preview.jpg`.
- Must include an unwrapped 48x57 preview.
- Must include an approximate circular projection.
- Must visibly show red hours, green minutes, and blue seconds.

## Stability Contract

- No dynamic allocation in the firmware render path.
- `ninja -C build` remains the validation build command.
- No git commit is part of this procedure.
