# Research: Web Response Stability

## Decision: Add raw-lwIP client progress and idle timeout handling

**Rationale**: The STA portal intentionally supports a single active TCP client. Modern browsers can open speculative sockets and send no request bytes; without a timeout, that silent PCB can occupy the only slot indefinitely. A `tcp_poll` callback plus a `last_progress_ms` timestamp lets the server reclaim silent or stalled clients while preserving the static-buffer model.

**Alternatives considered**:

- Allow multiple concurrent clients: rejected because it would require per-client buffers or heap allocation, increasing RAM use and complexity.
- Abort the existing client whenever a second client arrives: rejected because it could interrupt a valid large response or settings scan.
- Disable browser preconnect behavior: rejected because it is client-specific and not under device control.

## Decision: Keep response streaming but treat successful queueing and acknowledgments as progress

**Rationale**: Feature 007 introduced pages larger than the TCP send buffer, so streaming remains necessary. The timeout must distinguish a healthy multi-chunk transfer from a wedged client by refreshing progress when receive data arrives, response bytes are queued, or `tcp_sent` acknowledges data.

**Alternatives considered**:

- Return smaller pages only: rejected because it would undo the portal redesign.
- Block until all response data is sent: rejected because it would reduce cooperative Wi-Fi polling and main-loop responsiveness.

## Decision: Centralize client cleanup

**Rationale**: Current close/error paths reset similar global state in several places. A small cleanup helper lowers the chance of stale `s_client_pcb`, request length, or transmit flags surviving after close, reset, abort, or timeout.

**Alternatives considered**:

- Patch only the accept path: rejected because disconnects, send failures, and timeout paths can all leave stale state.
