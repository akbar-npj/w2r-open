# RFFE Probe

**RFFE** (Radio Frequency Front End) is the MIPI RFFE bus that phones use to talk to their RF
front-end components (power amplifiers, antenna tuners, switches). W2R Open can drive a small
**ESP32 USB-serial probe** that scans a phone's RFFE bus, then map the detected devices back onto
the boardview as test points.

This is a **local-only** feature — no network or account is involved. The probe firmware is open
source (see `firmware/rffe-probe/`).

## Hardware

- An **ESP32** board (ESP32-WROOM, 4 MB flash) wired to the phone's RFFE bus (CLK, DATA, 1V8) and
  connected to the PC over USB serial.
- The bundled, clean-room firmware image is `firmware/rffe-open-1.0.0.bin`, described by
  `firmware/manifest.json` (version, file, sha256, flash offset `0x10000`, chip, flash size).
- The original vendor's wiring sheet is referenced from the panel help as
  `docs/ESP32_RFFE.pdf`.

The probe is auto-detected by enumerating serial ports; `w2r-open --rffe-info` reports whether
serial support is available, the bundled firmware path, the flash offset, the discovered
`esptool`, and every serial port (with USB VID/PID).

## The RFFE panel

Open the panel from the **RFFE → Open RFFE Panel** menu (`Ctrl+Shift+R`) or the toolbar
**RFFE Probe** button. The panel is a dark, card-styled dock that matches the original app.

### Connecting and scanning

1. **Select a COM Port** from the port dropdown (hit **Refresh** if a port was just plugged in).
2. **Connect** to the probe.
3. **Scan RFFE Modules** to scan the phone's bus. A status LED and a terminal log show progress
   (*Connecting to the probe*, *Identifying the probe*, *Scanning RFFE bus*, …).
4. Detected devices appear under **Detected Devices**; each shield's catalogue devices appear
   under **Expected Devices**.

A scan cannot run while a firmware update is in progress, and vice versa.

### Shields and test points

A **shield** groups the expected devices and the mapped test points for one board. The panel lets
you:

- **Add / Rename / Remove / Hide** shields (hidden shields show as `name (hidden)`).
- See each shield's **expected devices** and its mapped **test points**, with a counter like
  *"12 of 40 pads mapped on this board"*.
- **Get Test Point** — pick a pad on the boardview and add it to the current shield.
- **Map device to net** — associate a detected device with a net.
- Assign a role to a test point: **CLK**, **DATA** or **1V8**.
- **Blink** the marked nets on the canvas (the *RFFE blink* animation) and **Export RFFE test
  points** to a file.

Marked nets are highlighted and can blink on the PCB. The RFFE menu also offers **Blink RFFE
Nets** and **Export RFFE Test Points…**.

### Where the data is stored

The shields and test points are saved per user as **`rffe_testpoints.json`** under
`QStandardPaths::AppLocalDataLocation` (e.g. `~/.local/share/…/rffe_testpoints.json`). It is not
shipped — it is read and written at runtime.

## RFFE settings

**RFFE → RFFE Settings…** (or the gear button) opens the marking/animation options:

| Setting | Default | Range |
|---------|---------|-------|
| Net & component highlight blink | 450 ms | 150 – 1400 ms (0 disables) |
| Hold mark for | 0 ms | 1000 – 120000 ms (0 = don't hold) |
| Halo ring around marked pads | on | — |
| Probe is an ESP32 | on | — |
| Beep when a scan or update starts and ends | on | — |

Values are clamped to the ranges above on save.

## Updating the probe firmware

**RFFE → Update Probe Firmware…** flashes the bundled image using **esptool**. The dialog shows
the staged progress recovered from the original app: *Attach flash*, *Negotiating speed*,
*Erasing flash*, *Writing firmware*, *Verifying*, *Restarting the probe*, ending in
*"Firmware updated. The probe has restarted."*

Before flashing, the image is validated against `manifest.json`: the manifest must exist and be
readable, name an image inside the firmware folder, carry a `sha256` that matches the file, and
the file must be large enough and start with the ESP32 image marker (`0xE9`) and target a 4 MB
flash chip. Each failure reports the specific reason and **writes nothing**.

Requirements:

- **esptool** must be installed (`pip install esptool`) and on `PATH`. `w2r-open --rffe-info`
  shows whether it was found.
- The probe's COM port must be selected.
- Override the firmware image for testing with `W2R_RFFE_FIRMWARE=/path/to/image.bin`.

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| "Pick the probe's COM port first." | Choose the port in the dropdown. |
| No ports listed | Plug the probe in, hit **Refresh**; check the USB-serial driver. |
| esptool not found | `pip install esptool` (or install your distro package). |
| Scan never starts | A firmware update may be running; wait for it to finish. |
| Firmware rejected | The image/manifest failed validation — read the message (hash, marker, flash size). |
| Probe not detected after flashing | The probe restarts; re-select the port and reconnect. |

## Verify your setup

```sh
w2r-open --rffe-info              # serial support, firmware, esptool, ports
w2r-open --rffe-selftest          # shields/test-points store round-trip
w2r-open --rffe-palette-selftest  # panel palette + stylesheet round-trip
```
