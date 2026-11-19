#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# RUN upstream's QtTest unit tests (unit_tests/, built by build.sh with normal flags into
# mayhem/build/tests). Each binary prints "Totals: P passed, F failed, S skipped, ..." — the counts
# come from those lines, so a binary that prints no Totals line (crashed, or a no-op) counts as failed.
export QT_QPA_PLATFORM=offscreen
TESTDIR="$SRC/mayhem/build/tests"
scratch=$(mktemp -d); trap 'rm -rf "$scratch"' EXIT
export HOME="$scratch" XDG_CONFIG_HOME="$scratch" XDG_RUNTIME_DIR="$scratch"
P=0; F=0; S=0
for t in TestIsoTpHandler TestUdsHandler TestObd2Pids TestObd2Poller TestObd2Window TestObd2Dbc \
         TestFileDialogSplit TestLogNotes TestLogNotesUi; do
  if [ ! -x "$TESTDIR/$t" ]; then echo "missing $TESTDIR/$t (build.sh should have built it)"; F=$((F+1)); continue; fi
  out=$(cd "$scratch" && "$TESTDIR/$t" 2>&1); rc=$?
  tot=$(printf '%s\n' "$out" | grep -E '^Totals: [0-9]+ passed, [0-9]+ failed, [0-9]+ skipped' | tail -1)
  if [ -z "$tot" ]; then
    echo "FAIL $t: no Totals line (exit $rc)"; printf '%s\n' "$out" | tail -20; F=$((F+1)); continue
  fi
  p=$(echo "$tot" | sed -E 's/^Totals: ([0-9]+) passed, ([0-9]+) failed, ([0-9]+) skipped.*/\1/')
  f=$(echo "$tot" | sed -E 's/^Totals: ([0-9]+) passed, ([0-9]+) failed, ([0-9]+) skipped.*/\2/')
  s=$(echo "$tot" | sed -E 's/^Totals: ([0-9]+) passed, ([0-9]+) failed, ([0-9]+) skipped.*/\3/')
  [ "$rc" -ne 0 ] && [ "$f" -eq 0 ] && f=1
  echo "$t: $tot (exit $rc)"
  [ "$f" -ne 0 ] && printf '%s\n' "$out" | grep -E '^FAIL|Loc:' | head -20
  P=$((P+p)); F=$((F+f)); S=$((S+s))
done
emit_ctrf "qtest" "$P" "$F" "$S"
