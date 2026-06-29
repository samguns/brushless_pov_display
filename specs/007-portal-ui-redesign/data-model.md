# Phase 1 Data Model: Management Portal UI/UX Redesign

**Feature**: 007-portal-ui-redesign

The redesign is mostly presentational; the only genuinely new persisted/runtime
data is the **brightness** value. Everything else is existing data re-laid-out.

## Entities

### 1. Display preferences (new)

| Field | Type | Range / Default | Persistence | Notes |
|-------|------|-----------------|-------------|-------|
| `brightness` | `uint8_t` | 0–100 (%) UI; default = full | Flash (V3 record) | Scales live LED output; survives reboot |
| `theme` | enum {dark, light} | default `dark` | Browser `localStorage` only | Client-side; never sent to device |

- **brightness** is owned by `wifi_config` at runtime (a field in
  `wifi_runtime_state_t`), read by `pov_leds.cpp` to set the driver scalar, and
  persisted to flash on a changed `POST /display`.
- **theme** is not device state; it exists only in the served page + the
  browser. No data-model impact on firmware beyond emitting the toggle markup.

### 2. Flash credential record — V3 (extends existing)

Current `wifi_flash_record_v2_t` (171 B) gains one byte → V3 (172 B), still well
within the 4 KB reserved sector. V1/V2 remain readable.

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;          // WIFI_FLASH_MAGIC_V3 (0xC0FFEE03)
    uint8_t  version;        // WIFI_FLASH_VERSION_V3 (3)
    char     ssid[33];
    char     password[64];
    char     admin_token[64];
    uint8_t  brightness;     // NEW: 0..100, persisted display brightness
    uint8_t  flags;
    uint32_t crc32;          // over all preceding bytes
} wifi_flash_record_v3_t;
```

**Migration / compatibility rules**:
- `load_credentials` checks magic in order V3 → V2 → V1.
- Reading a V2/V1 record yields `brightness = DEFAULT_BRIGHTNESS` (full).
- The next save (credential change *or* brightness change) writes a V3 record.
- CRC32 is recomputed over the V3 layout; an invalid/again-mismatched record is
  rejected exactly as today (device falls back to AP provisioning for creds).

**Persistence helpers** (in `wifi_flash`):
- `load_credentials(wifi_credentials_t*)` — unchanged signature; internally
  fills brightness into a new out-param or a combined struct (see below).
- Add `bool load_device_settings(wifi_credentials_t *creds, uint8_t *brightness)`
  **or** extend `wifi_credentials_t` with a `brightness` field — implementation
  detail for tasks; the contract is: creds + brightness load/save atomically in
  one record, and saving brightness preserves the current creds and token.
- `bool save_brightness(uint8_t brightness)` — read-modify-write the record,
  preserving SSID/password/admin_token; reuse atomic erase+write+verify.

### 3. Device status (existing — re-laid-out only)

Surfaced on the Overview screen as metric cards. No structural change.

| Field | Source |
|-------|--------|
| connectivity state | `wifi_config_get_connectivity_state_text()` |
| SSID | `wifi_config` runtime creds (already passed to `wifi_sta_http`) |
| IP address | `wifi_config_get_active_ip()` |
| blink active | `wifi_config_get_blink_active()` |
| blink frequency (Hz) | `wifi_config_get_blink_frequency_hz()` |

### 4. Firmware version (existing constant — newly displayed)

| Field | Type | Source |
|-------|------|--------|
| firmware version | string | Program version (`pico_set_program_version`, currently "0.1") exposed to the portal as a compile-time string constant |

Shown read-only in the System card. No new storage.

### 5. Nearby network (existing — re-laid-out only)

`scan_result_t` (SSID + secured flag) from `wifi_scan`, rendered as selectable
rows in the Network card. No change.

### 6. Read-only network addressing (display fidelity)

| Field | Source | Editable |
|-------|--------|----------|
| IP address | `wifi_config_get_active_ip()` | No |
| subnet mask | live netif netmask or shown placeholder | No |
| gateway | live netif gateway or shown placeholder | No |
| Static-IP toggle | always "off" | No |

Rendered for visual match only (FR-018); never submitted.

### 7. Action notice (existing)

Short banner string (`s_reconfig_notice`) reused; restyled per the design.

## State & Validation

- **Brightness submit (`POST /display`)**: parse integer 0–100; clamp out-of-range
  to the valid range; if the parsed value equals the stored value, skip the flash
  write (apply only); otherwise apply to the driver scalar and persist (V3).
- **Brightness apply order**: at boot, `pov_leds.cpp` loads the persisted
  brightness (via `wifi_config`) and calls the driver setter before/at the first
  frame; thereafter `POST /display` updates the runtime value and the driver
  scalar takes effect on the next submitted frame.
- **Theme**: no server-side validation; the toggle is purely client-side.
- **All existing validation** (SSID length, password 8–63, atomic credential
  persistence, single client) is unchanged (FR-013).

## Memory Budget Delta

| Item | Before | After | Notes |
|------|--------|-------|-------|
| Static page buffer (`STA_PAGE_BUF_SIZE`) | 8192 B | 16384 B (RES-004) | one fixed array, no heap |
| Flash record | 171 B (V2) | 172 B (V3) | within 4 KB sector |
| Runtime state | — | +1 byte brightness in `wifi_runtime_state_t` | negligible |
| Driver state | — | +1 byte brightness in `ws2812_driver_t` | negligible |

No dynamic allocation is introduced anywhere (Principle IV).
