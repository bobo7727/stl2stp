#!/usr/bin/env bash
# Generate a fresh export set without depending on an earlier build/ directory.
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
MOBILE_ROOT=$(cd -- "$SCRIPT_DIR/../../.." && pwd -P)
BUILD_DIR="$MOBILE_ROOT/build/core-shared"
OUTPUT_DIR="$MOBILE_ROOT/build/cadcheck"
TIMESTAMP=2026-10-21T02:30:00Z
CHECK=0

usage() {
    cat <<'EOF'
Usage: bash core/tools/cadcheck/run.sh [options]
  --build-dir DIR   Complete OCCT core build (default: mobile/build/core-shared)
  --output-dir DIR  Parent for a fresh run directory (default: mobile/build/cadcheck)
  --timestamp ISO  Fixed export timestamp (default: 2026-10-21T02:30:00Z)
  --check          Run the independent STEP and pcurve checkers; fail on errors
  -h, --help       Show this help

Run tools/verify.sh first to build the CLI and fixture generator. Relative option
paths are resolved from the calling directory. Existing output is kept.
EOF
}
fail() { printf 'cadcheck: %s\n' "$*" >&2; exit 1; }
value() { [[ $# -ge 2 && -n $2 ]] || fail "$1 requires a value"; }
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-dir) value "$@"; BUILD_DIR=$2; shift 2 ;;
        --output-dir) value "$@"; OUTPUT_DIR=$2; shift 2 ;;
        --timestamp) value "$@"; TIMESTAMP=$2; shift 2 ;;
        --check) CHECK=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) fail "unknown argument: $1 (use --help)" ;;
    esac
done

command -v python3 >/dev/null || fail 'python3 is required'
[[ -d $BUILD_DIR ]] || fail "build directory does not exist: $BUILD_DIR"
BUILD_DIR=$(cd -- "$BUILD_DIR" && pwd -P)
CLI="$BUILD_DIR/stl2stp-cli"
GENERATOR="$BUILD_DIR/tests/stl2stp-fixture-generator"
[[ -x $CLI ]] || fail "CLI not found: $CLI; run tools/verify.sh first"
[[ -x $GENERATOR ]] || fail "fixture generator not found: $GENERATOR; build with BUILD_TESTING=ON"

mkdir -p -- "$OUTPUT_DIR"
OUTPUT_DIR=$(cd -- "$OUTPUT_DIR" && pwd -P)
RUN_DIR=$(mktemp -d "$OUTPUT_DIR/run.XXXXXXXX")
FX="$RUN_DIR/fx"
OUT="$RUN_DIR/out"
mkdir -p -- "$FX" "$OUT"
printf 'CAD export run: %s\n' "$RUN_DIR"
export PYTHONDONTWRITEBYTECODE=1
"$GENERATOR" "$FX"
python3 "$SCRIPT_DIR/gen.py" "$FX"

convert() {
    local input=$1 output=$2
    shift 2
    printf '\n== %s\n' "$output"
    "$CLI" convert "$FX/$input" "$OUT/$output" --timestamp "$TIMESTAMP" "$@"
    [[ -s "$OUT/$output" ]] || fail "conversion produced no output: $output"
}
convert cube-binary.stl cube-original.step --mode original
convert cube-binary.stl cube-sewing.step --mode sewing
convert cube-binary.stl cube-original.igs --mode original
convert rot-cube-4.stl rot4-original-merge.step --mode original --merge-coplanar
convert rot-cube-4.stl rot4-sewing-merge.step --mode sewing --merge-coplanar
convert rot-cube-10.stl rot10-original-merge.step --mode original --merge-coplanar
convert rot-cube-10.stl rot10-sewing-merge.step --mode sewing --merge-coplanar
convert rot-cube-10.stl rot10-original-nomerge.step --mode original
convert rot-cube-10.stl rot10-original-merge.igs --mode original --merge-coplanar
convert two-shells.stl two-shells-original.step --mode original
convert two-shells.stl two-shells-sewing.step --mode sewing
convert rot-two-bodies.stl rot-two-bodies-original-merge.step --mode original --merge-coplanar
convert open-cube-4.stl open-cube-original.step --mode original
convert open-cube-4.stl open-cube-sewing-merge.step --mode sewing --merge-coplanar
convert sphere.stl sphere-original.step --mode original
convert sphere.stl sphere-sewing-merge.step --mode sewing --merge-coplanar
convert cube-binary.stl meta-cn.step --mode original --author "致知屋" --org "测试公司（深圳）" --product "零件 A-01" --description "中文描述 °±"
convert cube-binary.stl meta-cn.igs --mode original --author "致知屋" --org "测试公司（深圳）" --product "零件 A-01" --description "中文描述 °±"

steps=("$OUT/"*.step)
for step in "${steps[@]}"; do
    printf '\n## readback %s\n' "${step##*/}"
    "$CLI" readback "$step"
done
if [[ $CHECK -eq 1 ]]; then
    (cd -- "$RUN_DIR" && python3 "$SCRIPT_DIR/step_check.py" "${steps[@]}")
    python3 "$SCRIPT_DIR/pcurve_check.py" "${steps[@]}"
    printf '\nIndependent STEP and pcurve checks passed.\n'
fi
printf 'CAD exports and reports: %s\n' "$RUN_DIR"
