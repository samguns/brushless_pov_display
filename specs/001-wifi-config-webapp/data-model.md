# Data Model: WiFi Configuration Web App

**Date**: 2026-06-25
**Feature**: 001-wifi-config-webapp

---

## Entity: WiFi Credential Record

Represents the single set of WiFi credentials persisted to flash. Only one record exists at a time.

| Field      | Type             | Size     | Description                                              | Constraints                       |
|------------|------------------|----------|----------------------------------------------------------|-----------------------------------|
| `magic`    | `uint32_t`       | 4 B      | Validity marker. `0xC0FFEE01` = valid. `0xFFFFFFFF` = erased (no credentials). | Must equal `0xC0FFEE01` to be considered present. |
| `ssid`     | `char[]`         | 33 B     | WiFi network name, null-terminated.                      | 1–32 printable ASCII chars + NUL. Empty SSID is invalid. |
| `password` | `char[]`         | 64 B     | WiFi network password, null-terminated.                  | 0–63 chars + NUL. Empty password is valid (open networks). |
| `flags`    | `uint8_t`        | 1 B      | Reserved for future use.                                 | Must be `0x00` in v1.            |
| `crc32`    | `uint32_t`       | 4 B      | CRC-32 over all preceding bytes (offsets 0–101).        | Mismatch → record treated as corrupt/absent. |
| *(padding)*| `uint8_t[]`      | to 4096 B| Fills remainder of the 4 KB flash sector.                | Filled with `0xFF` (erased value). |

**Total record size**: 106 bytes of meaningful data within a 4 096-byte flash sector.

**Flash location**: Last 4 KB sector of the 2 MB flash.
- Byte offset from flash base: `PICO_FLASH_SIZE_BYTES - 4096` = `0x1FF000`
- Absolute XIP address: `XIP_BASE + 0x1FF000` = `0x101FF000`

**Validation rules**:
1. `magic == 0xC0FFEE01` → proceed to CRC check.
2. CRC-32 of bytes [0..101] must match `crc32` field → record is valid.
3. `ssid[0] != '\0'` → SSID is non-empty.
4. Failure of any rule → treat as "no credentials stored."

**State transitions**:

```
[Flash erased / magic=0xFF]
         │
         │  save_credentials(ssid, password)
         ▼
[magic=0xC0FFEE01, ssid filled, password filled, crc32 valid]
         │
         │  save_credentials(new_ssid, new_password)   ← erase + rewrite
         ▼
[Updated record]
         │
         │  clear_credentials()                         ← erase sector
         ▼
[Flash erased / magic=0xFF]
```

---

## Entity: Scan Result Entry

Transient (RAM-only, not persisted). Represents one discovered access point from a WiFi scan.

| Field       | Type        | Size   | Description                                     | Constraints                    |
|-------------|-------------|--------|-------------------------------------------------|--------------------------------|
| `ssid`      | `char[]`    | 33 B   | Network name, null-terminated.                  | May contain UTF-8 in practice; treated as byte string. |
| `rssi`      | `int16_t`   | 2 B    | Signal strength in dBm (negative; closer to 0 = stronger). | Typically –30 to –90 dBm. |
| `auth_mode` | `uint8_t`   | 1 B    | Authentication type: 0=Open, 3=WPA2, 5=WPA3.   | Display "Open" vs "Secured" in UI. |

**Deduplication**: Multiple scan entries with identical SSIDs are collapsed to the entry with the highest (strongest) RSSI.

**Capacity**: Up to 20 deduplicated entries stored in a static array. Additional results are discarded.

**Sort order**: Descending by RSSI (strongest signal first) before being served to the web UI.

---

## Entity: AP Session (runtime only)

Transient configuration of the device's own access point. Not persisted.

| Field      | Value (fixed)       | Notes                                               |
|------------|---------------------|-----------------------------------------------------|
| SSID       | `pov-leds-setup`    | Hard-coded. Identifies the device on the scan list. |
| Password   | `12345678`          | Fixed WPA2 passphrase. Communicated to user via docs/labelling. |
| Auth mode  | WPA2                |                                                     |
| Channel    | 6 (default)         | Fixed; avoids contention with device's STA scan.   |
| IP address | `192.168.4.1`       | Device IP while in AP mode (lwIP default).          |
| DHCP range | `192.168.4.2–20`    | Assigned to connecting clients by lwIP DHCP server. |

---

## Entity: Connection Attempt (runtime only)

Transient state during the STA connection validation phase.

| Field       | Type            | Description                                          |
|-------------|-----------------|------------------------------------------------------|
| `ssid`      | `char[33]`      | Target network name from form submission.            |
| `password`  | `char[64]`      | Target network password from form submission.        |
| `state`     | enum            | `IDLE`, `CONNECTING`, `SUCCESS`, `FAILED_AUTH`, `FAILED_TIMEOUT` |
| `timeout_ms`| `uint32_t`      | Max wait for connection: 15 000 ms.                  |

**Outcome mapping**:
- `SUCCESS` → persist credential record, reboot into STA mode.
- `FAILED_AUTH` → restart AP, serve error page "Incorrect password."
- `FAILED_TIMEOUT` → restart AP, serve error page "Network not found or out of range."
- `SAVE_FAILED` → restart AP, serve error page "Connected but failed to save settings."

---

## RAM Budget (Constitution Principle IV)

Static allocations introduced by this feature. All sizes are worst-case on a 32-bit ARM target (RP2040).

| Allocation | Type | Size |
|------------|------|------|
| Scan result array (`scan_result_t[20]`) | Static global | ~740 B |
| `wifi_credentials_t` (public API struct) | Stack / static | 97 B |
| Flash record struct (`wifi_flash_record_t`) | Stack (flash.c internal) | 106 B |
| Connection attempt state + `error_reason` + `pending_connect` | Static globals | ~100 B |
| DNS response buffer | Static | ~512 B |
| **Subtotal (non-lwIP)** | | **~1.6 KB** |
| lwIP heap (`MEM_SIZE` in `lwipopts.h`) | Heap slab | 16 KB (recommended) |
| lwIP MEMP pools (TCP, PCB, pbuf) | Static pools | ~4 KB (estimated) |
| **Total new SRAM for this feature** | | **~22 KB** |

**RP2040 SRAM headroom**: 264 KB total. Current firmware (PIO blink skeleton) uses minimal data RAM (< 4 KB). After this feature: ~26 KB used, leaving ~238 KB for future POV frame buffers and OS structures.

> **Note**: When POV frame buffer allocations are designed, the combined budget of WiFi + LED buffers must be verified against the 264 KB limit and re-documented here.
