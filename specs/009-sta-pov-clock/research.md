# Research: STA POV Clock Display

## Decision: Recommend 600 RPM nominal speed, 480-720 RPM initial range

**Rationale**: A 57-pixel WS2812 frame takes about 1.7 ms on the wire before
allowing for latch and scheduling overhead. A readable compact clock needs around
48 angular columns per revolution. At 600 RPM (10 revolutions per second), a
revolution lasts 100 ms and each 48-column slot is about 2.08 ms, leaving a small
but practical margin for DMA submission and cooperative loop work. Faster targets
such as 1200 RPM reduce the slot to about 1.04 ms for 48 columns, which is below
the physical WS2812 transfer time for 57 LEDs.

**Alternatives considered**:

- 1200 RPM: higher refresh but insufficient column budget for `HH:MM:SS CST`
  using a 57-LED WS2812 chain.
- 900 RPM: still tight for 48 columns (~1.39 ms per column).
- 300 RPM: ample timing margin but only 5 revolutions per second, likely more
  flicker than necessary.

## Decision: Use raw lwIP UDP + DNS for network time calibration

**Rationale**: The firmware already runs lwIP in polling/no-socket mode through
`pico_cyw43_arch_lwip_poll`, and `lwipopts.h` has DNS and UDP enabled while
sockets and netconn are disabled. A small raw UDP NTP request/response state
machine fits the existing networking model, avoids enabling sockets or larger
lwIP subsystems, and can be stepped from the same super-loop as the STA portal.

**Alternatives considered**:

- lwIP SNTP app module: convenient if already configured, but would add another
  subsystem and still needs configuration work in this no-OS build.
- HTTP time service: simpler parsing in some environments, but depends on a
  service-specific protocol and uses the TCP client side not currently present.
- Manual time entry: avoids networking but fails the requested STA-mode
  calibration behavior.

## Decision: Maintain calibrated UTC epoch plus monotonic boot timestamp

**Rationale**: The target only needs current wall-clock seconds after a successful
calibration. Storing the calibrated UTC seconds together with the boot-time
microsecond timestamp at calibration lets the clock advance from the monotonic
timer without repeated network requests. UTC+8 conversion is pure arithmetic
with no daylight-saving rules.

**Alternatives considered**:

- Continuous network polling every second: unnecessary network traffic and more
  failure modes.
- RTC persistence across reboot: useful later, but no persistent RTC requirement
  exists in this feature.

## Decision: Render a compact 48-column built-in clock layout

**Rationale**: A fixed 48-column polar frame can fit `HH:MM:SS CST` using compact
3x5 glyphs plus spacing. Each column maps to one 57-LED radial frame. The
renderer can scale the 5-row font into a central vertical band and leave the rest
dark, keeping the framebuffer small and deterministic.

**Alternatives considered**:

- Full-size proportional text: better aesthetics but too many columns for the
  WS2812 timing budget at useful spin speeds.
- Display `HH:MM:SS` only: easier, but the spec asks for CST format. The compact
  layout can still include the timezone label.
- Pre-render full 48 x 57 pixel bitmap: simple playback but uses more static RAM
  and duplicates content that can be generated from glyph tables.

## Decision: Gate normal display on both time and rotation suitability

**Rationale**: The device must not show a confident clock when time is
uncalibrated, the Hall reading is stale, or the measured speed is outside the
supported range. A single display health state keeps these decisions explicit and
observable.

**Alternatives considered**:

- Always show the last known time: misleading after calibration failure or long
  network loss.
- Show distorted output at any speed: makes validation harder and undermines
  trust in the clock.
