# K6 GPO Portal (web UI source)

React + Vite + Tailwind (shadcn/ui) source for the control panel served by the
ESP32 firmware. The firmware does **not** run this build directly — instead the
compiled app is inlined into a single HTML file, gzip-compressed, and embedded
as a byte array in `firmware/src/portal_html.h`, which `handleIndex()` serves
with `Content-Encoding: gzip`.

Every control is wired to the firmware's existing HTTP API (`/api/status`,
`/api/stats`, `/api/files`, `/api/wifi`, `/api/ota`, `/api/terminal`, …); no
server changes are needed to change the look of the portal.

## Requirements

Node 18+ and npm.

## Regenerate the embedded portal

From this directory:

```bash
npm install
npm run build      # vite build -> dist/public/
npm run inline     # -> esp32_portal.html (single self-contained file)

# Compress and turn into the C byte array the firmware embeds:
gzip -9 -n -c esp32_portal.html > esp32_portal.html.gz
python3 - <<'PY'
data=open("esp32_portal.html.gz","rb").read()
out="../../firmware/src/portal_html.h"
L=["// AUTO-GENERATED — do not edit by hand.",
"// Gzip-compressed single-file React portal (source project: tools/portal).",
"// Regenerate: see tools/portal/README.md",
"#pragma once","#include <pgmspace.h>","#include <stddef.h>","",
"const uint8_t PORTAL_HTML_GZ[] PROGMEM = {"]
row=[];body=[]
for b in data:
    row.append("0x%02x,"%b)
    if len(row)==20: body.append("  "+"".join(row)); row=[]
if row: body.append("  "+"".join(row))
L.append("\n".join(body)); L.append("};")
L.append("const size_t PORTAL_HTML_GZ_LEN = %d;"%len(data)); L.append("")
open(out,"w").write("\n".join(L)); print("wrote",out,len(data),"bytes")
PY
```

Then rebuild the firmware (`cd ../../firmware && pio run`) and flash it.

## Local preview against a real device

`npm run build` then open `dist/public/index.html`, or run a dev server and
proxy `/api/*` to a phone box on your network. The app talks to relative
`/api/...` paths, so serving it from the device (or a proxy) is all that's
needed.
