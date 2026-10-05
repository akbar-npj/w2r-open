cmake_minimum_required(VERSION 3.21)
project(w2r-open VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "Offline schematic & PCB boardview viewer")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
qt_standard_project_setup()

# --- Optional Qt modules (enabled as later phases land) ---------------------------
find_package(Qt6 QUIET COMPONENTS Pdf PdfWidgets Svg SerialPort)
if(Qt6Pdf_FOUND AND Qt6PdfWidgets_FOUND)
  set(W2R_HAVE_PDF ON)
endif()
if(Qt6Svg_FOUND)
  set(W2R_HAVE_SVG ON)
endif()
if(Qt6SerialPort_FOUND)
  set(W2R_HAVE_SERIALPORT ON)
endif()

# --- Vendored OpenBoardView parsers (MIT) ------------------------------------------
# Only the format parsers and a tiny shim are vendored; see third_party/openboardview/.
add_library(obr_parsers STATIC
  third_party/openboardview/FileFormats/BRDFileBase.cpp
  third_party/openboardview/FileFormats/BRDFile.cpp
  third_party/openboardview/FileFormats/BVR3File.cpp
  third_party/openboardview/shim/utils.cpp
)
target_include_directories(obr_parsers PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/openboardview/FileFormats
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/openboardview
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/openboardview/shim
)
target_compile_options(obr_parsers PRIVATE -w) # upstream code is warning-noisy

# --- Application -------------------------------------------------------------------
qt_add_executable(w2r-open
  src/main.cpp
  src/core/Board.cpp
  src/core/BoardLoader.cpp
  src/core/DiodeSheet.cpp
  src/core/PinNameRecovery.cpp
  src/core/Settings.cpp
  src/core/Theme.cpp
  src/core/formats/ObAdapter.cpp
  src/core/library/ArchiveBackend.cpp
  src/core/library/LibraryIndex.cpp
  src/core/rffe/RffeFlasher.cpp
  src/core/rffe/RffeProbe.cpp
  src/ui/LibraryPanel.cpp
  src/ui/MainWindow.cpp
  src/ui/NetInspector.cpp
  src/ui/PcbView.cpp
  src/ui/PdfView.cpp
  src/ui/PreferencesDialog.cpp
  src/ui/RffePanel.cpp
)

# Embedded builtin PCB themes (imported from the original app).
qt_add_resources(w2r-open "resources"
  PREFIX "/resources"
  BASE "src/resources"
  FILES src/resources/themes.json
)
target_include_directories(w2r-open PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Lets an uninstalled build locate the bundled probe firmware without a QApplication.
target_compile_definitions(w2r-open PRIVATE W2R_FIRMWARE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/firmware")
target_link_libraries(w2r-open PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent obr_parsers)

if(W2R_HAVE_PDF)
  target_compile_definitions(w2r-open PRIVATE W2R_HAVE_PDF)
  target_link_libraries(w2r-open PRIVATE Qt6::Pdf Qt6::PdfWidgets)
endif()
if(W2R_HAVE_SVG)
  target_compile_definitions(w2r-open PRIVATE W2R_HAVE_SVG)
  target_link_libraries(w2r-open PRIVATE Qt6::Svg)
endif()
if(W2R_HAVE_SERIALPORT)
  target_compile_definitions(w2r-open PRIVATE W2R_HAVE_SERIALPORT)
  target_link_libraries(w2r-open PRIVATE Qt6::SerialPort)
endif()

install(TARGETS w2r-open RUNTIME DESTINATION bin)

# Bundled ESP32 RFFE probe firmware (open, MIT) — flashed by the RFFE panel at 0x10000.
install(FILES firmware/rffe-open-1.0.0.bin firmware/manifest.json
        DESTINATION share/w2r-open/firmware)

# Optional translations: install any compiled catalogues. Generate with
#   lupdate src -ts translations/w2r-open_<locale>.ts && lrelease translations/*.ts
file(GLOB W2R_QM_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.qm")
if(W2R_QM_FILES)
  install(FILES ${W2R_QM_FILES} DESTINATION share/w2r-open/translations)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
