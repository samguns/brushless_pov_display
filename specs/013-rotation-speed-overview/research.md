# Research: Rotation Speed Overview

## Decision: publish through the existing runtime status pipeline

**Decision**: Main publishes availability plus rounded RPM to `wifi_config`,
which forwards the status to `wifi_sta_http`; the Overview builder receives the
same values as explicit inputs.

**Rationale**: This mirrors blink-status ownership, keeps the web layer isolated
from Hall hardware, and makes page rendering deterministic and directly testable.

**Alternatives considered**: Reading the Hall driver from the HTTP callback was
rejected because the sensor instance is owned by main and this would couple UI
transport to hardware. A new endpoint and browser polling were rejected because
the requested page-load metric does not require live client-side updates.

## Decision: preserve whether a valid sample has ever existed

**Decision**: Main tracks an `available` flag that becomes true after the first
valid measurement and remains true thereafter. While unavailable, Overview shows
`-- RPM`; after availability is established, stale readings publish `0 RPM`.

**Rationale**: The Hall driver reports both startup-without-two-edges and stopped
rotation as invalid/stale. Remembering prior validity is the smallest state that
separates an unknown startup value from a confirmed stopped value.

**Alternatives considered**: Showing `0 RPM` for every invalid reading was
rejected because it claims knowledge before measurement exists. Showing the last
moving RPM after staleness was rejected because it misrepresents stopped rotation.

## Decision: round once at the application boundary

**Decision**: Convert the finite, bounded non-negative sensor RPM to the nearest
whole `uint32_t` in main and carry that display-ready scalar through the status
pipeline.

**Rationale**: Whole RPM matches the compact dashboard requirement, avoids float
formatting in the embedded web builder, and guarantees every layer displays the
same rounded value.

**Alternatives considered**: Carrying a float through all layers was rejected as
unnecessary representation and formatting complexity. Truncation was rejected
because nearest rounding better represents the underlying measurement.

## Decision: use only fixed-size module state

**Decision**: Add one availability flag and one 32-bit RPM value to the Wi-Fi
runtime state and HTTP status state; add no buffers or dynamic allocation.

**Rationale**: The existing request path is single-threaded in the main loop, so
ordinary scalar assignment is sufficient. The page remains well within its
existing fixed 16 KiB buffer.

**Alternatives considered**: Allocating a status object per request and adding
locking were rejected because there is no concurrent producer/consumer context
in the current super-loop architecture.

