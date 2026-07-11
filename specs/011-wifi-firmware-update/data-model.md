# Data Model: WiFi Firmware Update

## Firmware package envelope

The browser submits `.povota`: a fixed 256-byte envelope followed by the bootloader-generated
FOTA image.

| Field | Size | Validation |
|-------|------|------------|
| magic | 8 B | Exact `POVOTA01` |
| format version | 1 B | Supported version only |
| board ID | 16 B | Exact compiled target ID |
| payload length | 4 B | Non-zero and at most download-slot capacity |
| build ID | 32 B | Display-safe ASCII identity |
| envelope CRC32 | 4 B | CRC over header fields |
| reserved | remainder | Zero in format 1 |
| FOTA payload | variable | Exact declared length and SHA-256 valid |

The envelope is never written to the slot. The payload is streamed in 256-byte chunks; a
partial final chunk is padded only after declared-length validation.

## Flash layout

The pinned dependency's linker scripts are the source of exact addresses; application code
uses its exported limits, never duplicate slot literals. Its dual-slot alignment requires an
8 KB reserved tail; the final 4 KB retains the existing settings offset and the preceding
4 KB is intentionally unused/reserved.

| Region | Owner | Lifecycle |
|--------|-------|-----------|
| Bootloader + swap metadata | FOTA dependency | USB migration; not changed by normal update |
| Active slot | FOTA bootloader | Known-good application |
| Download slot | update module via FOTA API | Invalid until full SHA validation |
| Reserved 8 KB tail (final 4 KB settings) | `wifi_flash` | Credentials/token/brightness persist across swaps |

## Update session

| Field | Type | Purpose |
|-------|------|---------|
| state | enum | Lifecycle below |
| authorized | bool | Stored admin token was presented |
| expected / received bytes | `uint32_t` | Progress and length enforcement |
| board / build ID | bounded header values | Compatibility and post-restart identity |
| last error/result | enum/string key | Plain-language UI response |

```text
IDLE → RECEIVING_HEADER → RECEIVING_PAYLOAD → VALIDATING → READY_TO_REBOOT
  └──────────────────────────────────────────────→ FAILED
READY_TO_REBOOT → RESTARTING → PENDING_COMMIT → SUCCESS | ROLLED_BACK
```

Disconnect, timeout, malformed input, duplicate, incompatibility, oversize, CRC, or SHA
failure invalidates/discards the download and retains the running application.

## Memory budget delta

| Allocation | Maximum |
|------------|---------|
| Upload metadata/parser | 1 KB |
| Aligned FOTA staging buffer | 256 B |
| Session/status state | <512 B |
| New static RAM | <2 KB |

The existing 16 KB page buffer is reused; firmware bytes reside in flash slots.
