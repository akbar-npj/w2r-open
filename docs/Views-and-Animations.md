# Views and Animations

## Split views

The central area shows the PCB and the schematic. Switch layout from the toolbar or the
**View** menu:

| Mode | Shortcut | Layout |
|------|----------|--------|
| PCB Only | `Ctrl+1` | just the board |
| PDF Only | `Ctrl+2` | just the schematic |
| PDF \| PCB | `Ctrl+3` | schematic left, board right |
| PDF \| PDF | `Ctrl+4` | two schematics (e.g. two pages/boards) |
| Swap Panes | — | exchange the two panes |

**Full Screen** (`F11`) hides the chrome for maximum board area.

## Canvas animations

The original app has a set of canvas effects. They share a **master switch**, so you can turn
them all off at once:

**View → Animations → Enable Animations** (master), plus:

| Effect | What it does |
|--------|--------------|
| **Spotlight (dim others)** | Darkens everything except the selected net, so it stands out. |
| **Net pulse** | The selected net gently pulses. |
| **Trace reveal** | Traces draw/animate in rather than appearing instantly. |
| **RFFE blink** | Blinks the nets marked as RFFE test points (see [RFFE Probe](RFFE-Probe.md)). |

Each toggle is remembered across sessions. The master switch gates them all — with animations
disabled the canvas is static, which is handy on slow/remote displays.

## Tree font size

The Net Inspector and Device Library trees can be resized for readability:

**View → Tree Font → Increase / Decrease / Reset**.

The chosen point size is saved and applies to both trees.

## Where these are stored

All of the above (view mode preferences, animation toggles, tree font size, window geometry and
dock layout) are persisted via `QSettings`. See
[Preferences and Language](Preferences-and-Language.md).
