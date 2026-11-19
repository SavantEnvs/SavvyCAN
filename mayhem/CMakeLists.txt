# Fuzz harnesses for SavvyCAN. Builds the file-loading sources (the same set upstream's
# unit_tests/CMakeLists.txt links for TestFileDialogSplit / TestLogNotes) into one static library
# plus the ISO-TP/UDS protocol handlers, compiled with the fuzz flags from CMAKE_CXX_FLAGS, then one libFuzzer binary and one -standalone
# reproducer per target.
cmake_minimum_required(VERSION 3.21)
project(SavvyCAN_fuzz LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 CONFIG REQUIRED COMPONENTS Core Gui Widgets SerialBus)

set(SAVVY ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(LIB_FUZZING_ENGINE "-fsanitize=fuzzer" CACHE STRING "libFuzzer link flag")
set(STANDALONE_MAIN_OBJ "" CACHE FILEPATH "compiled StandaloneFuzzTargetMain object")
set(LSAN_OFF_OBJ "" CACHE FILEPATH "compiled lsan_off.cc object (leak detection off)")
set(FUZZ_OUT_DIR "${CMAKE_BINARY_DIR}/out" CACHE PATH "where the fuzz binaries are copied")

add_library(savvy_core STATIC
    ${SAVVY}/framefileio.cpp ${SAVVY}/framefileio.h
    ${SAVVY}/lognotes.cpp ${SAVVY}/lognotes.h
    ${SAVVY}/dbc/dbchandler.cpp ${SAVVY}/dbc/dbchandler.h
    ${SAVVY}/dbc/dbc_classes.cpp ${SAVVY}/dbc/dbc_classes.h
    ${SAVVY}/blfhandler.cpp ${SAVVY}/blfhandler.h
    ${SAVVY}/pcaplite.cpp ${SAVVY}/pcaplite.h
    ${SAVVY}/utility.cpp ${SAVVY}/utility.h
    ${SAVVY}/can_structs.h
    ${SAVVY}/bus_protocols/isotp_handler.cpp ${SAVVY}/bus_protocols/isotp_handler.h
    ${SAVVY}/bus_protocols/uds_handler.cpp ${SAVVY}/bus_protocols/uds_handler.h
    ${SAVVY}/canfilter.cpp ${SAVVY}/canfilter.h)
target_include_directories(savvy_core PUBLIC ${SAVVY} ${SAVVY}/dbc ${SAVVY}/bus_protocols
                                             ${SAVVY}/connections ${SAVVY}/utils)
target_link_libraries(savvy_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::SerialBus)

function(savvy_fuzzer name src filters)
    foreach(kind fuzz standalone)
        if(kind STREQUAL "fuzz")
            set(exe ${name})
        else()
            set(exe ${name}-standalone)
        endif()
        add_executable(${exe} ${src} ${LSAN_OFF_OBJ})
        set_target_properties(${exe} PROPERTIES AUTOMOC OFF)
        target_compile_definitions(${exe} PRIVATE "FUZZ_FILTERS=${filters}")
        target_link_libraries(${exe} PRIVATE savvy_core)
        if(kind STREQUAL "fuzz")
            target_link_options(${exe} PRIVATE ${LIB_FUZZING_ENGINE})
        else()
            target_sources(${exe} PRIVATE ${STANDALONE_MAIN_OBJ})
        endif()
        add_custom_command(TARGET ${exe} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${FUZZ_OUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${exe}> ${FUZZ_OUT_DIR}/)
    endforeach()
endfunction()

# Frame-log loaders (FrameFileIO::loadFrameFileAs filter indexes, framefileio.cpp loadFilters()).
savvy_fuzzer(fuzz_autodetect fuzz_frames.cpp "0")
savvy_fuzzer(fuzz_pcan       fuzz_frames.cpp "11")
savvy_fuzzer(fuzz_blf        fuzz_frames.cpp "15")
savvy_fuzzer(fuzz_wireshark  fuzz_frames.cpp "24,25")
savvy_fuzzer(fuzz_asc        fuzz_frames.cpp "14")
savvy_fuzzer(fuzz_clx000     fuzz_frames.cpp "22")
savvy_fuzzer(fuzz_binlogs    fuzz_frames.cpp "23,21,8")
savvy_fuzzer(fuzz_textlogs   fuzz_frames.cpp "1,2,3,4,5,6,7,9,10,12,13,16,17,18,19,20,26,27")
# Signal-definition importers (DBCHandler::loadFileAs filter indexes).
savvy_fuzzer(fuzz_dbc          fuzz_dbc.cpp "0")
savvy_fuzzer(fuzz_dbc_json_csv fuzz_dbc.cpp "1,2")
# Log-notes sidecar / embedded block.
savvy_fuzzer(fuzz_lognotes fuzz_lognotes.cpp "0")
# ISO-TP reassembly + UDS decoding of a CAN frame stream.
savvy_fuzzer(fuzz_isotp_uds fuzz_isotp_uds.cpp "0")
