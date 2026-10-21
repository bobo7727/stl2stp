import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:path/path.dart' as p;

import 'engine.dart';
import 'ffi/bindings.dart';
import 'ffi/native_library.dart';
import 'models.dart';
import 'stl_reader.dart' show looksAscii;

/// Cancel flag in native memory so the C cancel callback (running inside the
/// worker isolate, which is blocked in FFI) can read it without messages.
class NativeCancelToken implements CancelToken {
  NativeCancelToken() : _flag = calloc<Int32>();
  final Pointer<Int32> _flag;
  bool _disposed = false;
  int get address => _flag.address;
  @override
  bool get isCancelled => !_disposed && _flag.value != 0;
  @override
  void cancel() {
    if (!_disposed) _flag.value = 1;
  }

  @override
  void dispose() {
    if (_disposed) return;
    _disposed = true;
    calloc.free(_flag);
  }
}

/// Real engine: calls stl2stp-core through dart:ffi on a background isolate.
class FfiEngine implements ConversionEngine {
  FfiEngine._(this.libraryPath);

  /// Path of the opened library, or null when linked into the process (iOS).
  final String? libraryPath;

  /// Returns null if the core library cannot be found/opened on this platform.
  static FfiEngine? tryCreate() {
    try {
      final path = resolveCoreLibraryPath();
      // Validate all symbols once on the main isolate.
      Stl2StpBindings(path == null ? DynamicLibrary.process() : DynamicLibrary.open(path));
      return FfiEngine._(path);
    } catch (_) {
      return null;
    }
  }

  @override
  String get name => 'stl2stp-core (FFI${libraryPath == null ? '' : ': ${p.basename(libraryPath!)}'})';

  @override
  bool get isReal => true;

  @override
  CancelToken newCancelToken() => NativeCancelToken();

  @override
  Future<MeshInfo> inspect(String path) async {
    final lib = libraryPath;
    final r = await Isolate.run(() => _inspectWorker(lib, path));
    if (r['status'] as int < 0) throw LoadException(r['status'] as int, r['error'] as String);
    final file = File(path);
    final size = file.lengthSync();
    final raf = file.openSync();
    final head = raf.readSync(size < 84 ? size : 84);
    raf.closeSync();
    // Binary check needs the size: replicate looksAscii with a size-aware header.
    final isAscii = _isAsciiHeader(head, size);
    return MeshInfo(
      path: path,
      fileName: p.basename(path),
      fileSize: size,
      faces: r['faces'] as int,
      vertices: r['vertices'] as int,
      edges: r['edges'] as int,
      min: (r['min'] as List).cast<double>(),
      max: (r['max'] as List).cast<double>(),
      isAscii: isAscii,
    );
  }

  static bool _isAsciiHeader(List<int> head, int size) {
    if (head.length >= 84) {
      final n = head[80] | head[81] << 8 | head[82] << 16 | head[83] << 24;
      if (84 + 50 * n == size) return false;
    }
    return looksAscii(Uint8List.fromList(head));
  }

  @override
  Future<ConvertResult> convert(ConvertRequest request,
      {required void Function(ConvertProgress) onProgress, required CancelToken cancel}) async {
    final token = cancel as NativeCancelToken;
    final port = ReceivePort();
    final errPort = ReceivePort();
    final exitPort = ReceivePort();
    final stages = request.stages;
    final sw = Stopwatch()..start();
    final done = Completer<ConvertResult>();
    final s = request.settings;
    final iges = s.format == ExportFormat.iges;
    String meta(String v) => iges ? stripNonAscii(v) : v.trim();
    final args = <String, Object?>{
      'port': port.sendPort,
      'lib': libraryPath,
      'cancel': token.address,
      'in': request.inputPath,
      'out': request.outputPath,
      'format': s.format.name,
      'simplify': s.simplify,
      'target': s.targetFaces(request.faces),
      'mode': s.mode.value,
      'merge': s.mergeCoplanar,
      'verify': s.verifyEffective,
      'weld': s.weld,
      'author': meta(s.author),
      'org': meta(s.organization),
      'product': meta(s.product),
      'description': meta(s.description),
    };
    port.listen((msg) {
      final m = msg as Map;
      if (m['t'] == 'p') {
        final st = Stage.values[m['s'] as int];
        final f = (m['f'] as double).clamp(0.0, 1.0);
        onProgress(ConvertProgress(st, f, overallProgress(stages, st, f)));
      } else if (m['t'] == 'done' && !done.isCompleted) {
        sw.stop();
        final topo = m['topo'] as List;
        done.complete(ConvertResult(
          status: m['status'] as int,
          outputPath: request.outputPath,
          message: m['error'] as String,
          topology: Topology(
              solids: topo[0] as int, shells: topo[1] as int, faces: topo[2] as int, edges: topo[3] as int,
              vertices: topo[4] as int),
          verified: m['verified'] as bool?,
          facesAfterSimplify: m['facesAfter'] as int?,
          warnings: (m['warnings'] as List).cast<int>(),
          elapsed: sw.elapsed,
          failedStage: m['failed'] == null ? null : Stage.values[m['failed'] as int],
        ));
      }
    });
    errPort.listen((e) {
      if (!done.isCompleted) {
        done.complete(ConvertResult(
            status: CoreStatus.errorInternal, outputPath: request.outputPath, message: 'worker error: $e'));
      }
    });
    exitPort.listen((_) {
      if (!done.isCompleted) {
        done.complete(ConvertResult(
            status: CoreStatus.errorInternal, outputPath: request.outputPath, message: 'worker exited'));
      }
    });
    await Isolate.spawn(_convertWorker, args, onError: errPort.sendPort, onExit: exitPort.sendPort);
    try {
      final r = await done.future;
      if (!r.ok) {
        try {
          final f = File(request.outputPath);
          if (f.existsSync()) f.deleteSync();
        } catch (_) {}
      }
      return r;
    } finally {
      port.close();
      errPort.close();
      exitPort.close();
    }
  }
}

DynamicLibrary _open(String? path) => path == null ? DynamicLibrary.process() : DynamicLibrary.open(path);

Map<String, Object?> _inspectWorker(String? lib, String path) {
  final b = Stl2StpBindings(_open(lib));
  final h = b.create();
  if (h == nullptr) return {'status': CoreStatus.errorMemory, 'error': 'create failed'};
  final cPath = path.toNativeUtf8();
  final stats = calloc<Stl2StpMeshStats>();
  try {
    var st = b.loadStl(h, cPath, nullptr);
    if (st < 0) return {'status': st, 'error': b.errorOf(h)};
    st = b.getMeshStats(h, stats);
    if (st < 0) return {'status': st, 'error': b.errorOf(h)};
    final s = stats.ref;
    return {
      'status': st,
      'error': '',
      'faces': s.faces,
      'vertices': s.vertices,
      'edges': s.edges,
      'min': [s.minimum[0], s.minimum[1], s.minimum[2]],
      'max': [s.maximum[0], s.maximum[1], s.maximum[2]],
    };
  } finally {
    calloc.free(stats);
    calloc.free(cPath);
    b.destroy(h);
  }
}

// ---- worker isolate state (isolate-local globals used by C callbacks) ----
SendPort? _wPort;
int _wStage = 0;
double _wLast = -1;
int _wLastMs = 0;
final Stopwatch _wClock = Stopwatch();
Pointer<Int32> _wCancel = nullptr;

/// Export callback covers both "build faces" and "write file"; split at 70 %.
bool _wExportSplit = false;

void _post(int stage, double f, {bool force = false}) {
  final now = _wClock.elapsedMilliseconds;
  if (!force && stage == _wStage && (f - _wLast).abs() < 0.01 && now - _wLastMs < 50) return;
  _wStage = stage;
  _wLast = f;
  _wLastMs = now;
  _wPort?.send({'t': 'p', 's': stage, 'f': f});
}

void _onProgress(double fraction, Pointer<Void> _) {
  if (_wExportSplit) {
    if (fraction < 0.7) {
      _post(Stage.build.index, fraction / 0.7);
    } else {
      _post(Stage.write.index, (fraction - 0.7) / 0.3);
    }
  } else {
    _post(_wStage, fraction);
  }
}

int _onCancel(Pointer<Void> _) => _wCancel == nullptr ? 0 : (_wCancel.value != 0 ? 1 : 0);

void _convertWorker(Map<String, Object?> a) {
  _wPort = a['port'] as SendPort;
  _wCancel = Pointer<Int32>.fromAddress(a['cancel'] as int);
  _wClock.start();
  final b = Stl2StpBindings(_open(a['lib'] as String?));
  final warnings = <int>[];
  final topo = calloc<Stl2StpTopology>();
  final topo2 = calloc<Stl2StpTopology>();
  final stats = calloc<Stl2StpMeshStats>();
  final cb = calloc<Stl2StpCallbacks>();
  final opt = calloc<Stl2StpExportOptions>();
  final strings = <Pointer<Utf8>>[];
  Pointer<Utf8> str(String s) {
    final ptr = s.toNativeUtf8();
    strings.add(ptr);
    return ptr;
  }

  cb.ref
    ..progress = Pointer.fromFunction<ProgressNative>(_onProgress)
    ..cancelled = Pointer.fromFunction<CancelNative>(_onCancel, 0)
    ..userData = nullptr;
  final h = b.create();
  int? facesAfter;
  bool? verified;
  void finish(int status, {int? failed, String error = ''}) {
    _wPort!.send({
      't': 'done',
      'status': status,
      'error': error,
      'failed': failed,
      'warnings': warnings,
      'facesAfter': facesAfter,
      'verified': verified,
      'topo': [topo.ref.solids, topo.ref.shells, topo.ref.faces, topo.ref.edges, topo.ref.vertices],
    });
  }

  int note(int st) {
    if (st > 0 && !warnings.contains(st)) warnings.add(st);
    return st;
  }

  try {
    if (h == nullptr) return finish(CoreStatus.errorMemory, failed: Stage.read.index, error: 'create failed');
    // 1. read
    _post(Stage.read.index, 0, force: true);
    var st = note(b.loadStl(h, str(a['in'] as String), cb));
    if (st < 0) return finish(st, failed: Stage.read.index, error: b.errorOf(h));
    _post(Stage.read.index, 1, force: true);
    // 2. simplify (weld first so QEM sees closed seams; core/weld)
    if (a['simplify'] == true) {
      _post(Stage.simplify.index, 0, force: true);
      if (a['weld'] == true) {
        final merged = calloc<Uint64>();
        try {
          st = b.weldVertices(h, 0, merged);
          if (st < 0) return finish(st, failed: Stage.simplify.index, error: b.errorOf(h));
          if (merged.value > 0) note(CoreStatus.warningRepaired);
        } finally {
          calloc.free(merged);
        }
      }
      st = note(b.simplify(h, a['target'] as int, cb));
      if (st < 0) return finish(st, failed: Stage.simplify.index, error: b.errorOf(h));
      if (b.getMeshStats(h, stats) >= 0) facesAfter = stats.ref.faces;
      _post(Stage.simplify.index, 1, force: true);
    }
    // 3+4. build + write
    final d = b.defaultExportOptions();
    opt.ref
      ..sewingTolerance = d.sewingTolerance
      ..mergeCoplanar = a['merge'] == true ? 1 : 0
      ..mode = a['mode'] as int
      ..author = str(a['author'] as String)
      ..organization = str(a['org'] as String)
      ..product = str(a['product'] as String)
      ..description = str(a['description'] as String)
      ..timestamp = nullptr
      ..mergeAngularTolerance = d.mergeAngularTolerance
      ..strictOriginal = d.strictOriginal
      ..weldVertices = a['weld'] == true ? 1 : 0
      ..weldTolerance = d.weldTolerance;
    _post(Stage.build.index, 0, force: true);
    _wExportSplit = true;
    final out = str(a['out'] as String);
    st = note(a['format'] == 'iges' ? b.exportIges(h, out, opt, cb, topo) : b.exportStep(h, out, opt, cb, topo));
    _wExportSplit = false;
    if (st < 0) {
      final failed = _wStage == Stage.write.index ? Stage.write.index : Stage.build.index;
      return finish(st, failed: failed, error: b.errorOf(h));
    }
    final exportMsg = st > 0 ? b.errorOf(h) : '';
    _post(Stage.write.index, 1, force: true);
    // 5. verify (STEP only)
    if (a['verify'] == true) {
      _post(Stage.verify.index, 0, force: true);
      final v = b.readStep(h, out, cb, topo2);
      if (v == CoreStatus.errorCancelled) return finish(v, failed: Stage.verify.index, error: b.errorOf(h));
      final notSolid = warnings.contains(CoreStatus.warningNotSolid);
      verified = v >= 0 && topo2.ref.faces > 0 && (topo2.ref.solids >= 1 || notSolid);
      _post(Stage.verify.index, 1, force: true);
    }
    final finalStatus = warnings.isEmpty ? CoreStatus.ok : warnings.first;
    finish(finalStatus, error: exportMsg);
  } catch (e) {
    finish(CoreStatus.errorInternal, error: '$e');
  } finally {
    if (h != nullptr) b.destroy(h);
    for (final s in strings) {
      calloc.free(s);
    }
    calloc.free(topo);
    calloc.free(topo2);
    calloc.free(stats);
    calloc.free(cb);
    calloc.free(opt);
  }
}
