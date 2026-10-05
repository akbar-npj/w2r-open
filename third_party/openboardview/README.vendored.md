# Vendored: OpenBoardView boardview parsers

These files are adapted from **OpenBoardView** (<https://github.com/OpenBoardView/OpenBoardView>),
which is **MIT licensed** — see `LICENSE` in this directory. MIT is permissive, so it does **not**
impose a copyleft obligation on W2R Open; our own code may be licensed separately.

## What was taken
`FileFormats/`:
- `BRDFileBase.{h,cpp}` — shared parser base (parts/pins/nails/outline, mils)
- `BRDFile.{h,cpp}` — obfuscated BRD (`.brd`), signature `{23 e2 63 28}`, decode `~rotl8(c,2)`
- `BVR3File.{h,cpp}` — `BVRAW_FORMAT_3` (`.bvr`)

## What was changed
- Upstream `utils.h`/`utils.cpp` depend on SDL2 and a ghc::filesystem fallback. We replaced them
  with `shim/utils.h`, `shim/utils.cpp` and `shim/filesystem_impl.h` (std::filesystem only).
  The helper behaviour is identical; the only omission is SDL logging.
- Upstream `utf8/utf8.h` is a git submodule (utf8-cpp). We provide a minimal, self-contained
  `utf8/utf8.h` implementing the single `utf8valid()` function that `BRDFileBase.cpp` uses.
- The parser `.cpp` files themselves are unmodified.

## Adding more formats
Drop additional `FileFormats/*.cpp` files here and add them to `obr_parsers` in the top-level
`CMakeLists.txt`. `ASCFile` (Allegro split-sets), `CADFile` (GenCAD), `BDVFile`, `FZFile`,
`CSTFile`, `ADFile`, `BRD2File` and `XZZPCBFile` are candidates for later phases.
