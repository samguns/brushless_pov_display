# Research: WiFi Firmware Update

**Date**: 2026-07-11  
**Feature**: 011-wifi-firmware-update

## RES-001: Atomic RP2040 WiFi update mechanism

**Decision**: Vendor a pinned MIT-licensed
[`pico_fota_bootloader`](https://github.com/JZimnol/pico_fota_bootloader) under
`deps/pico_fota_bootloader`, with SHA-256 checking and rollback enabled.

**Rationale**: It is Pico SDK 2.2.0 compatible and provides bootloader, active/download
slots, aligned writes, SHA validation, slot-valid marking, commit, and rollback when the new
image is not committed at its first healthy boot.

**Alternatives considered**: USB BOOTSEL alone (not WiFi); single-slot self-flash (power loss
can remove all bootable firmware); bespoke bootloader (duplicates established RP2040 logic).

## RES-002: Persistent settings with FOTA layout

**Decision**: Configure the bootloader end-of-flash reserved block as 8 KB (required for
two 4 KB-aligned equal slots) and keep credentials, token, and brightness at the existing
final 4 KB offset `PICO_FLASH_SIZE_BYTES - 4096`.

**Rationale**: This preserves existing provisioned devices' offset and prevents either
application slot from erasing settings.

**Alternatives considered**: Store settings in a slot (erased during update); move settings
(breaks migration compatibility).

## RES-003: Browser upload transport

**Decision**: Use one streaming `multipart/form-data` `POST /update`, plus `GET /update`
and `GET /update/status`. Incrementally parse only headers/boundaries and forward file bytes
to the FOTA module.

**Rationale**: The current 1 KB raw-lwIP request buffer cannot retain firmware. A bounded
state machine handles disconnects/timeouts without heap allocation and browser polling gives
plain-language progress.

**Alternatives considered**: Full request accumulation (too small); base64/form encoding
(larger and more complex).

## RES-004: Package validation and board compatibility

**Decision**: `tools/package_firmware.py` wraps the FOTA `.bin` in a fixed 256-byte
`.povota` envelope: magic/version, target board ID, payload length, build ID, and CRC. The
inner image retains its FOTA SHA-256 trailer.

**Rationale**: The envelope produces a clear pre-install wrong-board/size failure; FOTA SHA
detects corruption/truncation. Only the payload is written in 256-byte chunks.

**Alternatives considered**: Bare `.bin` (no readable compatibility metadata); filename
suffix (not device-validated); install before validation (violates FR-007).

## RES-005: Authorization and first migration

**Decision**: Require the existing stored admin token for every FOTA mutation, retain USB
BOOTSEL recovery, and document the one-time USB bootloader migration.

**Rationale**: The legacy update endpoint is currently unauthenticated, contrary to FR-003.
The ROM bootloader is USB/PICOBOOT only, so a deployed single-slot application cannot safely
install a permanent rollback bootloader through the normal WiFi flow.

**Alternatives considered**: Open updates (unsafe); promise USB-free migration (unsafe and
misleading).
