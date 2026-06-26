# Data Model: STA Management Portal & Firmware Update

**Date**: 2026-06-25
**Feature**: 002-sta-portal-fw-update

This feature is primarily behavioural; it introduces no new persisted record formats. It
**reuses** the feature-001 credential record unchanged and adds a build-time memory-layout
constraint plus a few runtime-only state entities.

---

## Entity: Credential Flash Region (reused, unchanged format)

The credential record format is identical to feature 001 (see
`specs/001-wifi-config-webapp/data-model.md`). This feature only adds a **build-time
reservation** so the region cannot be overwritten by a firmware update.

| Property | Value |
|----------|-------|
| Record format | Unchanged: `magic`(4) · `ssid`(33) · `password`(64) · `flags`(1) · `crc32`(4) |
| Byte offset | `PICO_FLASH_SIZE_BYTES - 4096` = `0x1FF000` (unchanged) |
| XIP address | `XIP_BASE + 0x1FF000` = `0x101FF000` (unchanged) |
| Magic (valid) | `0xC0FFEE01` (unchanged) |
| Forward compatibility | New firmware reads records written by old firmware (FR-004) — guaranteed because offset and format are unchanged. |

**Validation rules**: identical to feature 001 (`magic` match → CRC-32 over bytes 0–101 →
non-empty SSID).

---

## Entity: Firmware Boundary (build-time constraint)

A link-time constraint, not a runtime data structure. Enforced by `memmap_wifi_creds.ld`.

| Field | Value | Description |
|-------|-------|-------------|
| `FLASH` ORIGIN | `0x10000000` | Start of flash (unchanged from SDK default). |
| `FLASH` LENGTH (linkable) | `PICO_FLASH_SIZE_BYTES - 4096` = `0x1FF000` (2 044 KB) | Firmware may occupy up to, but not into, the reserved sector. |
| Reserved region | `0x101FF000` … `0x101FFFFF` (last 4 KB) | Holds the credential record; never emitted into the UF2. |
| Build-time check | `ASSERT(__flash_binary_end <= ORIGIN(FLASH) + LENGTH(FLASH), …)` | Fails the link if firmware overflows (FR-003, SC-005). |

**State**: purely static. Either the build passes (firmware fits) or it fails with the
assertion message. No runtime transitions.

---

## Entity: STA Connection Context (runtime only)

Transient state describing the active STA-mode connection, used by the management portal.

| Field | Type | Description | Source |
|-------|------|-------------|--------|
| `ip_addr` | `ip4_addr_t` | DHCP-assigned IPv4 address. | `netif_ip4_addr(STA netif)` |
| `ssid` | `char[33]` | Connected network name. | `load_credentials()` / stored creds |
| `link_status` | enum (`CYW43_LINK_*`) | Current link state, polled each loop. | `cyw43_tcpip_link_status()` |

**Lifecycle**: Populated when `wifi_config_run_sta()` starts; `link_status` refreshed every
poll. On link loss → silent reconnect (≤15 s) → on failure, AP fallback (FR-014).

---

## Entity: Firmware Update Request (runtime only)

Transient state machine for the update-confirmation flow served by the STA portal.

| State | Meaning | Transition |
|-------|---------|------------|
| `IDLE` | Normal STA serving. | `GET /update` → `AWAITING_CONFIRM` |
| `AWAITING_CONFIRM` | Confirmation page shown with 60 s countdown. | `POST /update` → `CONFIRMED`; `GET /` or Cancel → `IDLE`; 60 s elapse (client-side) → `IDLE` |
| `CONFIRMED` | User confirmed; response flushed. | Device calls `reset_usb_boot(0,0)` → enters USB MSD (no return) |

**Notes**:
- The 60 s countdown (FR-009, FR-013) is rendered client-side on the confirmation page. If
  the user takes no action, the page returns to `/` and the device stays in STA mode — no
  reboot occurs (see research RES-003).
- Only a confirmed `POST /update` triggers `reset_usb_boot()` (FR-010); Cancel returns to
  the status page with no disruption (FR-011).

---

## RAM Budget Delta (Constitution Principle IV)

Static allocations **added by this feature**, on top of feature 001's ~22–26 KB.

| Allocation | Type | Size |
|------------|------|------|
| STA page buffer (`s_sta_page_buf`) | Static global | ~4 KB |
| STA header buffer | Static global | 256 B |
| STA request accumulation buffer | Static global | ~1 KB |
| STA connection context + update state | Static globals | < 256 B |
| **Subtotal (new for this feature)** | | **< 6 KB** |

No new lwIP heap or MEMP pools are added (the existing feature-001 lwIP instance is reused).

**Updated WiFi-subsystem total**: ~22–26 KB (feature 001) + < 6 KB (this feature) ≈ **< 32 KB**
of 264 KB SRAM, leaving > 230 KB for future POV frame buffers.
