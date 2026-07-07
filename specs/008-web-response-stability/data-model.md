# Data Model: Web Response Stability

## Portal Client Session

Represents the single TCP client currently accepted by the STA management portal.

**Fields**:

- `pcb`: active TCP control block, or empty when no client is connected
- `request_length`: number of bytes accumulated into the fixed request buffer
- `tx_active`: whether an HTML or JSON body is currently being streamed
- `tx_sent`: number of body bytes queued so far
- `tx_length`: total body bytes expected
- `last_progress_ms`: monotonic timestamp of the last receive, queue, acknowledgment, or accepted connection progress

**Validation rules**:

- A session with no request bytes and no transmit activity must be released after the idle timeout.
- A session with transmit activity must be released after the progress timeout if no progress occurs.
- Closing a session must clear request and transmit state.

## Request Buffer

Represents the fixed-size HTTP request accumulator.

**Fields**:

- `bytes`: static byte storage
- `length`: current used length

**Validation rules**:

- Must remain NUL-terminated for parsing.
- Must be reset when a session ends.

## Response Stream

Represents the active static response body stream.

**Fields**:

- `body`: pointer to the static page buffer
- `length`: response body length
- `sent`: body bytes queued to lwIP
- `active`: streaming flag

**Validation rules**:

- Body must remain valid until streaming completes or the client is closed.
- Streaming state must be cleared on normal finish, error, abort, timeout, and explicit stop.
