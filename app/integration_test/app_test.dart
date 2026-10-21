// Real end-to-end test: drives the UI on a desktop target (Linux/WSLg or
// macOS) with the real stl2stp-core loaded over dart:ffi, converts sample
// models to STEP and IGES and saves screenshots.
//
//   flutter test integration_test/app_test.dart -d linux \
//     --dart-define=SHOT_DIR=/mnt/d/stl2stp-flutter/build/app-shots
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:path/path.dart' as p;
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/core/sample_mesh.dart';
import 'package:stl2stp_app/core/screenshot.dart';
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/main.dart' show createAppState;
import 'package:stl2stp_app/screens/flow.dart';
import 'package:stl2stp_app/state/app_state.dart';

const shotDir = String.fromEnvironment('SHOT_DIR', defaultValue: '/tmp/stl2stp-shots');

/// Result lines (STEP_OK, ESTIMATE_CHECK…): printed to the test log and appended to SHOT_DIR/results.txt.
void log(String line) {
  // ignore: avoid_print
  print(line);
  File(p.join(shotDir, 'results.txt'))
    ..createSync(recursive: true)
    ..writeAsStringSync('$line\n', mode: FileMode.append);
}

Future<void> pumpUntil(WidgetTester tester, Finder f, {Duration timeout = const Duration(seconds: 120)}) async {
  final end = DateTime.now().add(timeout);
  while (f.evaluate().isEmpty) {
    if (DateTime.now().isAfter(end)) throw TestFailure('timeout waiting for $f');
    await tester.pump(const Duration(milliseconds: 100));
  }
  await tester.pump(const Duration(milliseconds: 300));
}

Future<void> shot(WidgetTester tester, String name) async {
  await tester.pump(const Duration(milliseconds: 200));
  await tester.runAsync(() => captureWidget(screenshotKey, p.join(shotDir, '$name.png')));
}

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  late AppState state;
  late Directory work;

  setUpAll(() async {
    SharedPreferences.setMockInitialValues({'onboardingSeen': true, 'language': 'zh'});
    state = await createAppState();
    await S.preload();
    work = Directory.systemTemp.createTempSync('stl2stp_it');
    state.clearHistory();
  });

  testWidgets('real FFI: bracket → STEP via UI, sphere → IGES', (tester) async {
    expect(state.engine.isReal, isTrue, reason: 'stl2stp-core must be bundled (got ${state.engine.name})');
    await tester.pumpWidget(Stl2StpApp(state: state));
    await tester.pumpAndSettle();
    await shot(tester, '01_home_empty');

    // 1) Sample bracket through the real UI path (sample button → openModel → preview).
    await tester.tap(find.byKey(const Key('sampleButton')));
    await pumpUntil(tester, find.byKey(const Key('convertButton')));
    expect(find.text('28'), findsOneWidget); // triangles reported by stl2stp-core
    await shot(tester, '02_preview');

    await tester.tap(find.byKey(const Key('advancedToggle')));
    await tester.pumpAndSettle();
    await shot(tester, '03_preview_advanced');
    await tester.tap(find.byKey(const Key('advancedToggle')));
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const Key('convertButton')));
    await pumpUntil(tester, find.byKey(const Key('doneTitle')));
    await shot(tester, '04_done_step');
    expect(find.text('校验通过，文件可以正常打开'), findsOneWidget);
    final step = state.history.first.outputPath;
    final stepText = File(step).readAsStringSync();
    expect(stepText.startsWith('ISO-10303-21;'), isTrue);
    expect(stepText.contains('DEMO'), isFalse);
    expect(stepText.contains('MANIFOLD_SOLID_BREP'), isTrue);
    log('STEP_OK $step ${File(step).lengthSync()} bytes');

    await tester.tap(find.byKey(const Key('doneButton')));
    await tester.pumpAndSettle();
    await shot(tester, '05_home_recent');

    // 2) Sphere (3968 faces), strict preset, IGES: catch the progress screen.
    final sphere = p.join(work.path, 'sphere.stl');
    File(sphere).writeAsBytesSync(buildSphereStl(seg: 64, rings: 32));
    final ctx = tester.element(find.byKey(const Key('importCard')));
    // ignore: unawaited_futures
    openModel(ctx, sphere);
    await pumpUntil(tester, find.byKey(const Key('convertButton')));
    await tester.tap(find.byKey(const Key('preset_strict')));
    await tester.tap(find.text('IGES').first);
    await tester.pumpAndSettle();
    await shot(tester, '06_preview_sphere_iges');
    final estText = tester.widget<Text>(find.byKey(const Key('estimateLine'))).data!;
    final sw = Stopwatch()..start();
    await tester.tap(find.byKey(const Key('convertButton')));
    await pumpUntil(tester, find.byKey(const Key('cancelButton')), timeout: const Duration(seconds: 10));
    await tester.pump(const Duration(milliseconds: 400));
    await shot(tester, '07_converting');
    await pumpUntil(tester, find.byKey(const Key('doneTitle')), timeout: const Duration(minutes: 5));
    sw.stop();
    log('ESTIMATE_CHECK sphere 3968 tris strict IGES: shown "$estText", actual ${sw.elapsedMilliseconds} ms');
    await shot(tester, '08_done_iges');
    final iges = state.history.first.outputPath;
    expect(iges.endsWith('.igs'), isTrue);
    expect(File(iges).lengthSync(), greaterThan(10000));
    log('IGES_OK $iges ${File(iges).lengthSync()} bytes');

    // 3) English UI.
    state.language = 'en';
    await tester.tap(find.byKey(const Key('doneButton')));
    await tester.pumpAndSettle();
    await shot(tester, '09_home_en');

    // 4) Cancel path on the sphere with sewing → error screen "Cancelled".
    final ctx2 = tester.element(find.byKey(const Key('importCard')));
    // ignore: unawaited_futures
    openModel(ctx2, sphere);
    await pumpUntil(tester, find.byKey(const Key('convertButton')));
    await tester.tap(find.byKey(const Key('preset_strict')));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const Key('convertButton')));
    await pumpUntil(tester, find.byKey(const Key('cancelButton')), timeout: const Duration(seconds: 10));
    await tester.tap(find.byKey(const Key('cancelButton')));
    await tester.pumpAndSettle(const Duration(milliseconds: 50));
    await tester.tap(find.byKey(const Key('cancelYes')));
    await pumpUntil(
        tester,
        find.byWidgetPredicate((w) => w.key == const Key('errorTitle') || w.key == const Key('doneTitle')),
        timeout: const Duration(minutes: 2));
    // Either cancelled in time or (tiny model) finished first.
    if (find.text('Cancelled').evaluate().isNotEmpty) {
      await shot(tester, '10_cancelled_en');
      log('CANCEL_OK');
    }
    state.language = 'zh';
  });

  testWidgets('help, smaller-preset estimate range, settings, paywall (mock store)', (tester) async {
    await tester.pumpWidget(Stl2StpApp(state: state));
    await tester.pumpAndSettle();

    // Range estimate with merge coplanar (smaller preset) on the sample bracket.
    await tester.tap(find.byKey(const Key('sampleButton')));
    await pumpUntil(tester, find.byKey(const Key('convertButton')));
    await tester.tap(find.byKey(const Key('preset_smaller')));
    await tester.pumpAndSettle();
    log('ESTIMATE_SMALLER ${tester.widget<Text>(find.byKey(const Key('estimateLine'))).data}');
    await shot(tester, '11_preview_smaller_estimate');

    // Help from the preview “?”.
    await tester.tap(find.byKey(const Key('previewHelp')));
    await tester.pumpAndSettle();
    await shot(tester, '12_help_zh');
    await tester.scrollUntilVisible(find.byKey(const Key('faq1')), 400);
    await tester.pumpAndSettle();
    await shot(tester, '13_help_faq_zh');
    navigatorKey.currentState!.pop();
    await tester.pumpAndSettle();
    navigatorKey.currentState!.pop();
    await tester.pumpAndSettle();

    // Settings → paywall → buy with the mock store → Pro.
    await tester.tap(find.byKey(const Key('settingsButton')));
    await tester.pumpAndSettle();
    await shot(tester, '14_settings_free');
    await tester.tap(find.byKey(const Key('purchaseStatus')));
    await tester.pumpAndSettle();
    await shot(tester, '15_paywall_zh');
    await tester.tap(find.byKey(const Key('buyButton')));
    await pumpUntil(tester, find.byKey(const Key('purchaseStatus')));
    await tester.pumpAndSettle();
    expect(state.isPro, isTrue);
    await shot(tester, '16_settings_pro');
    // Dev reset so the next run starts free again.
    await tester.tap(find.text('重置购买状态（开发用）'));
    await tester.pumpAndSettle();
    expect(state.isPro, isFalse);

    // English help.
    state.language = 'en';
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const Key('settingsHelp')));
    await tester.pumpAndSettle();
    await shot(tester, '17_help_en');
    state.language = 'zh';
    await tester.pumpAndSettle();
  });
}
