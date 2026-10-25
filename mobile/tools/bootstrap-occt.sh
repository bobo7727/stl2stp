#!/usr/bin/env bash
# Build the pinned OCCT dependency locally; no package installation is performed.
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
MOBILE_ROOT=$(cd -- "$SCRIPT_DIR/.." && pwd -P)
WORK_DIR="$MOBILE_ROOT/build/occt"
PREFIX=
JOBS=4
VERSION=8.0.1
URL="https://github.com/Open-Cascade-SAS/OCCT/archive/refs/tags/V$VERSION.tar.gz"
SHA256=6297cc55a1720523a437c54d07dc16b9da8c5f5ab5800da3c6ebded568d53c18

usage() {
    cat <<'EOF'
Usage: bash tools/bootstrap-occt.sh [options]
  --work-dir DIR  Download/source/build workspace (default: mobile/build/occt)
  --prefix DIR    Installation prefix (default: <work-dir>/install)
  --jobs N        Build parallelism (default: 4)
  -h, --help      Show this help

Requires CMake, Ninja, curl, tar, sha256sum, and a C/C++ toolchain. CC/CXX select
compilers; otherwise CMake uses the system defaults. All default output stays
inside mobile/build. Relative option paths use the calling directory.
EOF
}
fail() { printf 'bootstrap-occt: %s\n' "$*" >&2; exit 1; }
value() { [[ $# -ge 2 && -n $2 ]] || fail "$1 requires a value"; }
while [[ $# -gt 0 ]]; do
    case $1 in
        --work-dir) value "$@"; WORK_DIR=$2; shift 2 ;;
        --prefix) value "$@"; PREFIX=$2; shift 2 ;;
        --jobs) value "$@"; JOBS=$2; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) fail "unknown argument: $1 (use --help)" ;;
    esac
done
[[ $JOBS =~ ^[1-9][0-9]*$ ]] || fail '--jobs must be a positive integer'
for tool in cmake ninja curl tar sha256sum; do
    command -v "$tool" >/dev/null || fail "$tool is required; install it before running this script"
done

mkdir -p -- "$WORK_DIR"
WORK_DIR=$(cd -- "$WORK_DIR" && pwd -P)
PREFIX=${PREFIX:-"$WORK_DIR/install"}
mkdir -p -- "$PREFIX" "$WORK_DIR/downloads" "$WORK_DIR/source"
PREFIX=$(cd -- "$PREFIX" && pwd -P)
ARCHIVE="$WORK_DIR/downloads/V$VERSION.tar.gz"
SOURCE_DIR="$WORK_DIR/source/OCCT-$VERSION"
BUILD_DIR="$WORK_DIR/build"

verify_archive() {
    local actual
    actual=$(sha256sum -- "$1")
    actual=${actual%% *}
    [[ $actual == "$SHA256" ]] || fail "SHA256 mismatch for $1; expected $SHA256"
}
if [[ ! -f $ARCHIVE ]]; then
    printf 'Downloading OCCT %s\n' "$VERSION"
    curl --fail --location --retry 3 --output "$ARCHIVE.part" "$URL"
    verify_archive "$ARCHIVE.part"
    mv -- "$ARCHIVE.part" "$ARCHIVE"
else
    verify_archive "$ARCHIVE"
fi

if [[ ! -d $SOURCE_DIR ]]; then
    EXTRACT_DIR=$(mktemp -d "$WORK_DIR/source/.extract.XXXXXXXX")
    tar -xzf "$ARCHIVE" -C "$EXTRACT_DIR"
    [[ -f "$EXTRACT_DIR/OCCT-$VERSION/CMakeLists.txt" ]] || fail "unexpected archive layout in $EXTRACT_DIR"
    printf '%s\n' "$SHA256" > "$EXTRACT_DIR/OCCT-$VERSION/.stl2stp-source-sha256"
    mv -- "$EXTRACT_DIR/OCCT-$VERSION" "$SOURCE_DIR"
    rmdir -- "$EXTRACT_DIR"
else
    [[ -f "$SOURCE_DIR/.stl2stp-source-sha256" ]] || fail "existing source has no bootstrap marker: $SOURCE_DIR; choose a new --work-dir"
    [[ $(cat "$SOURCE_DIR/.stl2stp-source-sha256") == "$SHA256" ]] || fail "source version marker does not match OCCT $VERSION"
fi

printf 'Building OCCT %s\nSource: %s\nBuild: %s\nInstall: %s\n' "$VERSION" "$SOURCE_DIR" "$BUILD_DIR" "$PREFIX"
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DINSTALL_DIR="$PREFIX" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_LIBRARY_TYPE=Shared \
    -DBUILD_MODULE_FoundationClasses=ON -DBUILD_MODULE_ModelingData=ON \
    -DBUILD_MODULE_ModelingAlgorithms=ON -DBUILD_MODULE_DataExchange=ON \
    -DBUILD_MODULE_ApplicationFramework=OFF -DBUILD_MODULE_Visualization=OFF \
    -DBUILD_MODULE_Draw=OFF -DBUILD_MODULE_DETools=OFF \
    -DBUILD_DOC_Overview=OFF -DBUILD_SAMPLES_QT=OFF \
    -DUSE_FREETYPE=OFF -DUSE_TK=OFF -DUSE_TCL=OFF -DUSE_OPENGL=OFF \
    -DUSE_GLES2=OFF -DUSE_XLIB=OFF -DUSE_FREEIMAGE=OFF -DUSE_RAPIDJSON=OFF \
    -DUSE_DRACO=OFF -DUSE_TBB=OFF -DUSE_VTK=OFF
cmake --build "$BUILD_DIR" --parallel "$JOBS"
cmake --install "$BUILD_DIR"
printf '\nOCCT %s installed. Verify the core with:\n  bash %q --occt-root %q\n' \
    "$VERSION" "$SCRIPT_DIR/verify.sh" "$PREFIX"
