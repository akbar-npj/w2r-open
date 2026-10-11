# Schematics

Schematic PDFs (board-level schematics) are rendered in the right pane using **QtPdf** (the
PDFium backend). This is where you trace a net through the schematic while cross-referencing
the board.

> QtPdf is optional. If the build was configured without Qt6::Pdf, the pane shows a placeholder
> instead of the document.

## Viewing

- **Continuous multi-page scrolling** with page navigation.
- **Zoom / fit-to-width**.
- A page spin-box and total-page label.
- Open a schematic with **File → Open Schematic…** or pass it as the second file on the command
  line: `w2r-open board.brd sheet.pdf`.

## Full-text net search

Type a net name (or any text) into the search box:

- Every match is highlighted in the document.
- A match counter shows the position (e.g. *3 / 17*); step through matches with **next** /
  **prev**.
- Searching in the PDF also **highlights the net on the PCB** (the reverse of cross-probing).

## Occurrences panel

Every search hit is listed in the **Occurrences** dock at the bottom of the window: the net, the
page, and a short surrounding-context label per hit. Click an entry to jump the PDF straight to
that match. The panel appears automatically when a search yields hits.

## Cross-probing

The PCB and the schematic are linked in both directions:

- **PCB → PDF:** click a net on the board; the schematic searches for it and jumps to the first
  occurrence.
- **PDF → PCB:** search in the schematic; the matching net is highlighted on the board.

The **Net Inspector** dock lists every net and its pins; selecting one highlights it on both
panes.

## Split views

The PDF pane can be shown alone, next to the PCB, or as a second PDF. See
[Views and Animations](Views-and-Animations.md).
