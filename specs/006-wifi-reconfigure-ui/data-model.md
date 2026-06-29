# Data Model: Wi-Fi Reconfiguration UI

**Date**: 2026-06-29
**Feature**: 006-wifi-reconfigure-ui

This feature largely reuses existing entities. New/extended structures are minimal.

## Entity: CredentialChangeRequest (transient)

Parsed from the `POST /config` form body; not persisted.

| Field | Type | Description |
|---|---|---|
| `ssid` | char[33] | Submitted network name (1..32 chars after URL-decode) |
| `password` | char[64] | Submitted WPA2 passphrase (8..63 chars) |

**Validation rules**:
- `ssid` non-empty and ≤ `WIFI_SSID_MAX_LEN` (32).
- `password` length within WPA2 bounds (8..63, ≤ `WIFI_PASS_MAX_LEN`).
- Values are URL-decoded before validation; rejection occurs before any radio
  action.

## Entity: ReconfigOutcome (transient)

Result of a change attempt, used to render the response page.

| Field | Type | Description |
|---|---|---|
| `status` | enum | `APPLIED`, `REJECTED_VALIDATION`, or `FAILED_CONNECT` |
| `reason` | text | High-level message for display (no secrets) |
| `active_ssid` | char[33] | SSID in effect after the attempt (new on success, previous on failure) |

**State transitions**:
- `REJECTED_VALIDATION`: input invalid → no disconnect; previous network intact.
- `FAILED_CONNECT`: valid input, new network unreachable/bad password → revert to
  previous credentials; device reachable.
- `APPLIED`: connected on new network → persisted; portal restarts on new network.

## Entity: Stored Credentials (reused — `wifi_credentials_t`)

Existing persisted record; unchanged shape.

| Field | Type | Description |
|---|---|---|
| `ssid` | char[33] | Active/connect SSID |
| `password` | char[64] | Active/connect passphrase |
| `admin_token` | char[64] | Preserved across a credential change |

Roles during a change:
- **Active (backup)**: the currently working set, retained until the candidate
  connects.
- **Candidate**: the submitted set under test; persisted only on successful
  connect.

## Entity: Scan Result (reused — `scan_result_t`)

Existing scan entry; rendered as a selectable list (≤ `WIFI_SCAN_MAX_RESULTS` = 20).

| Field | Type | Description |
|---|---|---|
| `ssid` | char[33] | Network name (selecting fills the SSID field) |
| `rssi` | int16 | Signal strength (used for sort/display) |
| `secured` | uint8 | 1 = secured (show lock), 0 = open |

## Static Memory Notes

- No new persistent buffers: reuses `wifi_sta_http` static page/request buffers
  (`s_page_buf` 4096, `s_req_buf` 1024) and the bounded scan-result array.
- The reconfiguration page with a 20-network list fits within the 4096-byte page
  buffer (~80 bytes/row); confirm during implementation and trim row markup if
  needed.
- No heap allocation is introduced in the credential-write path.
