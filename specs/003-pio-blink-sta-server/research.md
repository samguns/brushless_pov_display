# Research: PIO Blink Concurrent with STA HTTP Server

**Date**: 2026-06-26
**Feature**: 003-pio-blink-sta-server

## RES-001: Concurrency architecture for blink + STA HTTP

**Decision**: Use a single non-blocking superloop in `main` that repeatedly services:
1) WiFi/lwIP poll, 2) STA/AP runtime step, 3) blink runtime step.
`wifi_config_run_sta()` infinite-loop behavior is replaced (or wrapped) by step-style APIs so control returns to `main` every iteration.

**Rationale**:
- Current `wifi_config_run_sta()` never returns, which prevents any blink execution.
- Poll-mode CYW43 (`pico_cyw43_arch_lwip_poll`) already expects frequent non-blocking polling.
- A central loop gives deterministic scheduling and straightforward timing visibility.

**Alternatives considered**:
- Keep blocking STA loop and move blink into WiFi callbacks: rejected; poor separation and timing risk.
- Run dual-core split (core0 WiFi/core1 blink): rejected for this increment; higher complexity and synchronization overhead.
- RTOS task scheduler: rejected; not required for current scope.

## RES-002: Preserve PIO timing while serving HTTP

**Decision**: Keep blink generation fully PIO-driven; CPU only updates blink interval state and writes TX values when phase changes are due. Derive timing from `clock_get_hz(clk_sys)` and avoid hardcoded clock constants.

**Rationale**:
- Matches constitution principles I and II.
- Prevents HTTP load from directly impacting waveform generation path.
- Ensures portability if system clock changes.

**Alternatives considered**:
- Software/GPIO toggling in CPU loop: rejected due to jitter under network load.
- Fixed 125 MHz constant for interval math: rejected by constitution.

## RES-003: STA endpoint authorization behavior

**Decision**: Require shared admin token for mutating/configuration endpoints in STA mode; return HTTP 401 (JSON error) on missing/invalid token; apply invalid-attempt throttling and return HTTP 429 on exceed.

**Rationale**:
- Reflects approved clarifications in the spec.
- Provides clear, testable security behavior for LAN-exposed controls.
- Keeps read-only status endpoints available without authentication.

**Alternatives considered**:
- No auth in STA mode: rejected for security risk.
- Full user/password account system: rejected as out of scope.

## RES-004: Token persistence strategy

**Decision**: Persist admin token in the same flash-managed credential domain as WiFi credentials and update it via provisioning/configuration flow.

**Rationale**:
- Survives reboot/update and avoids compile-time secrets.
- Aligns with existing credential lifecycle and minimizes new storage mechanisms.

**Alternatives considered**:
- Compile-time token only: rejected (requires reflashing for change).
- Serial-at-boot runtime token: rejected (operationally brittle).

## RES-005: Reconnect behavior during blink operation

**Decision**: On STA drop, continue blink runtime while attempting reconnect within current timeout policy; on timeout, fall back to existing AP provisioning path without stopping blink runtime scheduler.

**Rationale**:
- Directly satisfies FR-007/FR-008 and user stories around continuity.
- Reuses existing recovery path from current codebase.

**Alternatives considered**:
- Pause blink during reconnect: rejected; violates primary feature intent.
