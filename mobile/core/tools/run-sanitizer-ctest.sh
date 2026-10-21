#!/bin/bash
# Bounded sanitizer test run for stl2stp-core.
#   core/tools/run-sanitizer-ctest.sh <build-dir> <asan|ubsan> [OCCT_ROOT]
#   (compiler from CC/CXX, e.g. CC=gcc-10 CXX=g++-10)
# Guards (added after an unbounded ASan run wrote ~31 GB of logs):
#  * gcc-10 ASan on kernels with high mmap_rnd_bits (e.g. WSL2 6.x) can loop forever printing
#    "AddressSanitizer:DEADLYSIGNAL"; the run is wrapped in `setarch -R` (no ASLR, no root needed).
#  * per-test timeout 120 s, whole run timeout 40 min, console log capped at 20 MB,
#    watchdog kills ctest if the build dir grows beyond 2 GB, Testing/Temporary removed afterwards.
#  * LeakSanitizer is off by default: OCCT 8.0.1 keeps a ~7.6 MB allocation per transfer
#    alive at exit (Standard::Allocate). Set STL2STP_DETECT_LEAKS=1 to enable it.
set -u
B=${1:?build dir}; KIND=${2:?asan|ubsan}; OCCT=${3:-$HOME/occt-install}
SRC=$(cd "$(dirname "$0")/.." && pwd)
case $KIND in
  asan)  F="-fsanitize=address,undefined" ;;
  ubsan) F="-fsanitize=undefined" ;;
  *) echo "kind must be asan or ubsan"; exit 2 ;;
esac
cmake -S "$SRC" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DOCCT_ROOT="$OCCT" \
  "-DCMAKE_CXX_FLAGS=-Werror $F -fno-sanitize-recover=undefined -fno-omit-frame-pointer" \
  "-DCMAKE_C_FLAGS=$F -fno-sanitize-recover=undefined" "-DCMAKE_EXE_LINKER_FLAGS=$F" > "$B.cfg.log" 2>&1 || { echo "configure failed: $B.cfg.log"; exit 1; }
cmake --build "$B" > "$B.build.log" 2>&1 || { echo "build failed: $B.build.log"; exit 1; }
export ASAN_OPTIONS=detect_leaks=${STL2STP_DETECT_LEAKS:-0}:halt_on_error=1:malloc_context_size=12
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
( while sleep 10; do [ "$(du -sm "$B" | cut -f1)" -gt 2048 ] && { echo "watchdog: $B > 2 GB, stopping"; pkill -f "ctest --test-dir $B"; }; done ) &
WATCH=$!
LOG="$B.ctest.log"
timeout 2400 setarch "$(uname -m)" -R ctest --test-dir "$B" -j4 --timeout 120 --output-on-failure 2>&1 | head -c 20M > "$LOG"
RC=${PIPESTATUS[0]}
kill $WATCH 2>/dev/null
rm -rf "$B/Testing/Temporary"
grep -a "tests passed" "$LOG"
echo "sanitizer reports: asan=$(grep -a -c 'ERROR: AddressSanitizer' "$LOG") ubsan=$(grep -a -c 'runtime error' "$LOG") deadlysignal=$(grep -a -c DEADLYSIGNAL "$LOG")"
exit $RC
