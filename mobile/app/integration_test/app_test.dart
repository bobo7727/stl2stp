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
import 'package:stl2stp_app/screens/help_screen.dart';
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
  testWidgets('real FFI weld on/off, help, licences, memory warning, split layout, history swipe', (tester) async {
    await tester.pumpWidget(Stl2StpApp(state: state));
    await tester.pumpAndSettle();
    final openPole = p.join(work.path, 'open_pole_sphere.stl');
    File(openPole).writeAsBytesSync(buildSphereStl(seg: 64, rings: 32, exactPoles: false));

    Future<String> convertOpenPole({required bool weld, String? shotName}) async {
      final ctx = tester.element(find.byKey(const Key('importCard')));
      // ignore: unawaited_futures
      openModel(ctx, openPole);
      await pumpUntil(tester, find.byKey(const Key('convertButton')));
      // Recommended preset (ORIGINAL, weld on), STEP: earlier tests left IGES/strict selected.
      await tester.tap(find.byKey(const Key('preset_recommended')));
      await tester.pumpAndSettle();
      await tester.tap(find.text('STEP').first);
      await tester.pumpAndSettle();
      final sw = find.byKey(const Key('weldSwitch'));
      if (sw.evaluate().isEmpty) {
        await tester.dragUntilVisible(find.byKey(const Key('advancedToggle')), find.byType(ListView).first, const Offset(0, -200));
        await tester.pumpAndSettle();
        await tester.tap(find.byKey(const Key('advancedToggle')));
        await tester.pumpAndSettle();
      }
      await tester.dragUntilVisible(sw, find.byType(ListView).first, const Offset(0, -150));
      await tester.pumpAndSettle();
      if (tester.widget<SwitchListTile>(sw).value != weld) {
        await tester.tap(sw);
        await tester.pumpAndSettle();
      }
      if (shotName != null) await shot(tester, shotName);
      await tester.ensureVisible(find.byKey(const Key('convertButton')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('convertButton')));
      await pumpUntil(tester, find.byKey(const Key('doneTitle')), timeout: const Duration(minutes: 3));
      return state.history.first.outputPath;
    }

    // 1) Open-pole sphere: weld on (default) → one closed solid; weld off → open shell.
    final on = File(await convertOpenPole(weld: true, shotName: '18_preview_weld_toggle')).readAsStringSync();
    final onSolids = 'MANIFOLD_SOLID_BREP'.allMatches(on).length;
    expect(onSolids, 1);
    log('WELD_ON open-pole sphere: MANIFOLD_SOLID_BREP=$onSolids');
    await tester.tap(find.byKey(const Key('doneButton')));
    await tester.pumpAndSettle();
    final off = File(await convertOpenPole(weld: false)).readAsStringSync();
    await shot(tester, '19_done_weld_off_open');
    expect('MANIFOLD_SOLID_BREP'.allMatches(off).length, 0);
    expect(off.contains('SHELL_BASED_SURFACE_MODEL'), isTrue);
    log('WELD_OFF open-pole sphere: MANIFOLD_SOLID_BREP=0, open shell');
    await tester.tap(find.byKey(const Key('doneButton')));
    await tester.pumpAndSettle();

    // 2) Help topic for the weld option.
    // ignore: unawaited_futures
    openHelp(tester.element(find.byKey(const Key('importCard'))), topic: 'weld');
    await tester.pumpAndSettle();
    await tester.pump(const Duration(milliseconds: 600));
    await shot(tester, '20_help_weld');
    navigatorKey.currentState!.pop();
    await tester.pumpAndSettle();

    // 3) Settings → 开源许可 → OCCT text; Flutter licence page.
    await tester.tap(find.byKey(const Key('settingsButton')));
    await tester.pumpAndSettle();
    await tester.dragUntilVisible(find.text('开源许可'), find.byType(ListView).first, const Offset(0, -200));
    await tester.pumpAndSettle();
    await tester.tap(find.text('开源许可'));
    await tester.pumpAndSettle();
    await shot(tester, '21_licenses');
    await tester.tap(find.byKey(const Key('occtLicenseText')));
    await pumpUntil(tester, find.byKey(const Key('occtLicenseBody')), timeout: const Duration(seconds: 10));
    await shot(tester, '22_occt_license_text');
    expect(tester.widget<SelectableText>(find.byKey(const Key('occtLicenseBody'))).data, contains('Open CASCADE exception'));
    navigatorKey.currentState!.pop();
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const Key('allLicenses')));
    await tester.pump(const Duration(seconds: 2));
    await tester.pumpAndSettle();
    await shot(tester, '23_package_licenses');
    navigatorKey.currentState!.popUntil((r) => r.isFirst);
    await tester.pumpAndSettle();

    // 4) Memory warning: a 150 MB budget makes the 3968-face sphere a warning.
    state.memoryBudgetMB = 150;
    final sphere = p.join(work.path, 'sphere_mem.stl');
    File(sphere).writeAsBytesSync(buildSphereStl(seg: 64, rings: 32));
    // ignore: unawaited_futures
    openModel(tester.element(find.byKey(const Key('importCard'))), sphere);
    await pumpUntil(tester, find.byKey(const Key('convertButton')));
    await tester.tap(find.byKey(const Key('convertButton')));
    await pumpUntil(tester, find.byKey(const Key('memoryDialog')), timeout: const Duration(seconds: 10));
    await tester.pumpAndSettle();
    await shot(tester, '24_memory_warning');
    log('MEMORY_WARN ${tester.widget<AlertDialog>(find.byKey(const Key('memoryDialog'))).title is Text ? (tester.widget<AlertDialog>(find.byKey(const Key('memoryDialog'))).title as Text).data : ''}');
    await tester.tap(find.text('取消'));
    await tester.pumpAndSettle();
    state.memoryBudgetMB = null;
    navigatorKey.currentState!.popUntil((r) => r.isFirst);
    await tester.pumpAndSettle();

    // 5) History swipe (held halfway to show the share action).
    final row = find.text(state.history.first.fileName).first;
    final g = await tester.startGesture(tester.getCenter(row));
    await g.moveBy(const Offset(40, 0));
    await tester.pump();
    await g.moveBy(const Offset(110, 0));
    await tester.pump(const Duration(milliseconds: 300));
    await shot(tester, '25_history_swipe_share');
    await g.moveBy(const Offset(-150, 0));
    await g.up();
    await tester.pumpAndSettle();

    // 6) iPad-width window: two panes, preview on the right.
    tester.view.devicePixelRatio = 1;
    tester.view.physicalSize = const Size(1280, 860);
    await tester.pumpWidget(Stl2StpApp(state: state));
    await tester.pumpAndSettle();
    await shot(tester, '26_wide_split_home');
    // ignore: unawaited_futures
    openModel(tester.element(find.byKey(const Key('importCard'))), sphere);
    await pumpUntil(tester, find.byKey(const Key('convertButton')));
    await tester.pumpAndSettle();
    await shot(tester, '27_wide_split_preview');
    expect(find.byKey(const Key('splitLeft')), findsOneWidget);
    tester.view.reset();
    await tester.pumpWidget(Stl2StpApp(state: state));
    await tester.pumpAndSettle();
  });
}
