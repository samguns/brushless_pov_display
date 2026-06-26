# Contract: WS2812 POV Hello Demo Behavior

**Feature**: 004-ws2812-pov-hello

## Scope

Defines externally observable runtime behavior for the built-in POV Hello demo.

## Configuration Contract

- Input: requested LED count.
- Valid range: 1..57.
- Behavior:
  - Values in range are accepted as-is.
  - Values above 57 are clamped to 57 and reported via debug output.
  - Invalid/non-operational values trigger bounded error state and preserve responsiveness.

## Output Transport Contract

- LED frame emission uses DMA -> TX FIFO -> PIO transport.
- CPU bit-banging is not used for WS2812 frame transmission.

## Playback Contract

- Sequence: H, e, l, l, o.
- Start condition: auto-start after WS2812 output-path readiness.
- Cadence: 1000 ms per character, tolerance +/-100 ms.
- Looping: continuous replay; after o, next character is H.
- Timing source: runtime timing values derive from `clock_get_hz(clk_sys)`.

## Observability Contract

Required debug events:
- Driver readiness success/failure.
- Active LED count used for rendering.
- Character transition events (`from`, `to`, timestamp).
- Bounded-error events for invalid count or initialization issues.

## Stability Contract

- System remains responsive during continuous playback.
- No out-of-range LED writes occur when requested count exceeds 57.
- Build workflow remains unchanged (`ninja -C build`).
