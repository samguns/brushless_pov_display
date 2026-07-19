# Contract: Web Log Viewer HTTP and UI

**Feature**: 020-web-log-viewer

This contract extends the existing station-mode management portal. It uses the
same raw-lwIP listener, one-active-client limit, local-network access boundary,
shared dark/light shell, and fixed response storage as Overview and Settings.
No route in this contract mutates device state.

## Common response rules

- Log routes are same-origin and do not enable cross-origin access.
- Log HTML and JSON responses include `Cache-Control: no-store` and
  `X-Content-Type-Options: nosniff`.
- Every response includes the correct content type and content length and then
  closes the connection using the portal's existing bounded send lifecycle.
- A response must be complete and valid or a bounded error is returned; partial
  HTML/JSON is never treated as success.
- Successful `/logs/updates` access is not itself retained as a log entry.

## Access boundary

`GET /logs` and `GET /logs/updates` are unauthenticated read-only routes,
matching existing station-portal GET access from the same local network. They
never return passwords, administrative/authorization tokens, request bodies,
or firmware payload contents. This feature does not expose the listener to the
public internet and does not add CORS.

## GET /logs

Returns the self-contained Logs screen (`text/html; charset=utf-8`). The page
contains the existing shared navigation and theme support plus:

- `Logs` title and current-boot diagnostic-history description;
- visible `Connecting`, `Live`, and `Disconnected` connection states;
- boot-session and uptime indicators;
- bounded oldest-to-newest log rows;
- Pause/Resume control and unseen-entry indicator;
- a Clear control that empties only the browser's displayed rows while retaining
  the device history, session, cursor, and live/pause state;
- empty-history, gap, truncation, and device-restarted indicators.

The initial HTML embeds no diagnostic entries. It starts in `Connecting` and
retrieves them through the update route.

## GET /logs/updates

Returns one immutable snapshot batch as `application/json`.

### Query parameters

| Parameter | Required | Format | Default / validation |
|---|---|---|---|
| `session` | No | Exactly 16 hexadecimal characters | Absent on first request. Invalid non-empty value returns 400. |
| `after` | No | Unsigned decimal 32-bit sequence | Defaults to 0. Represents the last entry actually displayed. |
| `limit` | No | Unsigned decimal integer 0..16 | Defaults to 16. `0` requests metadata only and never advances the cursor. |

Unknown parameters are ignored. Duplicate recognized parameters, overflow,
signed values, invalid characters, or an out-of-range limit return 400. Query
parsing reads only the request target and never request headers or bodies.

### Successful response

```json
{
  "session": "7f90a4b31c2d6e08",
  "uptime_ms": 123456,
  "oldest_seq": 42,
  "newest_seq": 169,
  "next_after": 57,
  "more": true,
  "session_changed": false,
  "gap": {
    "first_missing": 1,
    "last_missing": 41
  },
  "entries": [
    {
      "seq": 42,
      "time_ms": 3210,
      "source": "hall",
      "message": "speed rpm=382 rad_s=40.00 target_rpm=382 target_rad_s=40 range=306-509 period_us=157080",
      "truncated": false
    }
  ]
}
```

### Response field rules

- `session` is always the current nonzero boot ID, lower-case, zero-padded hex.
- Empty history uses `oldest_seq: 0`, `newest_seq: 0`, `next_after: 0`,
  `more: false`, `gap: null`, and `entries: []`.
- Entries are strictly increasing and contain at most `limit` items.
- `next_after` equals the last returned sequence. If no entry is returned, it
  remains the effective valid cursor (zero for initial empty history).
- `more` means an entry newer than `next_after` existed in the response snapshot.
- `gap` is `null` when no sequence was missed; otherwise both endpoints are
  inclusive and exact for the overwritten range known to the store.
- Source labels come only from the fixed firmware enum. Message data cannot
  inject a source label.
- JSON escapes quote, backslash, control, and HTML-significant bytes; valid
  non-ASCII UTF-8 remains valid, and invalid input is visibly replaced.

### Cursor cases

#### First request (no session)

- Ignore any implied prior boot continuity.
- Begin at the oldest retained entry, up to `limit`.
- If `oldest_seq > 1`, return `gap` for `1..oldest_seq-1` so the page does not
  imply that current-boot history is complete.
- `session_changed` is false because no prior session was claimed.

#### Same session, cursor retained

- Start at `after + 1`.
- Return no duplicate at or below `after`.
- `gap` is null.

#### Same session, cursor overwritten

- If `after + 1 < oldest_seq`, start at `oldest_seq`.
- Return `gap` from `after + 1` through `oldest_seq - 1`.

#### Different session

- Set `session_changed: true` and ignore the old `after` value.
- Start from the current oldest retained entry.
- Report any already-overwritten portion of the new session as `gap`.
- The browser clears old rows and inserts a visible device-restarted marker
  before displaying the new session; it must not merge the two sessions.

#### Metadata-only pause probe (`limit=0`)

- Return current range, uptime, session, session-change, and gap metadata.
- Return `entries: []`, do not advance `next_after`, and set `more` when newer
  retained data exists after the displayed cursor.
- The browser uses the newest range to show unseen activity without allocating
  a second pending queue or changing capture.

#### Cursor newer than current range

- With a matching session, `after > newest_seq` is an invalid cursor and returns
  400. With a mismatched session, the different-session behavior takes priority.

### Error response

Status `400 Bad Request`, bounded JSON:

```json
{"error":"invalid log cursor"}
```

Errors reveal no request header/body content or secret values.

## Browser behavior contract

### Polling

- Exactly one update request may be in flight per page.
- When caught up and not paused, schedule the next request 1,000 ms after the
  prior request settles. When `more` is true, request the next batch immediately.
- Abort a request that has not completed within 4 seconds, mark the page
  `Disconnected`, and retry after 1, 2, 4, then at most 5 seconds.
- Any successful batch returns the page to `Live`. Existing rows remain visible
  through failures.

### Rendering and ordering

- Construct every entry row with DOM element creation and `textContent`; no
  device-provided field is assigned through `innerHTML`.
- Display `#sequence`, boot-relative `HH:MM:SS.mmm`, source, message, and a
  visible truncation badge when applicable.
- Accept only the expected session and strictly increasing sequences. Duplicate
  or reversed client data is not appended and changes the page to a visible
  error/disconnected state rather than silently corrupting order.
- Retain at most 128 entry rows in the browser. Trimming a browser row must not
  clear or mutate device history.

### Pause and resume

- Pause preserves scroll position and `displayed_after` and switches subsequent
  polling to `limit=0`.
- The unseen indicator reflects new retained activity or reports a gap when the
  exact count no longer fits the retained range.
- Resume immediately retrieves batches after `displayed_after`, inserts any gap
  marker, catches up, clears unseen state, and scrolls to the newest row.

### Responsive and regression behavior

- Logs is reachable in one navigation action from Overview and Settings, and
  those screens gain the same Logs link without losing existing content.
- At widths <=640 px, controls remain reachable, messages wrap within the page,
  and no page-wide horizontal overflow is introduced.
- The Logs page is fully self-contained; it fetches no fonts, scripts, styles,
  or analytics from the internet.
