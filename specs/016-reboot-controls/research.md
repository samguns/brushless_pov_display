# Research: Reboot Controls

## Decision: use a deferred watchdog reboot for normal restart

**Decision**: Accept a manual reboot request in the STA HTTP handler, send and
flush an acknowledgement, then call `watchdog_reboot(0, 0, 1)` from the portal
poll path and remain in a non-returning tight loop until reset.

**Rationale**: A watchdog reboot performs a normal boot through the installed
bootloader and active application, preserves flash-backed settings, and is
already supported by the linked Pico SDK watchdog dependency. Deferring the
operation follows the established USB-update response-flush pattern and keeps
reset work out of the raw TCP receive callback.

**Alternatives considered**: `reset_usb_boot()` was rejected because it enters
USB BOOTSEL rather than normal operation. Calling the FOTA perform path was
rejected because manual reboot must not install firmware. Direct reset-register
writes, reset-vector jumps, and a watchdog timeout were rejected as lower-level,
less explicit, or liable to misclassify the reboot reason.

## Decision: expose a confirmation route and a POST mutation

**Decision**: Add `GET /reboot` for confirmation and `POST /reboot` for the
manual restart request. Successful acceptance returns an acknowledgement before
the connection drops; cancel returns to Settings without changing device state.

**Rationale**: A separate confirmation page makes the interruption explicit,
keeps mutation off GET, meets the two-interaction discovery target, and works
without depending on browser-only confirmation behavior.

**Alternatives considered**: A direct Settings POST was rejected because it
provides weaker protection against accidental activation. A JavaScript-only
confirmation was rejected because server-rendered confirmation remains usable
and testable without relying on script execution.

## Decision: unify deferred reboot state and de-duplicate requests

**Decision**: Replace the USB-specific pending boolean with one fixed pending
action that can represent none, normal reboot, or USB BOOTSEL reboot. Only the
idle state may accept a new reboot action.

**Rationale**: One state makes normal and USB reboot mutually exclusive,
authoritatively prevents repeated requests from scheduling multiple actions,
and lets both routes reuse the established bounded response-flush sequence.

**Alternatives considered**: Separate booleans were rejected because invalid
combinations would be representable. Executing reset directly in the receive
callback was rejected because the browser might never receive acknowledgement.

## Decision: interlock manual reboot with firmware update state

**Decision**: Treat OTA receiving, validating, and ready-to-restart states as
busy. Settings renders Reboot unavailable with a reason, and direct reboot
requests are rejected with `409 Conflict`. Idle, failed, and rolled-back update
states permit manual reboot.

**Rationale**: Receiving and validating touch the inactive firmware slot, while
ready means an OTA restart is already committed. Rendering and enforcing the
same predicate prevents stale-page and direct-request races without aborting an
update.

**Alternatives considered**: Checking only the UI was rejected because clients
can replay a POST. Allowing reboot in the ready state was rejected because it
competes with the scheduled FOTA transition. Aborting OTA in favor of reboot was
rejected because it weakens update safety.

## Decision: remove frequency, retain Blink readiness

**Decision**: Remove the synthetic blink-frequency scalar, parameters, Overview
card, and `frequency_hz` JSON member end-to-end. Retain the Blink Active/Idle
status and `blink.active`, which currently represent display-driver readiness.

**Rationale**: The published 10 Hz/1 Hz value is not a measured LED or POV
frequency and has no operational meaning. The separate active state remains a
useful display-health signal and is protected by the specification's regression
requirements.

**Alternatives considered**: Hiding only the card was rejected because it leaves
dead runtime state and still exposes frequency through `/status`. Removing all
blink state was rejected as broader than requested. Returning frequency as zero
or null was rejected because it remains a forbidden placeholder/equivalent.

## Decision: reuse bounded buffers and existing module boundaries

**Decision**: Add page builders and route coordination inside the existing STA
web/HTTP modules, reuse the 16 KiB static response buffer, and add no heap,
persistent storage, or build dependency.

**Rationale**: Presentation belongs in `wifi_sta_web`, transport and deferred
actions in `wifi_sta_http`, update-state ownership in `wifi_firmware_update`, and
runtime publication in `wifi_config`. Removing two frequency scalars offsets the
small fixed pending-action change, so static RAM must not increase overall.

**Alternatives considered**: A new service/module was rejected as unnecessary
for one bounded control state. Client-side external assets and dynamic page
allocation were rejected because the portal is self-contained and deterministic.

## Decision: validate pure presentation plus hardware reset behavior

**Decision**: Add focused host coverage for the page/JSON builders and any pure
availability/state-transition helper, then validate HTTP sequencing and actual
reset/persistence on hardware. Continue to use `ninja -C build`, size/symbol
inspection, and `git diff --check` as release gates.

**Rationale**: Host tests cheaply prove removal and UI/contract rendering, while
only hardware can prove browser acknowledgement, watchdog reset mode, boot
timing, saved settings, and OTA exclusion across the complete network stack.

**Alternatives considered**: Raw-lwIP mocking for every route was rejected as
disproportionate unless coordination cannot be isolated. Hardware-only testing
was rejected because builder regressions are deterministic and host-testable.
