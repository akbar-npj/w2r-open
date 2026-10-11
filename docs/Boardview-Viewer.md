# Boardview Viewer

The PCB canvas renders the physical board — parts, pins, traces and the netlist — from the
boardview files that board-level technicians use to find test points and trace nets.

## Supported formats

| Extension | Format | Notes |
|-----------|--------|-------|
| `.brd` | Obfuscated BRD | The common Apple/phone boardview format. Carries no pin numbers (see below). |
| `.bvr` | `BVRAW_FORMAT_3` | The "raw" format, usually shipped alongside a `.brd`. |

Format detection is by **content sniffing first**, then by extension, so mis-named files still
load. A `.bvr` and its `.brd` are normally opened together — see *Pin-name recovery*.

The parsers are adapted from [OpenBoardView](https://github.com/OpenBoardView/OpenBoardView)
(MIT) and vendored under `third_party/openboardview/`.

## What is rendered

- **Outline** — the board edge.
- **Parts** — component bodies with names, top and bottom sides.
- **Pins** — pads, with ground and no-connect pins drawn in distinct colours.
- **Nets** — net names on pads; selecting a net highlights every pin on it.
- **Ratsnest** — thin lines showing the logical connections of the selected net (toggle with the
  toolbar **Ratsnest** button or the preference).

## Navigating

- **Fit** (`Home`) frames the whole board.
- **Flip side** (`Space`) swaps top/bottom.
- **Rotate** (`R`) rotates 90° clockwise.
- **Zoom** with `Ctrl++` / `Ctrl+-` or the toolbar buttons.
- **Pan** by dragging; the view culls off-screen geometry for speed.

## Pin-name recovery

The obfuscated `.brd` format stores pin *positions* but **not pin names or numbers**. When a
sibling `.bvr` for the same board is present, W2R Open recovers the pin names by **coordinate
correlation**: it matches the `.brd` pads to the `.bvr` pads by position and copies the names
across. This is what makes net/pin lookups, the Net Inspector and diode readings work on `.brd`
boards. Keep the `.brd` and `.bvr` in the same folder.

## Selecting a net

Click a pin or a net label on the canvas. The selection:

- highlights every pin on that net,
- is shown in the **Net Inspector** (right dock), and
- cross-probes into the schematic PDF (see [Schematics](Schematics.md)).

The status bar echoes the selected net or pin.

## Diode overlay

Imported diode/continuity readings can be drawn directly on the pads. See
[Diode Readings](Diode-Readings.md).

## Themes and animations

The canvas colours come from a theme; spotlight / net-pulse / trace-reveal effects are
available. See [Themes](Themes.md) and [Views and Animations](Views-and-Animations.md).

## Statistics from the CLI

```sh
w2r-open --stats board.brd
```

prints format, part/pin/net counts, outline points, board dimensions in mils and mm, and the
named/ground/NC pin breakdown — useful for verifying that a file parsed correctly.
