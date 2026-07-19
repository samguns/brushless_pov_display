# Phase 1 Data Model: Web Log Viewer

**Feature**: 020-web-log-viewer

All device-side state is current-boot-only and fixed-size. The browser owns its
viewer state; no viewer session, cursor, or queue is persisted on the device.

## 1. Log Entry

One accepted diagnostic event in production order.

| Field | Target type / size | Validation and meaning |
|---|---:|---|
| `uptime_ms` | `uint64_t` / 8 B | Milliseconds since the current boot; supplied by the initialized monotonic clock, never wall-clock time. |
| `sequence` | `uint32_t` / 4 B | Starts at 1 and increases once per retained event. Together with `boot_id`, uniquely orders this session. |
| `source` | enum / 1 B | One fixed, recognized source such as system, driver, clock, health, Hall, time, Wi-Fi connection/HTTP/DNS/scan/flash, DHCP, or firmware update. Label text is flash-resident. |
| `flags` | `uint8_t` / 1 B | Includes `TRUNCATED` and `SANITIZED`; unknown bits are zero on write. |
| `text_len` | `uint8_t` / 1 B | Number of stored message bytes excluding NUL; never exceeds 100. |
| `text` | `char[101]` / 101 B | Safe UTF-8 message, NUL-terminated. Sensitive values are removed before commit. |
| alignment | 5 B | Target padding so `sizeof(pov_log_entry_t) == 120`. |

### Entry validation

- Formatting is bounded; no allocation and no unbounded scan is permitted.
- A message longer than 100 visible bytes is shortened on a complete UTF-8 code
  point, visibly marked, and flagged `TRUNCATED`.
- Invalid UTF-8 is replaced with a visible replacement before storage.
- Credential and authorization key/value patterns are redacted and flagged
  `SANITIZED`; producer call sites must also omit secrets by construction.
- Newline/control input is stored in a representation that can be serialized as
  valid JSON and displayed as inert text.

## 2. Log Store

The sole device-side owner of current-boot history.

| Field | Target type / count | Meaning |
|---|---:|---|
| `entries` | `pov_log_entry_t[128]` | Fixed circular array; logical order is oldest retained to newest. |
| `boot_id` | `uint64_t` | Nonzero random identifier for the active boot, represented externally as 16 lowercase hex digits. |
| `next_sequence` | `uint32_t` | Sequence assigned to the next accepted entry. |
| `head` | `uint16_t` | Physical slot where the next entry is written. Range 0..127. |
| `count` | `uint16_t` | Number of valid entries. Range 0..128. |
| `clock_provider` | fixed function pointer | Returns boot-relative milliseconds; installed once during initialization for production and deterministic host tests. |
| `initialized` | boolean | Rejects/ignores retention until boot/session/clock state is valid. |

### Store invariants

- `count <= 128`; when full, an append overwrites the oldest slot and advances
  `head` without allocating or moving other entries.
- Valid logical sequences are contiguous from `oldest_sequence` through
  `newest_sequence` unless the store is empty.
- A snapshot reports session, count, oldest, and newest from one cooperative
  main/poll context. Log capture is explicitly not callable from interrupts.
- Sequence wrap resets the ring and starts a new logical session rather than
  presenting ambiguous order.
- Ring plus metadata is at most 15,392 target bytes and is checked at compile
  time against the 16 KiB feature budget.

## 3. Log Snapshot

A small by-value description used to plan a response without copying the ring.

| Field | Meaning |
|---|---|
| `boot_id` | Session identity at snapshot time. |
| `count` | Retained entry count. |
| `oldest_sequence` | First retrievable sequence, or 0 when empty. |
| `newest_sequence` | Last retrievable sequence, or 0 when empty. |
| `uptime_ms` | Device uptime when snapshot was taken. |

`pov_log_read(sequence, out)` copies one entry at a time after validating that
the requested sequence is still within the snapshot range. No batch array is
added to static RAM or the HTTP module.

## 4. Log Batch Representation

A transient HTTP representation serialized directly into the existing shared
16 KiB response buffer.

| Field | Meaning |
|---|---|
| `session` | Current boot ID as a 16-character hex string. |
| `uptime_ms` | Uptime at response snapshot. |
| `oldest_seq`, `newest_seq` | Current retained range, or zero for empty history. |
| `next_after` | Last sequence returned; the client supplies it as `after` next time. |
| `more` | More retained entries existed after this batch in the same snapshot. |
| `session_changed` | Requested boot session did not equal current session. |
| `gap` | Optional exact missing sequence range caused by overwrite. |
| `entries` | Zero to sixteen ordered Log Entry projections. |

The batch itself is never stored after response transmission. Escaping failure
or buffer exhaustion returns a complete bounded error response, never partial
JSON.

## 5. Viewer State

Ephemeral state in one browser page.

| Field | Values / rule |
|---|---|
| `connection` | `connecting`, `live`, or `disconnected`. |
| `session` | Last accepted 16-hex boot ID, or absent before first success. |
| `displayed_after` | Last sequence actually rendered; pause does not advance it. |
| `following` | `true` for auto-follow; `false` while paused. |
| `unseen_count` | Difference between newest metadata and displayed cursor, shown as a bounded count/gap indication. |
| `rows` | Oldest-to-newest DOM rows, capped at 128. |
| `retry_delay` | 1, 2, 4, then at most 5 seconds after failures. |
| `scroll_position` | Preserved during pause and disconnection; resume scrolls to newest after catch-up. |

### Viewer state transitions

```text
load -> connecting
connecting --successful batch--> live
connecting --failure/timeout--> disconnected
live --pause--> live + following=false
live --failure/timeout--> disconnected (rows preserved)
disconnected --successful same session--> live + resume from displayed_after
disconnected --successful new session--> live + clear rows + restart marker
paused --resume--> immediate bounded catch-up -> following=true -> newest row
any live/paused batch --stale cursor--> insert gap marker -> continue at oldest
```

## Relationships

- One Log Store contains zero to 128 Log Entries for exactly one Boot Session.
- One HTTP request reads one Log Snapshot and projects zero to sixteen entries
  into one Log Batch.
- A Viewer consumes successive batches, but its cursor never changes device
  history and it cannot pause capture.
- Session and sequence jointly prevent duplicate display and cross-boot merging.

## Static RAM Budget

| Item | Target bytes | Notes |
|---|---:|---|
| 128 x Log Entry | 15,360 | `128 * 120`; compile-time asserted. |
| Ring/session/index/clock state | <=32 | Target-layout cap including padding. |
| New HTTP response storage | 0 | Existing `s_page_buf[16384]` is reused. |
| New persistent device viewer state | 0 | Viewer state lives in the browser. |
| **Maximum feature persistent RAM** | **15,392** | 992 bytes below the 16 KiB limit. |

Stack-local formatting and single-entry copy objects are bounded but are not
persistent static RAM; their maximum sizes must still be reviewed during
implementation.
