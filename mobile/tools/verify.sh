#!/usr/bin/env bash
# Build and test the core from any current working directory.
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
MOBILE_ROOT=$(cd -- "$SCRIPT_DIR/.." && pwd -P)
BUILD_DIR=
OCCT_ROOT="$MOBILE_ROOT/build/occt/install"
JOBS=4
MESH_ONLY=0
CAD=0

usage() {
    cat <<'EOF'
Usage: bash tools/verify.sh [options]
  --mesh-only      Build/test mesh operations without OCCT (no STEP/IGES checks)
  --occt-root DIR  OCCT installation (default: mobile/build/occt/install)
  --build-dir DIR  Build output (default: mobile/build/core-shared or build/mesh)
  --jobs N         Build/test parallelism (default: 4)
  --cad            Also generate exports and run independent STEP/pcurve checks
  -h, --help       Show this help

The default is a full OCCT build with a shared core library, CLI, and tests.
Run tools/bootstrap-occt.sh first if OCCT is not installed. CC/CXX select the
toolchain. Relative option paths are resolved from the calling directory.
EOF
}
fail() { printf 'verify: %s\n' "$*" >&2; exit 1; }
value() { [[ $# -ge 2 && -n $2 ]] || fail "$1 requires a value"; }
while [[ $# -gt 0 ]]; do
    case $1 in
        --mesh-only) MESH_ONLY=1; shift ;;
        --occt-root) value "$@"; OCCT_ROOT=$2; shift 2 ;;
        --build-dir) value "$@"; BUILD_DIR=$2; shift 2 ;;
        --jobs) value "$@"; JOBS=$2; shift 2 ;;
        --cad) CAD=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) fail "unknown argument: $1 (use --help)" ;;
    esac
done
[[ $JOBS =~ ^[1-9][0-9]*$ ]] || fail '--jobs must be a positive integer'
[[ $MESH_ONLY -eq 0 || $CAD -eq 0 ]] || fail '--cad requires a full OCCT build; remove --mesh-only'
for tool in cmake ninja ctest; do
    command -v "$tool" >/dev/null || fail "$tool is required"
done
if [[ $CAD -eq 1 ]]; then
    command -v python3 >/dev/null || fail '--cad requires python3'
fi

if [[ $MESH_ONLY -eq 1 ]]; then
    BUILD_DIR=${BUILD_DIR:-"$MOBILE_ROOT/build/mesh"}
    WITH_OCCT=OFF
    printf 'Scope: mesh-only. STEP/IGES and OCCT regression tests are not run.\n'
else
    BUILD_DIR=${BUILD_DIR:-"$MOBILE_ROOT/build/core-shared"}
    WITH_OCCT=ON
    [[ -f "$OCCT_ROOT/include/opencascade/Standard_Version.hxx" || -f "$OCCT_ROOT/inc/Standard_Version.hxx" ]] || \
        fail "OCCT headers not found under $OCCT_ROOT; run tools/bootstrap-occt.sh or pass --occt-root"
    OCCT_ROOT=$(cd -- "$OCCT_ROOT" && pwd -P)
    printf 'Scope: full core, including STEP/IGES and OCCT regression tests.\nOCCT: %s\n' "$OCCT_ROOT"
fi
mkdir -p -- "$BUILD_DIR"
BUILD_DIR=$(cd -- "$BUILD_DIR" && pwd -P)

# Discard prior OCCT discovery paths if a caller reuses a build with a new prefix.
cmake -S "$MOBILE_ROOT/core" -B "$BUILD_DIR" -G Ninja \
    -U '_occt_*' -U OCCT_INCLUDE_DIR -U OCCT_LIBRARY_DIR -U OpenCASCADE_DIR \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTING=ON -DSTL2STP_BUILD_CLI=ON \
    -DSTL2STP_WITH_OCCT="$WITH_OCCT" -DOCCT_ROOT="$OCCT_ROOT"
cmake --build "$BUILD_DIR" --parallel "$JOBS"
ctest --test-dir "$BUILD_DIR" --output-on-failure --parallel "$JOBS"
if [[ $CAD -eq 1 ]]; then
    bash "$MOBILE_ROOT/core/tools/cadcheck/run.sh" --build-dir "$BUILD_DIR" --check
fi
printf '\nRequested verification passed. Build directory: %s\n' "$BUILD_DIR"
