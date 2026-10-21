import 'models.dart';

/// Cancellation flag shared between the UI and a running conversion.
abstract class CancelToken {
  bool get isCancelled;
  void cancel();
  void dispose();
}

class SimpleCancelToken implements CancelToken {
  bool _c = false;
  @override
  bool get isCancelled => _c;
  @override
  void cancel() => _c = true;
  @override
  void dispose() {}
}

/// What the UI needs from the conversion core. Implemented by [FfiEngine]
/// (real stl2stp-core over dart:ffi) and [DemoEngine] (no native code; used
/// by widget tests and on platforms where the core is not built yet).
abstract class ConversionEngine {
  /// Human-readable name shown in Settings › About.
  String get name;

  /// True when conversions produce real CAD files.
  bool get isReal;

  /// Loads an STL with the core and returns its stats. Throws [LoadException].
  Future<MeshInfo> inspect(String path);

  CancelToken newCancelToken();

  /// Runs read → (simplify) → build → write → (verify) and reports progress.
  Future<ConvertResult> convert(ConvertRequest request,
      {required void Function(ConvertProgress) onProgress, required CancelToken cancel});
}
