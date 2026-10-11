# Device Library

The **Device Library** dock indexes a folder tree of boardviews and schematics so you can find
and open a board — and its matching schematic — in one click, without hunting through folders.

## Roots

The library scans one or more **roots**. The roots are resolved in this order:

1. the `W2R_LIBRARY_ROOTS` environment variable (colon-separated),
2. the roots saved in **File → Preferences**,
3. the default `~/Projects/Schematics`, if it exists.

```sh
W2R_LIBRARY_ROOTS=/mnt/boards:/mnt/schematics ./build/w2r-open
```

Editing the roots in Preferences saves them for next time.

## What gets indexed

- Boardview files (`.brd`, `.bvr`) and schematic PDFs.
- Files **inside archives** (`.rar`, `.zip`, `.7z`) — see below.

Each discovered file is grouped into a *library entry* (usually one board + its schematic), and
the entry is tagged with any **board numbers** found in the file name — Apple-style (`820-02443`),
`NM-…`, `80-…`, and short designators like `A2442`. These tags are what search matches against.

## Archives

Collections are often stored as `.rar`/`.zip`/`.7z` archives containing a boardview plus its
schematic. W2R Open lists archive members using the **XAD tools** (`lsar` for listing, `unar` for
extraction), which ship with most distros and support RAR5.

- Nothing is extracted until you actually open a board.
- Extracted members land in a cache keyed by archive path:
  `~/.cache/w2r-open/extract/<hash>/`.

If `lsar`/`unar` are not installed, archive support is disabled and the app reports it (visible
in `w2r-open --library` output). Install the `unar`/`lsar` package for your distro to enable it.

## Using it

- The dock lists the indexed entries; type in the search box to filter by brand, name or board
  number.
- Selecting an entry opens the board; if the entry has a schematic, it is opened in the PDF pane
  at the same time.
- The dock has a status line (e.g. *"Library ready: N packages"*) and reports indexing progress
  on first run, which can take a minute for a large tree.

## From the CLI

```sh
w2r-open --library "820-02443"     # index, then search and list hits
w2r-open --resolve "820-02443"     # match → extract → parse, and report the result
```

`--library` also prints the archive backend status (`enabled`/`DISABLED` with a reason).
`--resolve` is a good way to confirm a query maps to a loadable board.

## Font size

The library and Net Inspector tree fonts can be sized from **View → Tree Font**
(*Increase* / *Decrease* / *Reset*). The choice is saved.
