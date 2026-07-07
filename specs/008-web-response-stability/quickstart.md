# Quickstart: Web Response Stability

## Prerequisites

- Firmware build directory already configured.
- Device flashed and connected to Wi-Fi with the STA management portal reachable.
- A browser or simple TCP tool on the same network.

## Build validation

```powershell
ninja -C build
```

Expected result: the firmware target builds successfully.

## Manual validation scenarios

### 1. Normal portal loads

1. Open `http://<device-ip>/`.
2. Refresh the page 20 times.
3. Open `http://<device-ip>/settings`.

Expected result: each page returns a complete response without rebooting the device.

### 2. Silent client recovery

1. Open a raw TCP connection to `<device-ip>:80` and send no bytes.
2. Wait at least 6 seconds.
3. Open `http://<device-ip>/` in a browser.

Expected result: the browser receives the Overview page.

### 3. Stalled client recovery

1. Start a request to `http://<device-ip>/settings`.
2. Stop reading from the socket before the full response completes.
3. Wait at least 11 seconds.
4. Request `http://<device-ip>/status`.

Expected result: the status JSON response succeeds.

### 4. Existing actions

1. Change brightness from Settings.
2. Open the firmware update confirmation page, then cancel.
3. Use Wi-Fi scan if a hardware validation environment is available.

Expected result: existing workflows still respond and do not leave the portal wedged.
