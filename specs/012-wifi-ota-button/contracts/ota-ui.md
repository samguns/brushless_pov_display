# OTA UI Contract

- `GET /settings`: shows **Update Firmware (USB)** linking to `/update` and **Update Firmware (WIFI)** linking to `/ota`.
- `GET /ota`: shows package selection, restart warning, a visual progress bar with percentage, status, and a USB recovery link.
- `POST /ota`: accepts one compatible package only; duplicate active upload is rejected.
- `GET /ota/status`: returns non-secret session progress and outcome.
