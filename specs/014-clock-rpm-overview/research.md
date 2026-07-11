# Research: Clock and RPM Overview

## Decision: publish the existing formatted clock snapshot

**Decision**: Main publishes `pov_clock_time_t.calibrated` and its bounded
`HH:MM:SS` text through `wifi_config` and `wifi_sta_http` to the Overview builder.

**Rationale**: The POV clock already advances and formats authoritative CST time.
Reusing its snapshot prevents duplicate time conversion and keeps the web layer
independent from NTP and monotonic-clock internals.

**Alternatives considered**: Recomputing time in the HTTP callback was rejected
because it duplicates clock ownership. Reading `pov_clock_time_t` through a
shared pointer was rejected because it couples modules and weakens snapshot safety.

## Decision: copy bounded text at module boundaries

**Decision**: Each status-owning module copies at most eight clock characters
plus a terminator and stores a separate availability flag.

**Rationale**: Fixed copies are lifetime-safe across request callbacks, require
no heap, and keep request rendering deterministic even as main advances time.

**Alternatives considered**: Passing transient pointers was rejected because the
HTTP module retains status between calls. Carrying epoch/time-zone data was
rejected because the web layer only needs already formatted CST output.

## Decision: preserve independent unavailable states

**Decision**: Uncalibrated time renders `--:--:-- CST`; RPM continues to render
`-- RPM`, a measured value, or `0 RPM` according to its existing independent state.

**Rationale**: An unavailable clock must not suppress useful speed diagnostics,
and missing rotation must not hide calibrated time.

**Alternatives considered**: Hiding both cards until all data is valid was
rejected because it removes the partial diagnostics needed without USB.

## Decision: remain request-driven

**Decision**: Overview renders the latest status when a normal page request is
served; no browser polling or status endpoint change is added.

**Rationale**: This exactly matches refresh-based usage, adds no recurring radio
or HTTP traffic, and preserves the current self-contained page behavior.

**Alternatives considered**: Client polling was rejected as unrequested scope.
Rendering a browser-local clock was rejected because it would not prove the
device's calibrated clock state.

