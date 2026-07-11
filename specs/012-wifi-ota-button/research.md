# Research

## Decision: retain two explicit update routes

**Decision**: Keep `/update` for USB BOOTSEL and use a new `/ota` route for browser upload.

**Rationale**: The current action remains a recovery path, while a separate action avoids
accidentally changing its established behavior.

**Alternatives considered**: Replacing the USB action was rejected because it removes a
needed migration/recovery route.

## Decision: poll existing session status for visual progress

**Decision**: Render a native progress bar and percentage from the existing safe OTA status
response, polling during upload and validation.

**Rationale**: The device already reports received and expected bytes; using it avoids adding
another buffer or transport and keeps the display responsive after a browser reconnect.

**Alternatives considered**: A purely indeterminate animation was rejected because owners
need evidence that an upload is advancing.
