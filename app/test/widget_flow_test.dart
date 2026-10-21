import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/core/engine.dart';
import 'package:stl2stp_app/core/models.dart';
import 'package:stl2stp_app/core/sample_mesh.dart';
import 'package:stl2stp_app/core/stl_reader.dart';
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/screens/flow.dart';
import 'package:stl2stp_app/screens/preview_screen.dart';
import 'package:stl2stp_app/state/app_state.dart';

Future<AppState> makeState({ConversionEngine? engine, Map<String, Object> prefs = const {}}) async {
  SharedPreferences.setMockInitialValues({'onboardingSeen': true, ...prefs});
  final p = await SharedPreferences.getInstance();
  final dir = Directory.systemTemp.createTempSync('stl2stp_test');
  addTearDown(() => dir.deleteSync(recursive: true));
  return AppState(engine: engine ?? DemoEngine(stepDelay: const Duration(milliseconds: 5)), prefs: p, outputDir: dir);
}

LoadedModel sampleModel(String path) {
  final mesh = parseStlBytes(buildBracketStl());
  final b = mesh.bounds();
  return LoadedModel(
    MeshInfo(
        path: path,
        fileName: 'bracket.stl',
        fileSize: 1484,
        faces: mesh.totalTriangles,
        vertices: 16,
        edges: 42,
        min: b.min,
        max: b.max,
        isAscii: false),
    mesh,
  );
}

void main() {
  setUpAll(() async {
    TestWidgetsFlutterBinding.ensureInitialized();
    await S.preload();
  });

  testWidgets('onboarding then empty home', (tester) async {
    final state = await makeState(prefs: {'onboardingSeen': false});
    await tester.pumpWidget(Stl2StpApp(state: state));
    await tester.pumpAndSettle();
    expect(find.text('把 STL 变成可编辑的 CAD 文件'), findsOneWidget);
    await tester.tap(find.byKey(const Key('onboardingNext')));
    await tester.pumpAndSettle();
    expect(find.text('三步完成'), findsOneWidget);
    await tester.tap(find.byKey(const Key('onboardingNext')));
    await tester.pumpAndSettle();
    expect(find.text('导入 STL 文件'), findsOneWidget);
    expect(find.text('还没有转换记录'), findsOneWidget);
    expect(state.onboardingSeen, isTrue);
    // Demo engine shows the warning banner.
    expect(find.textContaining('演示模式'), findsOneWidget);
  });

  testWidgets('preview → convert (demo) → done → history', (tester) async {
    await tester.binding.setSurfaceSize(const Size(430, 1400));
    final state = await makeState();
    final tmp = Directory.systemTemp.createTempSync('stl_in');
    addTearDown(() => tmp.deleteSync(recursive: true));
    final input = '${tmp.path}/bracket.stl';
    File(input).writeAsBytesSync(buildBracketStl());
    await tester.pumpWidget(Stl2StpApp(state: state, homeOverride: PreviewScreen(model: sampleModel(input))));
    await tester.pumpAndSettle();
    expect(find.text('bracket.stl'), findsOneWidget);
    expect(find.text('28'), findsOneWidget); // triangles
    expect(find.text('60 × 40 × 50'), findsOneWidget);
    expect(find.text('二进制 STL'), findsOneWidget);
    expect(find.text('转换为 STEP'), findsOneWidget);

    // Switch to IGES: button label and IGES info banner change.
    await tester.tap(find.text('IGES').first);
    await tester.pumpAndSettle();
    expect(find.text('转换为 IGES'), findsOneWidget);
    expect(find.textContaining('IGES 在 CAD 中打开是一组曲面'), findsOneWidget);
    await tester.tap(find.text('STEP').first);
    await tester.pumpAndSettle();

    // Pick "smaller" preset → advanced shows simplify on.
    await tester.tap(find.byKey(const Key('preset_smaller')));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const Key('advancedToggle')));
    await tester.pumpAndSettle();
    expect(find.text('保留 50%（约 14 面）'), findsOneWidget);

    await tester.tap(find.byKey(const Key('convertButton')));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 16));
    expect(find.text('正在转换'), findsOneWidget);
    expect(find.text('减少面数'), findsWidgets);
    // Let the demo engine finish.
    for (var i = 0; i < 100 && find.byKey(const Key('doneTitle')).evaluate().isEmpty; i++) {
      await tester.pump(const Duration(milliseconds: 20));
    }
    await tester.pumpAndSettle();
    expect(find.text('转换完成'), findsOneWidget);
    expect(find.text('bracket.step'), findsOneWidget);
    expect(find.text('校验通过，文件可以正常打开'), findsOneWidget);
    expect(state.history, hasLength(1));
    expect(File(state.history.first.outputPath).existsSync(), isTrue);
    expect(state.history.first.settings.simplify, isTrue);
    await tester.binding.setSurfaceSize(null);
  });

  testWidgets('cancel asks for confirmation and shows cancelled screen', (tester) async {
    await tester.binding.setSurfaceSize(const Size(430, 1000));
    final state = await makeState(engine: DemoEngine(stepDelay: const Duration(milliseconds: 50)));
    await tester.pumpWidget(Stl2StpApp(state: state, homeOverride: PreviewScreen(model: sampleModel('/tmp/x.stl'))));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const Key('convertButton')));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 120));
    await tester.tap(find.byKey(const Key('cancelButton')));
    await tester.pumpAndSettle(const Duration(milliseconds: 10), EnginePhase.sendSemanticsUpdate, const Duration(seconds: 5));
    expect(find.text('确定要取消吗？'), findsOneWidget);
    await tester.tap(find.byKey(const Key('cancelYes')));
    for (var i = 0; i < 50 && find.byKey(const Key('errorTitle')).evaluate().isEmpty; i++) {
      await tester.pump(const Duration(milliseconds: 50));
    }
    await tester.pumpAndSettle();
    expect(find.text('已取消'), findsOneWidget);
    expect(find.text('回到预览'), findsOneWidget);
    expect(state.history, isEmpty);
    await tester.binding.setSurfaceSize(null);
  });

  testWidgets('engine failure maps to error screen with code', (tester) async {
    await tester.binding.setSurfaceSize(const Size(430, 1000));
    final state = await makeState(
        engine: DemoEngine(stepDelay: const Duration(milliseconds: 1), failWith: CoreStatus.errorOcct));
    await tester.pumpWidget(Stl2StpApp(state: state, homeOverride: PreviewScreen(model: sampleModel('/tmp/x.stl'))));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const Key('convertButton')));
    for (var i = 0; i < 100 && find.byKey(const Key('errorTitle')).evaluate().isEmpty; i++) {
      await tester.pump(const Duration(milliseconds: 10));
    }
    await tester.pumpAndSettle();
    expect(find.text('转换失败'), findsOneWidget);
    expect(find.textContaining('代码 -6'), findsOneWidget);
    expect(find.text('复制错误详情'), findsOneWidget);
    await tester.binding.setSurfaceSize(null);
  });

  testWidgets('English locale', (tester) async {
    final state = await makeState(prefs: {'language': 'en'});
    await tester.pumpWidget(Stl2StpApp(state: state));
    await tester.pumpAndSettle();
    expect(find.text('Import STL File'), findsOneWidget);
  });
}
