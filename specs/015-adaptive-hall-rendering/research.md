# Research: Adaptive Hall-Synchronized Rendering

## Decision: use 480-800 RPM with 40 angular columns

**Decision**: Keep 600 RPM nominal for reference, accept 480-800 RPM inclusive,
and render the compact clock over 40 angular columns.

**Rationale**: A 57-pixel WS2812 column needs about 1.71 ms of wire time. At the
800 RPM ceiling, a 40-column revolution provides 1.875 ms per slot, leaving a
small bounded margin for latch and scheduling. At ~764 RPM the slot is about
1.963 ms. The 28-column `HH:MM:SS` glyph layout fits centered in 40 columns.

**Alternatives considered**: Retaining 48 columns was rejected because ~764 RPM
provides only 1.636 ms per slot, shorter than physical transfer time. Raising the
ceiling above 800 RPM was rejected because the remaining margin becomes too small
without changing LED transport or angular resolution again.

## Decision: expose the accepted Hall edge timestamp

**Decision**: Add the latest accepted edge timestamp to the consumer-facing Hall
measurement and copy it into rotation state as the angular phase reference.

**Rationale**: Period alone determines frequency but not phase. An edge timestamp
anchors a known point once per revolution, enabling stable angular placement and
automatic correction on every fresh Hall event.

**Alternatives considered**: Free-running from renderer startup was rejected
because loop delay and period changes can shift phase. Direct renderer access to
Hall interrupt state was rejected because it violates hardware abstraction and
would require synchronization inside presentation code.

## Decision: derive target column from absolute phase

**Decision**: For each render opportunity, compute elapsed time from the latest
Hall reference, reduce it modulo the measured revolution period, and map that
phase to the 40-column range. Submit a frame only when the target column changes.

**Rationale**: Absolute phase mapping cannot accumulate truncation or loop-delay
error across revolutions. If several columns expire, the next decision selects
the current column directly rather than replaying stale work.

**Alternatives considered**: Repeatedly adding a truncated column interval was
rejected because fractional error accumulates. Deadline catch-up without Hall
phase was better than naive stepping but still left the image anchored to an
arbitrary software start time.

## Decision: update stability only on a new Hall generation

**Decision**: Carry the accepted Hall edge count as a sample generation and
update period-history stability only when that generation changes.

**Rationale**: The main loop reads the same capture many times per revolution.
Treating each read as a new sample can make a one-sample disturbance appear
stable on the next loop iteration. Generation-aware evaluation holds the state
until another physical revolution confirms recovery, satisfying the two-
revolution settling rule.

**Alternatives considered**: Comparing only timestamps was viable but the
existing accepted edge count is already monotonic and diagnostic. Updating on
every super-loop read was rejected as physically misleading.

## Decision: enforce complete WS2812 transfer readiness

**Decision**: Treat the transport as busy until DMA, PIO wire time, and the
required low latch interval have completed. Store a fixed transfer-ready
timestamp on every submission. When busy, drop the expired column and let the
next phase calculation select the current position.

**Rationale**: At the upper speed boundary the transfer budget is tight. Dropping
an expired column preserves phase and memory safety. DMA completion alone can
occur while words remain in the PIO FIFO and before the LEDs latch, so it cannot
protect the driver-owned brightness buffer or guarantee non-overlap.

**Alternatives considered**: Blocking until completion was rejected because it
would stall Wi-Fi/time/Hall work. A second unbounded queue was rejected because
obsolete columns have no value and consume deterministic memory.

## Decision: compensate the known presentation latency

**Decision**: Expose the deterministic frame wire-plus-latch duration and select
the angular column for the expected presentation time, not merely submission
start time.

**Rationale**: At 800 RPM the roughly 1.76 ms presentation latency is almost one
angular slot. Looking ahead by that bounded duration removes the constant phase
offset while Hall anchoring removes cumulative drift.

**Alternatives considered**: Submitting the column for the current instant was
rejected because it introduces a speed-dependent angular offset. A configurable
manual offset remains a future option for physical magnet-to-view alignment.

## Decision: validate jitter separately from drift

**Decision**: Host tests prove phase arithmetic, boundary inclusion, catch-up,
and lack of cumulative scheduling drift. Hardware tests measure actual strobe
jitter and readability across the supported range.

**Rationale**: Pure timestamp tests cannot measure IRQ, DMA, PIO, or electrical
latency on the target. The constitution's jitter budget therefore remains an
explicit hardware gate.

**Alternatives considered**: Claiming the hardware jitter budget from host tests
was rejected as unverifiable. Replacing the existing transport with a new alarm
or PIO scheduler was rejected until target measurements show it is necessary.
