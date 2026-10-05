# W2R Open RFFE probe firmware

Clean-room, MIT-licensed firmware for the Way2Repair ESP32 RFFE probe. It replaces the
proprietary `rffe-esp32-1.0.1.bin` that shipped with the Windows app, so the project can be
distributed without redistributing vendor binaries.

The prebuilt image `../rffe-open-1.0.0.bin` is the app-partition image and is flashed at
`0x10000`. The bootloader, partition table and `boot_app0` come from the Arduino ESP32 core.

## Hardware

The probe is an ESP32 that sits between the host (USB serial) and a phone's MIPI RFFE bus:

| Signal     | Default GPIO | Notes                                   |
|------------|--------------|-----------------------------------------|
| `RFFE_SCLK`| 19           | Clock, driven by the ESP32              |
| `RFFE_SDATA`| 18          | Bidirectional data (open-drain, pull-up)|
| `RFFE_VIO` | disabled     | Optional GPIO to enable the 1.8 V rail  |
| GND        | —            | Common ground with the phone            |

The pinout is compile-time configurable in `platformio.ini` (or via `-D` build flags).

> **Level shifting:** phone RFFE buses are 1.8 V. Never connect the ESP32's 3.3 V GPIO
> directly to a phone. Use a bidirectional level shifter and confirm the VIO rail before
> connecting. See the vendor wiring diagram (`ESP32_RFFE.pdf`) for the reference circuit.

## Build and flash

```sh
# Build (produces .pio/build/esp32dev/firmware.bin)
pio run

# Flash directly over USB (adjust the port)
pio run -t upload --upload-port /dev/ttyUSB0
```

Or copy `firmware.bin` over `../rffe-open-1.0.0.bin` and use the app's **Update Probe
Firmware** button, which drives esptool at `0x10000`.

## Host protocol

115200 8N1. The host sends one `\n`-terminated ASCII command per line; the probe replies
with `\n`-terminated lines.

| Host → probe  | Probe → host                                              |
|---------------|-----------------------------------------------------------|
| `way2_rffe_`  | `SCAN BEGIN`, then `DEV <usid> <reg0>` per device, then `SCAN END <count>` |
| `PING`        | `PONG`                                                    |
| `VER`         | `W2R-RFFE <version>`                                      |
| `STOP`        | `OK STOP` (aborts a running scan)                         |
| `HELP`        | `OK COMMANDS way2_rffe_ PING VER STOP HELP`               |
| anything else | `ERR UNKNOWN <token>`                                     |

On boot the probe prints a banner: `W2R-RFFE 1.0.0`.

`DEV` fields:

- `usid` — RFFE slave address, 0–15.
- `reg0` — the 8-bit value of Register 0 (device identification), hex.

The scan command `way2_rffe_` is deliberately identical to the vendor firmware, so the same
host code drives either image.

## How the scan works

The firmware bit-bangs a MIPI RFFE master (`src/main.cpp`). For each USID 0–15 it issues a
**Register 0 Read** using the MIPI RFFE command frame:

```
SSC | USID(4) | CMD(3)=010 | P(1) | ADDR(5)=0 | P(1) | turnaround | DATA(8) | P(1) | ACK
```

A device is reported when it acknowledges the read (ACK bit low). Parity is odd, as required
by the specification.

## Validation status

The host protocol is implemented and the firmware **builds cleanly**, but the RFFE timing and
framing have **not been validated against real RF front-end hardware**. `kHalfPeriodUs` in
`src/main.cpp` is a conservative default; bench-test with an oscilloscope and adjust the GPIO
map and timing for your probe board before trusting scan results.

## Licence

MIT. See the repository `LICENSE`.
