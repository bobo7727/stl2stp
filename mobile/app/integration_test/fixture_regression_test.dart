// Loads a few committed STL fixtures through the real FfiEngine (never
// DemoEngine), writes STEP and IGES, and re-reads STEP with stl2stp_read_step.
// IGES has no public read API; the file is checked for an IGES millimetre unit.
// Full OCCT re-reads of both formats live in core/tests/test_regression.cpp.
//
// Codemagic workflow ios-fixture-regression runs this on an iOS simulator.
// It is manual on purpose: building the simulator core spends macOS minutes.
import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/core/engine.dart';
import 'package:stl2stp_app/core/ffi_engine.dart';
import 'package:stl2stp_app/core/models.dart';

Future<File> materialize(Directory dir, String asset) async {
  final data = await rootBundle.load(asset);
  final file = File('${dir.path}/${asset.split('/').last}');
  await file.writeAsBytes(data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes), flush: true);
  return file;
}

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('FfiEngine converts committed fixtures and re-reads STEP', (tester) async {
    final engine = FfiEngine.tryCreate();
    expect(engine, isNotNull, reason: 'stl2stp-core did not load; refusing to fall back to DemoEngine');
    final ConversionEngine real = engine!;
    expect(real, isA<FfiEngine>());
    expect(real, isNot(isA<DemoEngine>()));
    expect(real.isReal, isTrue);
    expect(real.name.toLowerCase(), isNot(contains('demo')));

    final dir = await Directory.systemTemp.createTemp('stl2stp_fixtures');
    final cube = await materialize(dir, 'assets/regression/01_cube_20mm_binary.stl');
    final ascii = await materialize(dir, 'assets/regression/02_cube_20mm_ascii.stl');
    final open = await materialize(dir, 'assets/regression/07_open_mesh_cube_missing_top.stl');
    final invalid = await materialize(dir, 'assets/regression/10_invalid_not_stl.stl');

    Future<ConvertResult> convert(File input, MeshInfo info, ConvertSettings settings) {
      final out = File('${dir.path}/${input.uri.pathSegments.last}.${settings.format.extension}');
      final token = real.newCancelToken();
      return real
          .convert(
            ConvertRequest(inputPath: input.path, outputPath: out.path, settings: settings, faces: info.faces),
            onProgress: (_) {},
            cancel: token,
          )
          .whenComplete(token.dispose);
    }

    final cubeInfo = await real.inspect(cube.path);
    expect(cubeInfo.faces, 12);
    expect(cubeInfo.vertices, 8);
    expect(cubeInfo.isAscii, isFalse);
    final step = await convert(cube, cubeInfo, const ConvertSettings(verify: true));
    expect(step.ok, isTrue, reason: step.message);
    expect(step.status, CoreStatus.ok);
    expect(step.verified, isTrue, reason: 'stl2stp_read_step did not accept the STEP file');
    expect(step.topology.solids, 1);
    expect(step.topology.faces, 12);
    final stepText = await File(step.outputPath).readAsString();
    expect(stepText.startsWith('ISO-10303-21;'), isTrue);
    expect(stepText.contains('DEMO'), isFalse);
    expect(stepText.contains('MANIFOLD_SOLID_BREP'), isTrue);
    expect(stepText.contains('.MILLI.') || stepText.contains('MILLIMETRE'), isTrue, reason: 'STEP length unit is not mm');
    final stepBytes = await File(step.outputPath).length();
    // ignore: avoid_print
    print('FIXTURE_OK cube STEP ${step.elapsed.inMilliseconds} ms '
        'solids=${step.topology.solids} faces=${step.topology.faces} bytes=$stepBytes');

    final asciiInfo = await real.inspect(ascii.path);
    expect(asciiInfo.faces, 12);
    expect(asciiInfo.isAscii, isTrue);
    final iges = await convert(ascii, asciiInfo, const ConvertSettings(format: ExportFormat.iges, verify: true));
    expect(iges.ok, isTrue, reason: iges.message);
    expect(iges.status, CoreStatus.ok);
    expect(iges.topology.solids, 1);
    final igesBytes = await File(iges.outputPath).readAsBytes();
    expect(igesBytes.length, greaterThan(1000));
    final igesText = String.fromCharCodes(igesBytes);
    expect(igesText.contains('2HMM'), isTrue, reason: 'IGES unit record is not millimetres');
    expect(igesText.contains('DEMO'), isFalse);
    // ignore: avoid_print
    print('FIXTURE_OK cube IGES ${iges.elapsed.inMilliseconds} ms bytes=${igesBytes.length}');

    final openInfo = await real.inspect(open.path);
    expect(openInfo.faces, 10);
    final openStep = await convert(open, openInfo, const ConvertSettings(verify: true));
    expect(openStep.ok, isTrue, reason: openStep.message);
    expect(openStep.status, CoreStatus.warningNotSolid);
    expect(openStep.verified, isTrue);
    expect(openStep.topology.solids, 0);
    final openText = await File(openStep.outputPath).readAsString();
    expect(openText.contains('MANIFOLD_SOLID_BREP'), isFalse);
    // ignore: avoid_print
    print('FIXTURE_OK open mesh STEP status=${openStep.status} solids=${openStep.topology.solids}');

    try {
      await real.inspect(invalid.path);
      fail('invalid STL must fail in FfiEngine');
    } on LoadException catch (error) {
      expect(error.status, CoreStatus.errorFormat);
      expect(error.message, isNotEmpty);
      // ignore: avoid_print
      print('FIXTURE_OK invalid STL status=${error.status}');
    }
  });
}
