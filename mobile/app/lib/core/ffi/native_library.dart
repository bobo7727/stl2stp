import 'dart:ffi';
import 'dart:io';

import 'package:path/path.dart' as p;

/// Environment variable that overrides where the core library is loaded from.
const coreLibEnv = 'STL2STP_CORE_LIB';

/// Candidate locations of the stl2stp-core shared library for this platform.
///
/// * iOS: the static library is linked into Runner, so symbols live in the process.
/// * macOS: either linked into the app (process) or bundled as a dylib in Frameworks.
/// * Android: `libstl2stp-core.so` packaged in jniLibs.
/// * Linux: bundled next to the executable in `lib/` (see linux/CMakeLists.txt).
/// * Windows: `stl2stp-core.dll` next to the exe.
List<String> coreLibraryCandidates() {
  final env = Platform.environment[coreLibEnv];
  final exeDir = p.dirname(Platform.resolvedExecutable);
  return [
    if (env != null && env.isNotEmpty) env,
    if (Platform.isLinux) ...[p.join(exeDir, 'lib', 'libstl2stp-core.so'), 'libstl2stp-core.so'],
    if (Platform.isAndroid) 'libstl2stp-core.so',
    if (Platform.isMacOS) ...[
      p.join(exeDir, '..', 'Frameworks', 'libstl2stp-core.dylib'),
      'libstl2stp-core.dylib',
    ],
    if (Platform.isWindows) ...[p.join(exeDir, 'stl2stp-core.dll'), 'stl2stp-core.dll'],
  ];
}

/// Opens the core library or throws [ArgumentError] listing what was tried.
DynamicLibrary openCoreLibrary({String? explicitPath}) {
  if (explicitPath != null) return DynamicLibrary.open(explicitPath);
  if (Platform.isIOS || Platform.isMacOS) {
    final proc = DynamicLibrary.process();
    if (proc.providesSymbol('stl2stp_create')) return proc;
  }
  final errors = <String>[];
  for (final c in coreLibraryCandidates()) {
    try {
      final lib = DynamicLibrary.open(c);
      if (lib.providesSymbol('stl2stp_create')) return lib;
      errors.add('$c: no stl2stp_create');
    } catch (e) {
      errors.add('$c: $e');
    }
  }
  throw ArgumentError('stl2stp-core not found:\n${errors.join('\n')}');
}

/// Resolves the path that worked, so worker isolates can open the same library.
/// Returns null when the library lives in the process image (iOS/macOS static link).
String? resolveCoreLibraryPath() {
  if (Platform.isIOS || Platform.isMacOS) {
    if (DynamicLibrary.process().providesSymbol('stl2stp_create')) return null;
  }
  for (final c in coreLibraryCandidates()) {
    try {
      final lib = DynamicLibrary.open(c);
      if (lib.providesSymbol('stl2stp_create')) return c;
    } catch (_) {}
  }
  throw ArgumentError('stl2stp-core not found (tried: ${coreLibraryCandidates().join(', ')})');
}
