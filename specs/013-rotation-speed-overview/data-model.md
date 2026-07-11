# Data Model: Rotation Speed Overview

## Entity: Rotation Speed Status

Latest application-owned value exposed to the management UI.

| Field | Type | Rules |
|---|---|---|
| `available` | Boolean | False until the first valid Hall measurement; remains true afterward unless sensing never initialized. |
| `rpm` | Non-negative whole number | Nearest-whole conversion of a valid measurement; zero for stale/stopped rotation after availability. |

### State transitions

```text
Unavailable --first valid sample--> Rotating
Rotating --fresh valid sample------> Rotating (RPM replaced)
Rotating --measurement stale-------> Stopped (0 RPM)
Stopped --fresh valid sample-------> Rotating
Unavailable --no valid sample------> Unavailable
```

## Entity: Overview Rotation Metric

Request-time presentation derived from Rotation Speed Status.

| Status | Label | Display value |
|---|---|---|
| Unavailable | `Rotation Speed` | `-- RPM` |
| Rotating | `Rotation Speed` | `<whole-number> RPM` |
| Stopped | `Rotation Speed` | `0 RPM` |

## Relationships and validation

- Main owns the conversion from Hall measurement to Rotation Speed Status.
- Wi-Fi runtime and HTTP modules copy the latest two scalar fields without
  deriving sensor state.
- The web builder formats only the presentation described above.
- The RPM value is relevant only when `available` is true; unavailable rendering
  ignores the stored numeric field.

