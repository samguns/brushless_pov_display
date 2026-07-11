# Implementation Plan: WiFi OTA Button

**Branch**: `master` | **Date**: 2026-07-11 | **Spec**: [spec.md](spec.md)

## Summary

Add an **Update Firmware (WIFI)** action beside the retained USB action in the STA Settings
card. The upload page uses the existing bounded FOTA session status to drive a visible
percentage/progress bar and a polished hierarchy for selection, warning, state, and recovery.

## Technical Context

**Language/Version**: C11/C++17; Pico SDK 2.2.0.  
**Dependencies**: Existing raw-lwIP STA portal, `wifi_firmware_update`, FOTA bootloader.  
**Storage**: Existing inactive FOTA flash slot and final settings sector.  
**Testing**: `cmake --build build`; host package tests; hardware browser validation.  
**Target Platform**: Raspberry Pi Pico W.  
**Project Type**: Embedded firmware and inline management UI.  
**Constraints**: Preserve `/update` USB behavior; no full package in RAM; one upload session;
the progress UI must tolerate zero-byte idle status and reconnect polling.

## Constitution Check

| Principle | Status | Notes |
|---|---|---|
| I–II | PASS | No LED output or timing logic changes. |
| III | PASS | UI, HTTP transport, and FOTA session remain separated. |
| IV | PASS | Existing bounded session buffers are reused. |
| V | PASS | No additional build command. |

## Project Structure

```text
wifi_config/wifi_sta_web.{c,h}   # System buttons, styled OTA page, progress bar
wifi_config/wifi_sta_http.{c,h}  # OTA routes and upload lifecycle
wifi_config/wifi_firmware_update.{c,h} # existing session/status API
specs/012-wifi-ota-button/       # feature documentation
```

**Post-design re-check**: PASS.
