#!/usr/bin/env bash
# Build OCCT 8.0.1 (static) + stl2stp-core for iOS and wire them into the Flutter
# Runner without editing the Xcode project:
#
#   app/ios/Stl2StpCore/iphoneos/libStl2StpCore.a         (device arm64)
#   app/ios/Stl2StpCore/iphonesimulator/libStl2StpCore.a  (simulator arm64 [+ x86_64])
#   app/ios/Stl2StpCore/Stl2StpCore.xcframework           (same, for SwiftUI/other uses)
#   app/ios/Flutter/Stl2StpCore.xcconfig                  (picked up by Debug/Release.xcconfig)
#
# Run on a Mac with Xcode, CMake, Ninja and Python 3: bash app/tool/ios/build_ios_core.sh
# Env overrides: WORK (default ~/stl2stp-ios-deps), MIN_IOS (15.0), JOBS,
#                SIM_ARCHS ("arm64;x86_64"; use "arm64" on Apple silicon only),
#                OCCT_TGZ (path to V8.0.1.tar.gz if already downloaded).
# NOTE: written on Windows/WSL without a Mac; not executed yet. See docs/mac-setup-guide.md.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../../.." && pwd)"
APP="$REPO/app"
WORK="${WORK:-$HOME/stl2stp-ios-deps}"
MIN_IOS="${MIN_IOS:-15.0}"
# Derive the full exported C API and check Dart lookups before starting a build.
u_flags="$(python3 "$APP/tool/ios/core_symbols.py" --linker-flags)"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
SIM_ARCHS="${SIM_ARCHS:-arm64;x86_64}"
OCCT_TGZ="${OCCT_TGZ:-$WORK/V8.0.1.tar.gz}"
OCCT_SHA256=6297cc55a1720523a437c54d07dc16b9da8c5f5ab5800da3c6ebded568d53c18
OCCT_URL=https://github.com/Open-Cascade-SAS/OCCT/archive/refs/tags/V8.0.1.tar.gz

mkdir -p "$WORK"
cd "$WORK"
[ -f "$OCCT_TGZ" ] || curl -fL -o "$OCCT_TGZ" "$OCCT_URL"
echo "$OCCT_SHA256  $OCCT_TGZ" | shasum -a 256 -c -
[ -d "$WORK/OCCT-8.0.1" ] || tar xzf "$OCCT_TGZ" -C "$WORK"

ios_common=(
  -G Ninja
  -DCMAKE_SYSTEM_NAME=iOS
  # CMake maps this to -mios-version-min for both the OCCT and stl2stp-core builds.
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_IOS"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
  -DCMAKE_C_FLAGS="-Os"
  -DCMAKE_CXX_FLAGS="-Os"
)

build_occt() { # name sysroot archs
  local name=$1 sysroot=$2 archs=$3
  local B="$WORK/build-occt-$name" P="$WORK/occt-$name"
  # Always reconfigure so changed architectures/deployment targets invalidate
  # the affected outputs. CMake/Ninja still reuse work for unchanged inputs.
  cmake "${ios_common[@]}" -S "$WORK/OCCT-8.0.1" -B "$B" \
    -DCMAKE_OSX_SYSROOT="$sysroot" -DCMAKE_OSX_ARCHITECTURES="$archs" \
    -DINSTALL_DIR="$P" -DCMAKE_INSTALL_PREFIX="$P" \
    -DBUILD_LIBRARY_TYPE=Static \
    -DBUILD_MODULE_FoundationClasses=ON -DBUILD_MODULE_ModelingData=ON \
    -DBUILD_MODULE_ModelingAlgorithms=ON -DBUILD_MODULE_DataExchange=ON \
    -DBUILD_MODULE_ApplicationFramework=OFF -DBUILD_MODULE_Visualization=OFF \
    -DBUILD_MODULE_Draw=OFF -DBUILD_MODULE_DETools=OFF \
    -DBUILD_DOC_Overview=OFF -DBUILD_SAMPLES_QT=OFF -DBUILD_USE_PCH=OFF \
    -DUSE_FREETYPE=OFF -DUSE_TK=OFF -DUSE_TCL=OFF -DUSE_OPENGL=OFF -DUSE_GLES2=OFF -DUSE_XLIB=OFF \
    -DUSE_FREEIMAGE=OFF -DUSE_RAPIDJSON=OFF -DUSE_DRACO=OFF -DUSE_TBB=OFF -DUSE_VTK=OFF
  cmake --build "$B" --parallel "$JOBS"
  cmake --install "$B"
}

build_core() { # name sysroot archs
  local name=$1 sysroot=$2 archs=$3
  local B="$WORK/build-core-$name" P="$WORK/occt-$name" OUT="$APP/ios/Stl2StpCore/$name"
  # STL2STP_WITH_OCCT defaults to ON, but an old cache can keep OFF and
  # then compile exchange_disabled.cpp. The C API symbol check cannot tell
  # that stub from the real exchange, so the iOS build forces ON.
  cmake "${ios_common[@]}" -S "$REPO/core" -B "$B" \
    -DCMAKE_OSX_SYSROOT="$sysroot" -DCMAKE_OSX_ARCHITECTURES="$archs" \
    -DOCCT_ROOT="$P" -DBUILD_SHARED_LIBS=OFF -DSTL2STP_BUILD_CLI=OFF -DBUILD_TESTING=OFF \
    -DSTL2STP_WITH_OCCT=ON
  cmake --build "$B" --parallel "$JOBS"
  mkdir -p "$OUT"
  # One archive = core + every OCCT toolkit that was built (only what the
  # selected modules need). Unused code is dropped by the app linker.
  # Bash 3.2 (the macOS default) supports read -d and indexed arrays. Preserve
  # each complete path even when WORK or an archive name contains spaces.
  local archive
  local -a occt_libs=()
  while IFS= read -r -d '' archive; do
    occt_libs+=("$archive")
  done < <(find "$P" -name 'libTK*.a' -print0)
  if [ "${#occt_libs[@]}" -eq 0 ]; then
    echo "No OCCT static libraries found in $P" >&2
    return 1
  fi
  libtool -static -o "$OUT/libStl2StpCore.a" "$B/libstl2stp-core.a" "${occt_libs[@]}"
  ls -lh "$OUT/libStl2StpCore.a"
  lipo -info "$OUT/libStl2StpCore.a"
  # Validate every requested slice; archive creation alone does not prove that
  # the public C API is present for the architecture Runner will link.
  local -a check_archs=()
  local arch
  local IFS=';'
  for arch in $archs; do check_archs+=(--arch "$arch"); done
  python3 "$APP/tool/ios/check_ios_binary.py" "$OUT/libStl2StpCore.a" "${check_archs[@]}"
}

build_occt iphoneos iphoneos arm64
build_occt iphonesimulator iphonesimulator "$SIM_ARCHS"
build_core iphoneos iphoneos arm64
build_core iphonesimulator iphonesimulator "$SIM_ARCHS"

rm -rf "$APP/ios/Stl2StpCore/Stl2StpCore.xcframework"
xcodebuild -create-xcframework \
  -library "$APP/ios/Stl2StpCore/iphoneos/libStl2StpCore.a" -headers "$REPO/core/include" \
  -library "$APP/ios/Stl2StpCore/iphonesimulator/libStl2StpCore.a" -headers "$REPO/core/include" \
  -output "$APP/ios/Stl2StpCore/Stl2StpCore.xcframework"

# Force the C API symbols into Runner (nothing in Swift references them; Dart
# looks them up at runtime with DynamicLibrary.process()).
# core_symbols.py includes every public C function, including stl2stp_weld_vertices.
cat > "$APP/ios/Flutter/Stl2StpCore.xcconfig" <<XC
// Generated by app/tool/ios/build_ios_core.sh — do not commit.
// Generic simulator destinations ignore ONLY_ACTIVE_ARCH. Match Runner to
// the slices we actually built, rather than Xcode's default arm64 + x86_64.
ARCHS[sdk=iphonesimulator*] = ${SIM_ARCHS//;/ }
STL2STP_CORE_LIBRARY[sdk=iphoneos*] = \$(PROJECT_DIR)/Stl2StpCore/iphoneos/libStl2StpCore.a
STL2STP_CORE_LIBRARY[sdk=iphonesimulator*] = \$(PROJECT_DIR)/Stl2StpCore/iphonesimulator/libStl2StpCore.a
OTHER_LDFLAGS = \$(inherited) $u_flags "\$(STL2STP_CORE_LIBRARY)" -lc++
// Keep the FFI exports in Runner itself, including Debug on Xcode 16+.
// Otherwise Xcode splits code into Runner.debug.dylib and a launcher stub.
ENABLE_DEBUG_DYLIB = NO
// Keep global symbols so DynamicLibrary.process().lookup() can find them.
STRIP_STYLE = non-global
XC
echo "Done. Next: cd app && flutter run -d <simulator or device>"
