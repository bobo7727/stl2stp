import 'dart:async';
import 'dart:io';
import 'dart:isolate';

import 'package:path/path.dart' as p;

import 'engine.dart';
import 'models.dart';
import 'stl_reader.dart';

/// Engine without native code: stats come from the Dart STL reader and the
/// conversion is simulated (writes a clearly marked placeholder file).
/// Used by widget tests, debug/profile when stl2stp-core is not built yet,
/// and release builds only when `--dart-define=STL2STP_DEMO=true` (or `=1`)
/// is set. A release build never selects this automatically.
class DemoEngine implements ConversionEngine {
  DemoEngine({this.stepDelay = const Duration(milliseconds: 40), this.failWith});

  final Duration stepDelay;

  /// If set, the simulated conversion ends with this status.
  final int? failWith;

  @override
  String get name => 'Demo (no stl2stp-core)';

  @override
  bool get isReal => false;

  @override
  CancelToken newCancelToken() => SimpleCancelToken();

  @override
  Future<MeshInfo> inspect(String path) async {
    final file = File(path);
    if (!file.existsSync()) throw LoadException(CoreStatus.errorIo, 'not found: $path');
    final size = file.lengthSync();
    StlMesh mesh;
    try {
      mesh = size > 4 * 1024 * 1024 ? await Isolate.run(() => readStlFile(path)) : readStlFile(path);
    } on StlFormatException catch (e) {
      throw LoadException(
          e.message.contains('no ') ? CoreStatus.errorEmpty : CoreStatus.errorFormat, e.message);
    }
    final b = mesh.bounds();
    final v = mesh.uniqueVertexCount();
    return MeshInfo(
      path: path,
      fileName: p.basename(path),
      fileSize: size,
      faces: mesh.totalTriangles,
      vertices: v,
      edges: (mesh.totalTriangles * 3) ~/ 2,
      min: b.min,
      max: b.max,
      isAscii: mesh.isAscii,
    );
  }

  @override
  Future<ConvertResult> convert(ConvertRequest request,
      {required void Function(ConvertProgress) onProgress, required CancelToken cancel}) async {
    final sw = Stopwatch()..start();
    final stages = request.stages;
    for (final st in stages) {
      for (var i = 0; i <= 10; i++) {
        if (cancel.isCancelled) {
          return ConvertResult(
              status: CoreStatus.errorCancelled, outputPath: request.outputPath, failedStage: st, elapsed: sw.elapsed);
        }
        onProgress(ConvertProgress(st, i / 10, overallProgress(stages, st, i / 10)));
        await Future<void>.delayed(stepDelay);
      }
      if (failWith != null && st == Stage.build) {
        return ConvertResult(status: failWith!, outputPath: request.outputPath, failedStage: st, message: 'demo failure');
      }
    }
    final s = request.settings;
    final faces = s.simplify ? s.targetFaces(request.faces) : request.faces;
    File(request.outputPath).writeAsStringSync(s.format == ExportFormat.step
        ? 'ISO-10303-21;\n/* DEMO PLACEHOLDER - not a real STEP file (stl2stp-core not linked) */\nEND-ISO-10303-21;\n'
        : 'DEMO PLACEHOLDER - not a real IGES file (stl2stp-core not linked)\n');
    final solid = s.format == ExportFormat.step;
    return ConvertResult(
      status: CoreStatus.ok,
      outputPath: request.outputPath,
      topology: Topology(solids: solid ? 1 : 0, shells: 1, faces: s.mergeCoplanar ? (faces / 4).ceil() : faces),
      verified: s.verifyEffective ? true : null,
      facesAfterSimplify: s.simplify ? faces : null,
      elapsed: sw.elapsed,
    );
  }
}
