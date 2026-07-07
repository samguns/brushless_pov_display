# HTTP Reliability Contract

## Scope

This contract describes externally visible behavior of the STA management portal under idle, stalled, and normal browser connections. Existing route payloads are unchanged.

## Client lifecycle behavior

### Silent client

- **Given** a TCP client connects to port 80 and sends no request bytes
- **When** the idle timeout expires
- **Then** the device closes or aborts that client and frees the portal client slot
- **And** a later valid HTTP request can be accepted

### Stalled response client

- **Given** a client starts a valid request and the device begins a response
- **When** response queueing or acknowledgments stop for longer than the progress timeout
- **Then** the device closes or aborts that client and frees the portal client slot
- **And** a later valid HTTP request can be accepted

### Active client

- **Given** a client is still sending request bytes or acknowledging response bytes
- **When** each progress event arrives within the timeout window
- **Then** the device keeps the connection alive until the response completes

## Existing route compatibility

The following routes continue to return their existing response types:

- `GET /`
- `GET /status`
- `GET /settings`
- `GET /wifi`
- `GET /settings?scan=1`
- `POST /display`
- `POST /config`
- `GET /update`
- `POST /update`
