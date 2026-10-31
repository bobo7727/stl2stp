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
/// (real stl2stp-core over dart:ffi), [DemoEngine] (debug, profile, and
/// explicit test opt-in), and [UnavailableEngine] (a release build whose
/// native core failed to load or initialize).
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

/// Release builds use this when stl2stp-core cannot be loaded or initialized.
/// It never converts. The UI shows an error instead of [DemoEngine].
class UnavailableEngine implements ConversionEngine {
  UnavailableEngine({this.detail = ''});

  /// Technical reason from the failed load, for the error screen.
  final String detail;

  @override
  String get name => 'stl2stp-core (unavailable)';

  @override
  bool get isReal => false;

  @override
  CancelToken newCancelToken() => SimpleCancelToken();

  StateError get _error => StateError(detail.isEmpty ? 'stl2stp-core is not available' : detail);

  @override
  Future<MeshInfo> inspect(String path) => Future<MeshInfo>.error(_error);

  @override
  Future<ConvertResult> convert(ConvertRequest request,
          {required void Function(ConvertProgress) onProgress, required CancelToken cancel}) =>
      Future<ConvertResult>.error(_error);
}
