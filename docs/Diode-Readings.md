# Diode Readings

Diode-mode (continuity) readings are not stored in the boardview files. W2R Open imports them
from a **JSON or CSV sheet** you supply and overlays them on the matching pads, so you can see
every reading on the board at once.

## Where the data comes from

A diode/continuity sheet is a list of *(component, pin, value)* readings, e.g. from a multimeter
session or a shared repair database. W2R Open matches each reading to a pad by component designator
and pin name — which is why [pin-name recovery](Boardview-Viewer.md#pin-name-recovery) matters on
`.brd` boards.

## JSON formats

Either an object keyed by component, then pin:

```json
{
  "U1001": { "A1": "0.450V", "A2": "OL" },
  "U1002": { "B3": "0.512V" }
}
```

…or an array of records:

```json
[
  { "component": "U1001", "pin": "A1", "diode": "0.450V" },
  { "component": "U1001", "pin": "A2", "diode": "OL" }
]
```

## CSV format

Comma-separated, order `component,pin,diode`. A header row is optional.

```csv
component,pin,diode
U1001,A1,0.450V
U1001,A2,OL
```

Values are free-form strings (`0.450V`, `OL`, `0.6`, …); they are displayed verbatim on the pad.

## Importing in the GUI

**File → Import Diode Readings…** and pick the sheet. The readings are applied to the currently
open board and drawn on the pads. Toggle the overlay with the toolbar **Diode Readings** button or
the preference **Show diode readings on pads**.

## Importing from the CLI

```sh
w2r-open --diode board.brd sheet.json
w2r-open --diode board.brd sheet.csv
```

This applies the sheet and prints how many pins matched plus a few sample readings. It exits
non-zero if nothing matched — handy as a smoke test that a sheet lines up with a board.

## Rendering a preview

```sh
W2R_DIODE=sheet.csv w2r-open --render board.brd out.png
```

renders the board with the readings overlaid, for a quick visual check.
