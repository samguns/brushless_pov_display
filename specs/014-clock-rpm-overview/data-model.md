# Data Model: Clock and RPM Overview

## Entity: Clock Status

| Field | Type | Rules |
|---|---|---|
| `available` | Boolean | True only after successful device time calibration during the current boot. |
| `text` | Eight-character time | Valid values use `HH:MM:SS`; unavailable storage is ignored by presentation. |
| `zone` | Fixed label | Always `CST` for the existing UTC+8 clock. |

### State transitions

```text
Uncalibrated --successful time sync--> Calibrated
Calibrated --monotonic second tick----> Calibrated (text advances)
```

## Entity: Rotation Speed Status

Existing status retained from feature 013:

| Field | Type | Rules |
|---|---|---|
| `available` | Boolean | False until the first valid Hall measurement, then remains true. |
| `rpm` | Non-negative whole number | Nearest-whole valid RPM; zero after established rotation becomes stale. |

## Entity: Overview Runtime Snapshot

| Metric | Available presentation | Unavailable presentation |
|---|---|---|
| Current Clock | `<HH:MM:SS> CST` | `--:--:-- CST` |
| Rotation Speed | `<rpm> RPM`, including `0 RPM` when stopped | `-- RPM` |

- The two statuses are independent.
- Values are copied through existing fixed-size runtime boundaries.
- A request renders the latest fully published snapshot; it does not retain a
  pointer to mutable application state.

