# Parser smoke tests against real sample boardviews.
# Override with -DW2R_SAMPLE_DIR=/path/to/samples if the defaults don't exist.
set(W2R_SAMPLE_DIR "/home/shaanair/Projects/Schematics/Apple"
    CACHE PATH "Directory containing sample boardviews for tests")

add_test(NAME stats_brd
         COMMAND $<TARGET_FILE:w2r-open> --stats
                 "${W2R_SAMPLE_DIR}/A2442 820-02443 Boardview.brd")
add_test(NAME stats_bvr3
         COMMAND $<TARGET_FILE:w2r-open> --stats
                 "${W2R_SAMPLE_DIR}/A2442 820-02443_v2 BVR.bvr")

# Diode-sheet import. These exercise BRD pin-name recovery (the BRD has no pin numbers; they
# are recovered from the sibling BVR by coordinate correlation) and both sheet parsers.
# --diode exits non-zero unless at least one reading matched.
add_test(NAME diode_csv
         COMMAND $<TARGET_FILE:w2r-open> --diode
                 "${W2R_SAMPLE_DIR}/A2442 820-02443 Boardview.brd"
                 "${CMAKE_CURRENT_SOURCE_DIR}/data/diode.csv")
add_test(NAME diode_json
         COMMAND $<TARGET_FILE:w2r-open> --diode
                 "${W2R_SAMPLE_DIR}/A2442 820-02443 Boardview.brd"
                 "${CMAKE_CURRENT_SOURCE_DIR}/data/diode.json")

# RFFE probe environment: serial support, bundled firmware and esptool discovery.
add_test(NAME rffe_info COMMAND $<TARGET_FILE:w2r-open> --rffe-info)
