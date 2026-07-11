# Data Model

| Entity | Fields | Lifecycle |
|---|---|---|
| Update method | `usb` or `wifi_ota` | Selected from Settings |
| Upload session | state, byte counts, message, build identity | Existing bounded FOTA session |
| Progress presentation | percentage, received/expected bytes, visual state | Derived from upload session while page is open |
| Firmware identity | build ID | Displayed after successful restart |
