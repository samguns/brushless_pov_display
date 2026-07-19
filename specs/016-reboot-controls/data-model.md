# Data Model: Reboot Controls

## Pending Reboot Action

Represents the single deferred board-reset action owned by the STA HTTP module.

| Field | Values | Rules |
|---|---|---|
| `target` | `none`, `normal`, `usb_bootsel` | Only `none` may transition to a requested target. Normal and USB targets are mutually exclusive. |
| `response_active` | derived from HTTP transmission state | Reset execution waits for the acknowledgement to finish or for the existing bounded flush deadline. |

### State transitions

```text
none --accepted POST /reboot--> normal --flush/grace--> reset
none --accepted POST /update--> usb_bootsel --flush/grace--> reset
normal or usb_bootsel --duplicate request--> unchanged + conflict
```

The action is volatile, fixed-size, and cleared during portal initialization.
It is never persisted and cannot erase or replace configuration.

## Manual Reboot Availability

Derived from firmware-update and pending-action state; no separate persistent
entity is stored.

| Update state | Pending action | Available | Reason |
|---|---|---|---|
| `idle`, `failed`, `rolled_back` | `none` | yes | No firmware transition or reboot is active. |
| `receiving`, `validating`, `ready` | any | no | Firmware data is active or an OTA restart is committed. |
| any | `normal` or `usb_bootsel` | no | A reset has already been accepted. |

The same predicate controls Settings presentation and server-side acceptance so
a stale page cannot bypass the interlock.

## Reboot Presentation

| View | Required content | Action |
|---|---|---|
| Settings System card | Normal-mode purpose, temporary interruption warning, availability reason when blocked | Navigate to confirmation only when available. |
| Confirmation page | Normal reboot warning, settings-preservation statement | POST confirmation or cancel to Settings. |
| Accepted page | Restart accepted, connection will drop, reconnect guidance | No further mutation. |
| Conflict page | Update/reboot in progress explanation | Return to Settings or update status. |

All pages are derived into the existing fixed response buffer and have no
server-side session entity.

## Blink Readiness Status

| Field | Values | Lifecycle |
|---|---|---|
| `active` | boolean | Published from display-driver readiness through application, Wi-Fi runtime, HTTP snapshot, Overview, and `/status`. |

The former frequency scalar and `frequency_hz` representation are removed. Hall
rotation frequency and RPM are unrelated and remain unchanged.
