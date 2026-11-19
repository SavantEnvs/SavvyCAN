#!/usr/bin/env bash
# mayhem/build.sh — SavvyCAN fuzz harnesses + upstream's Qt unit tests.
#  1) mayhem/CMakeLists.txt: the project's file loaders (framefileio, blfhandler, pcaplite, lognotes,
#     dbc/*) compiled with $SANITIZER_FLAGS $DEBUG_FLAGS + libFuzzer coverage, one fuzzer and one
#     -standalone reproducer per target, copied to /mayhem/out/.
#  2) unit_tests/ (upstream's own CMake test project) with NORMAL flags into mayhem/build/tests/, for test.sh.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export CC CXX
cd "$SRC"

# 1) fuzz build
mkdir -p mayhem/build/fuzz
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o mayhem/build/fuzz/standalone_main.o
# leak detection off in every fuzz + standalone binary (ASan/UBSan stay on)
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o mayhem/build/fuzz/lsan_off.o
cmake -S mayhem -B mayhem/build/fuzz -DCMAKE_BUILD_TYPE=None \
      -DCMAKE_CXX_FLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1" \
      -DCMAKE_C_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1" \
      -DCMAKE_EXE_LINKER_FLAGS="$SANITIZER_FLAGS" \
      -DLIB_FUZZING_ENGINE="$LIB_FUZZING_ENGINE" \
      -DLSAN_OFF_OBJ="$SRC/mayhem/build/fuzz/lsan_off.o" \
      -DSTANDALONE_MAIN_OBJ="$SRC/mayhem/build/fuzz/standalone_main.o" \
      -DFUZZ_OUT_DIR="$SRC/out"
cmake --build mayhem/build/fuzz -j"$MAYHEM_JOBS"

# 2) upstream unit tests, normal flags
cmake -S unit_tests -B mayhem/build/tests -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build mayhem/build/tests -j"$MAYHEM_JOBS"
