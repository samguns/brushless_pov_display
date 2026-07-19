# Research: Round-Display Cartesian Text Rendering

## Problem

`pov_clock_renderer` builds `column_masks[40]` where each column is an angular
position and each glyph row maps to an LED radius. On a spinning radial arm this
paints each glyph column as a radial line, so flat text is smeared into arcs — the
text is not upright or readable. The display is physically a disc of at most 57
pixels across (57 LEDs spanning the diameter).

## Decision: Cartesian framebuffer + polar sampling

1. **Rasterize upright**: draw the clock text into a fixed 57x57 palette
   framebuffer `fb[y*57 + x]` using the existing bitmap font, scaled and centered
   (e.g. sx=2, sy=3 → 54x15 text centered in the 57x57 grid), with a palette index
   per pixel (0 off, 1 red hours, 2 green minutes, 3 blue seconds, 4 gray colon).
2. **Precompute trig**: for each column c in 0..39, angle = 2*pi*c/40; store
   `cos256[c] = round(cos(angle)*256)` and `sin256[c] = round(sin(angle)*256)` as
   `int16` (fixed-point Q8). Computed once at init (float used only there).
3. **Sample per column**: for the active column c and each LED i in
   `[0, min(active_count, frame_len))`:
   - `dx = i - CENTER` (CENTER = 28)
   - `x = CENTER + round(dx * cos256[c] / 256)`
   - `y = CENTER + round(dx * sin256[c] / 256)`
   - if `(x, y)` is inside `[0,57)^2` and inside the disc, output `palette[fb[y*57+x]]`;
     otherwise output 0 (dark).

### Why

- Produces upright text: the LED-to-Cartesian projection is the inverse of the
  physical polar sweep, so the displayed image equals `fb` (FR-001, FR-002).
- Disc masking falls out naturally: points beyond the circle map outside `fb`
  bounds or are explicitly radius-checked and left dark (FR-003).
- Integer-only hot loop on a Cortex-M0+ (no hardware FPU); float only at init
  (FR-008).
- Preserves the phase/period column pipeline; `render_current` keeps its signature
  and `pov_leds.cpp` is unchanged (FR-006).

### Alternatives considered

- **Keep polar masks, "unwarp" the font**: would require a curved font per radius;
  intractable and still radius-dependent. Rejected.
- **Runtime float trig per column**: soft-float each render; unnecessary cost.
  Rejected in favor of a precomputed Q8 table.
- **Full per-(column,LED) index LUT (40x57 uint16 ≈ 4.5 KB)**: larger than the Q8
  table (160 B) for no runtime benefit at this size. Rejected.

## Geometry note

Assume the 57 LEDs span the diameter (CENTER = 28, signed radius i-28). With 40
columns the disc is sampled along 40 diameters (20 unique lines mirrored), so
angular resolution is ~9° — text is legible in the central band and coarser near
the rim. This matches the WS2812 transfer-time cap (~40 columns/rev). If hardware
turns out to be a radius arm, only CENTER and the radius sign change.

## Testability

The sampler is a pure function of `fb`, the precomputed trig, and inputs. Host
tests assert: bounds safety (canary past active count stays 0), disc masking
(corners dark), color palette, blank-when-no-text, and determinism. A Python
reconstruction (sample all 40 columns back into a 57x57 image) provides visual
confirmation.
