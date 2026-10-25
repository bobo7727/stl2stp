// Weld option, licences, split (iPad) layout + history swipe, memory guard,
// memory pressure, background task bridge and haptics.
import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/core/engine.dart';
import 'package:stl2stp_app/core/licenses.dart';
import 'package:stl2stp_app/core/memory_guard.dart';
import 'package:stl2stp_app/core/models.dart';
import 'package:stl2stp_app/core/sample_mesh.dart';
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/platform/background_task.dart';
import 'package:stl2stp_app/platform/haptics.dart';
import 'package:stl2stp_app/screens/flow.dart';
import 'package:stl2stp_app/screens/help_screen.dart';
import 'package:stl2stp_app/screens/home_screen.dart';
import 'package:stl2stp_app/screens/licenses_screen.dart';
import 'package:stl2stp_app/screens/preview_screen.dart';
import 'package:stl2stp_app/state/app_state.dart';
import 'package:stl2stp_app/state/history.dart';

import 'widget_flow_test.dart' show sampleModel;

Future<AppState> makeState({
  ConversionEngine? engine,
  MemoryGuard memoryGuard = const MemoryGuard(),
  int? budget,
  BackgroundTasks? background,
  Haptics? haptics,
}) async {
  SharedPreferences.setMockInitialValues({'onboardingSeen': true});
  final p = await SharedPreferences.getInstance();
  final dir = Directory.systemTemp.createTempSync('stl2stp_feat');
  addTearDown(() => dir.deleteSync(recursive: true));
  return AppState(
    engine: engine ?? DemoEngine(stepDelay: const Duration(milliseconds: 5)),
    prefs: p,
    outputDir: dir,
    memoryGuard: memoryGuard,
    memoryBudgetBytes: budget ?? 4000 * 1000 * 1000,
    background: background ?? BackgroundTasks(enabled: false),
    haptics: haptics ?? Haptics(enabled: false),
  );
}

String bracketFile() {
  final tmp = Directory.systemTemp.createTempSync('stl_in');
  addTearDown(() => tmp.deleteSync(recursive: true));
  final input = '${tmp.path}/bracket.stl';
  File(input).writeAsBytesSync(buildBracketStl());
  return input;
}

/// Sets the window (and so MediaQuery) to [size] logical pixels; the default
/// test view (800×600) would otherwise count as a tablet.
void setLogicalSize(WidgetTester tester, Size size) {
  tester.view.devicePixelRatio = 1;
  tester.view.physicalSize = size;
  addTearDown(tester.view.reset);
}

Future<void> pumpUntil(WidgetTester tester, Finder f, {int max = 200}) async {
  for (var i = 0; i < max && f.evaluate().isEmpty; i++) {
    await tester.pump(const Duration(milliseconds: 20));
  }
  await tester.pumpAndSettle();
}

HistoryEntry entry(String path, {String source = 'a.stl'}) => HistoryEntry(
    outputPath: path,
    format: ExportFormat.step,
    bytes: 1234,
    time: DateTime.now(),
    sourceName: source,
    settings: const ConvertSettings());

void main() {
  setUpAll(() async {
    TestWidgetsFlutterBinding.ensureInitialized();
    await S.preload();
  });

  group('weld option', () {
    test('default on, json round trip, off makes the preset custom', () {
      const s = ConvertSettings();
      expect(s.weld, isTrue);
      expect(s.preset, Preset.recommended);
      final off = s.copyWith(weld: false);
      expect(off.preset, isNull);
      expect(ConvertSettings.fromJson(off.toJson()).weld, isFalse);
      expect(ConvertSettings.fromJson(const {}).weld, isTrue); // old saved settings
    });

    testWidgets('advanced settings toggle and help topic', (tester) async {
      setLogicalSize(tester, const Size(430, 1600));
      final state = await makeState();
      await tester.pumpWidget(Stl2StpApp(state: state, homeOverride: PreviewScreen(model: sampleModel(bracketFile()))));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('advancedToggle')));
      await tester.pumpAndSettle();
      final sw = find.byKey(const Key('weldSwitch'));
      await tester.ensureVisible(sw);
      expect(find.text('合并重合顶点'), findsOneWidget);
      expect(tester.widget<SwitchListTile>(sw).value, isTrue);
      await tester.tap(sw);
      await tester.pumpAndSettle();
      expect(tester.widget<SwitchListTile>(sw).value, isFalse);
      expect(helpTopics, contains('weld'));
      expect(S.of(tester.element(sw)).t('help.weld.body'), contains('百万分之一'));
    });
  });

  group('licences', () {
    testWidgets('OCCT LGPL + exception text and the package licence page', (tester) async {
      setLogicalSize(tester, const Size(430, 1000));
      final state = await makeState();
      await tester.pumpWidget(Stl2StpApp(state: state, homeOverride: const LicensesScreen()));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('occtLicenseText')));
      // The text loads asynchronously behind a spinner: pump real time.
      for (var i = 0; i < 50 && find.byKey(const Key('occtLicenseBody')).evaluate().isEmpty; i++) {
        await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 20)));
        await tester.pump(const Duration(milliseconds: 50));
      }
      await tester.pumpAndSettle();
      final body = tester.widget<SelectableText>(find.byKey(const Key('occtLicenseBody'))).data!;
      expect(body, contains('GNU LESSER GENERAL PUBLIC LICENSE'));
      expect(body, contains('Version 2.1'));
      expect(body, contains('Open CASCADE exception (version 1.0)'));
      // The entries registered for showLicensePage (the full registry also
      // parses Flutter's NOTICES, which is not available in widget tests).
      // (rootBundle already cached both texts in the test zone above.)
      List<LicenseEntry>? entries;
      occtLicenseEntries().toList().then((v) => entries = v);
      for (var i = 0; i < 50 && entries == null; i++) {
        await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 20)));
        await tester.pump();
      }
      expect(entries!, hasLength(2));
      expect(entries!.every((e) => e.packages.contains(occtPackageName)), isTrue);
      expect(entries!.first.paragraphs.first.text, contains('GNU LESSER'));
      registerAppLicenses();
      // "All licences" opens Flutter's licence page.
      tester.state<NavigatorState>(find.byType(Navigator).first).pop();
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('allLicenses')));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 500));
      expect(find.byType(LicensePage), findsOneWidget);
    });
  });

  group('split layout (iPad)', () {
    test('threshold', () {
      expect(useSplitLayout(const Size(430, 932)), isFalse); // iPhone portrait
      expect(useSplitLayout(const Size(932, 430)), isTrue); // wide window
      expect(useSplitLayout(const Size(820, 1180)), isTrue); // iPad portrait (tablet)
      expect(useSplitLayout(const Size(1180, 820)), isTrue); // iPad landscape
      expect(useSplitLayout(const Size(375, 1180)), isFalse); // iPad Slide Over
    });

    testWidgets('wide: history left, preview opens in the right pane', (tester) async {
      setLogicalSize(tester, const Size(1280, 860));
      final state = await makeState();
      final out = File('${state.outputDir.path}/done.step')..writeAsStringSync('x');
      state.addHistory(entry(out.path));
      await tester.pumpWidget(Stl2StpApp(state: state));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('splitLeft')), findsOneWidget);
      expect(find.byKey(const Key('detailPlaceholder')), findsOneWidget);
      expect(find.text('done.step'), findsOneWidget);
      final input = bracketFile();
      final ctx = tester.element(find.byKey(const Key('importCard')));
      await tester.runAsync(() async {
        unawaited(openModel(ctx, input));
        await Future<void>.delayed(const Duration(milliseconds: 800));
      });
      await tester.pumpAndSettle();
      expect(find.byType(PreviewScreen), findsOneWidget);
      // Left pane stays visible next to the preview.
      expect(find.byKey(const Key('importCard')), findsOneWidget);
      expect(find.text('done.step'), findsOneWidget);
      // The preview sits inside the right pane's navigator.
      expect(find.descendant(of: find.byType(Navigator).last, matching: find.byType(PreviewScreen)), findsOneWidget);
      expect(tester.getTopLeft(find.byType(PreviewScreen)).dx, greaterThanOrEqualTo(splitLeftWidth));
    });

    testWidgets('phone: unchanged single column, preview is a pushed page', (tester) async {
      setLogicalSize(tester, const Size(430, 932));
      final state = await makeState();
      await tester.pumpWidget(Stl2StpApp(state: state));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('splitLeft')), findsNothing);
      expect(find.byKey(const Key('detailPlaceholder')), findsNothing);
      final input = bracketFile();
      final ctx = tester.element(find.byKey(const Key('importCard')));
      await tester.runAsync(() async {
        unawaited(openModel(ctx, input));
        await Future<void>.delayed(const Duration(milliseconds: 800));
      });
      await tester.pumpAndSettle();
      expect(find.byType(PreviewScreen), findsOneWidget);
      expect(find.byKey(const Key('importCard')), findsNothing); // covered by the pushed page
    });
  });

  group('history swipe', () {
    testWidgets('swipe right falls back when sharing is unavailable, missing file explains, swipe left deletes', (tester) async {
      setLogicalSize(tester, const Size(430, 932));
      String? clipboard;
      const shareChannel = MethodChannel('dev.fluttercommunity.plus/share');
      final nativeShareCalls = <MethodCall>[];
      tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(SystemChannels.platform, (call) async {
        if (call.method == 'Clipboard.setData') clipboard = (call.arguments as Map)['text'] as String;
        return null;
      });
      // macOS/iOS/Android try the native sheet first. Make its failure explicit
      // so this fallback test is independent of the machine running Flutter.
      tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(shareChannel, (call) async {
        nativeShareCalls.add(call);
        throw PlatformException(code: 'share_unavailable', message: 'Simulated unavailable share sheet');
      });
      addTearDown(() {
        tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(SystemChannels.platform, null);
        tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(shareChannel, null);
      });
      final state = await makeState();
      final out = File('${state.outputDir.path}/keep.step')..writeAsStringSync('x');
      state.addHistory(entry('${state.outputDir.path}/gone.step'));
      state.addHistory(entry(out.path));
      await tester.pumpWidget(Stl2StpApp(state: state));
      await tester.pumpAndSettle();

      // Sharing is unsupported (Linux/Windows) or explicitly fails above
      // (macOS/iOS/Android): the path is copied and the history row stays.
      await tester.drag(find.text('keep.step'), const Offset(300, 0));
      await tester.pumpAndSettle();
      expect(clipboard, out.path);
      final usesNativeShare = Platform.isIOS || Platform.isAndroid || Platform.isMacOS;
      expect(nativeShareCalls, hasLength(usesNativeShare ? 1 : 0));
      if (usesNativeShare) {
        expect(nativeShareCalls.single.method, 'share');
        expect((nativeShareCalls.single.arguments as Map)['paths'], [out.path]);
      }
      expect(find.textContaining('文件路径已复制'), findsOneWidget);
      expect(state.history, hasLength(2));
      ScaffoldMessenger.of(tester.element(find.text('keep.step'))).removeCurrentSnackBar();
      await tester.pumpAndSettle();

      // Missing output: clear message with a remove action, no share.
      clipboard = null;
      await tester.drag(find.text('gone.step'), const Offset(300, 0));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('historyMissingSnack')), findsOneWidget);
      expect(find.textContaining('无法分享'), findsOneWidget);
      expect(clipboard, isNull);
      expect(nativeShareCalls, hasLength(usesNativeShare ? 1 : 0)); // missing file is never shared
      await tester.tap(find.text('删除记录'));
      await tester.pumpAndSettle();
      expect(state.history.map((e) => e.fileName), ['keep.step']);

      // Swipe left deletes the record and the file.
      await tester.drag(find.text('keep.step'), const Offset(-300, 0));
      await tester.pumpAndSettle();
      expect(state.history, isEmpty);
      expect(out.existsSync(), isFalse);
    });
  });

  group('memory guard', () {
    test('peak model and decisions', () {
      const g = MemoryGuard(baseBytes: 0, perFace: 1e6, simplifyPerTriangle: 0);
      const s = ConvertSettings();
      expect(g.peakBytes(28, s), 28000000);
      expect(g.peakBytes(28, s.copyWith(mode: ConvertMode.sewing)), greaterThan(28000000));
      expect(g.check(28, s, 100000000).level, MemoryLevel.ok);
      final warn = g.check(28, s, 30000000);
      expect(warn.level, MemoryLevel.warn);
      expect(warn.suggestPercent, 75); // 21 faces = 21 MB ≤ 0.7 × 30 MB
      final refuse = g.check(28, s, 20000000);
      expect(refuse.level, MemoryLevel.refuse);
      expect(refuse.suggestPercent, 50);
      // Calibrated defaults (tool/calib/memtime.csv): never below the measured
      // max RSS; 30k faces fit the 1.5 GB fallback, 100k (≈2 GB) does not.
      const d = MemoryGuard();
      const measured = {5000: 140e6, 20224: 437e6, 50000: 1016e6, 101760: 2017e6};
      measured.forEach((tris, rss) => expect(d.peakBytes(tris, s), greaterThanOrEqualTo(rss)));
      expect(d.peakBytes(50000, s.copyWith(mode: ConvertMode.sewing)), greaterThanOrEqualTo(1069e6));
      expect(d.peakBytes(101760, s.copyWith(simplify: true, keepPercent: 50)), greaterThanOrEqualTo(1028e6));
      expect(d.check(30000, s, MemoryBudget.fallbackBytes).level, MemoryLevel.ok);
      final big = d.check(101760, s, MemoryBudget.fallbackBytes);
      expect(big.level, MemoryLevel.refuse);
      expect(big.suggestPercent, isNotNull);
    });

    test('budget detection: override, /proc/meminfo parsing', () async {
      expect(await MemoryBudget.detect(overrideBytes: 123), 123);
      final f = File('${Directory.systemTemp.createTempSync('mem').path}/meminfo')
        ..writeAsStringSync('MemTotal:       16000000 kB\nMemAvailable:    8000000 kB\n');
      expect(MemoryBudget.linuxAvailable(f.path), 8000000 * 1024);
      expect(MemoryBudget.linuxAvailable('/nonexistent'), isNull);
    });

    testWidgets('warning dialog offers to simplify before converting', (tester) async {
      setLogicalSize(tester, const Size(430, 1200));
      final state = await makeState(
          memoryGuard: const MemoryGuard(baseBytes: 0, perFace: 1e6, simplifyPerTriangle: 0), budget: 30000000);
      await tester.pumpWidget(Stl2StpApp(state: state, homeOverride: PreviewScreen(model: sampleModel(bracketFile()))));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('convertButton')));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('memoryDialog')), findsOneWidget);
      expect(find.text('这个模型可能占用很多内存'), findsOneWidget);
      expect(find.textContaining('26.7 MB'), findsOneWidget); // 28e6 B
      await tester.tap(find.byKey(const Key('memorySimplify')));
      await pumpUntil(tester, find.byKey(const Key('doneTitle')));
      expect(state.history.single.settings.simplify, isTrue);
      expect(state.history.single.settings.keepPercent, 75);
    });

    testWidgets('refuses when even the conversion alone does not fit', (tester) async {
      setLogicalSize(tester, const Size(430, 1200));
      final state = await makeState(
          memoryGuard: const MemoryGuard(baseBytes: 0, perFace: 1e6, simplifyPerTriangle: 0), budget: 20000000);
      await tester.pumpWidget(Stl2StpApp(state: state, homeOverride: PreviewScreen(model: sampleModel(bracketFile()))));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('convertButton')));
      await tester.pumpAndSettle();
      expect(find.text('内存不够'), findsOneWidget);
      expect(find.byKey(const Key('memoryContinue')), findsNothing);
      await tester.tap(find.text('取消'));
      await tester.pumpAndSettle();
      expect(find.byType(PreviewScreen), findsOneWidget);
      expect(state.history, isEmpty);
    });
  });

  group('running conversion', () {
    testWidgets('memory pressure cancels cleanly with a clear message', (tester) async {
      setLogicalSize(tester, const Size(430, 1000));
      final state = await makeState(engine: DemoEngine(stepDelay: const Duration(milliseconds: 40)));
      await tester.pumpWidget(Stl2StpApp(state: state, homeOverride: PreviewScreen(model: sampleModel(bracketFile()))));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('convertButton')));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 100));
      expect(find.text('正在转换'), findsOneWidget);
      tester.binding.handleMemoryPressure();
      await pumpUntil(tester, find.byKey(const Key('errorTitle')));
      expect(find.byKey(const Key('errorNotice')), findsOneWidget);
      expect(find.textContaining('内存紧张'), findsWidgets);
      expect(state.history, isEmpty);
    });

    testWidgets('background task begins/ends around a conversion; expiry stops it', (tester) async {
      setLogicalSize(tester, const Size(430, 1000));
      const channel = MethodChannel('test/background');
      final calls = <String>[];
      var nextId = 7;
      tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(channel, (call) async {
        calls.add('${call.method}:${call.method == 'end' ? (call.arguments as Map)['id'] : ''}');
        return call.method == 'begin' ? nextId++ : null;
      });
      addTearDown(() => tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(channel, null));
      final bg = BackgroundTasks(channel: channel, enabled: true);
      final hapticCalls = <Object?>[];
      tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(SystemChannels.platform, (call) async {
        if (call.method == 'HapticFeedback.vibrate') hapticCalls.add(call.arguments);
        return null;
      });
      addTearDown(() => tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(SystemChannels.platform, null));
      final state = await makeState(
          engine: DemoEngine(stepDelay: const Duration(milliseconds: 20)), background: bg, haptics: Haptics(enabled: true));
      await tester.pumpWidget(Stl2StpApp(state: state, homeOverride: PreviewScreen(model: sampleModel(bracketFile()))));
      await tester.pumpAndSettle();

      // 1. Normal run: begin → end(7); light haptics on start and success.
      await tester.tap(find.byKey(const Key('convertButton')));
      for (var i = 0; i < 20 && find.text('可以切到其他 App，转换会在后台继续一小段时间').evaluate().isEmpty; i++) {
        await tester.pump(const Duration(milliseconds: 5));
      }
      expect(find.text('可以切到其他 App，转换会在后台继续一小段时间'), findsOneWidget);
      await pumpUntil(tester, find.byKey(const Key('doneTitle')));
      expect(calls, ['begin:', 'end:7']);
      expect(hapticCalls, ['HapticFeedbackType.lightImpact', 'HapticFeedbackType.lightImpact']);

      // 2. Second run, the OS expires the task: conversion stops with a message, failure haptic.
      await tester.tap(find.text('再转一个'));
      await tester.pumpAndSettle();
      final state2 = state; // same app state
      expect(state2.history, hasLength(1));
      await tester.pumpWidget(Stl2StpApp(
          key: const Key('second'), state: state, homeOverride: PreviewScreen(model: sampleModel(bracketFile()))));
      await tester.pumpAndSettle();
      hapticCalls.clear();
      await tester.tap(find.byKey(const Key('convertButton')));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 60));
      bg.handleExpired(8);
      await pumpUntil(tester, find.byKey(const Key('errorTitle')));
      expect(find.textContaining('后台时间'), findsWidgets);
      expect(calls.last, 'end:8');
      expect(hapticCalls.last, 'HapticFeedbackType.mediumImpact');
      expect(state.history, hasLength(1));
    });

    test('background and haptics are no-ops on desktop', () async {
      final bg = BackgroundTasks();
      expect(bg.enabled, Platform.isIOS);
      expect(await bg.begin('x'), isNull);
      await bg.end(null);
      expect(Haptics().enabled, Platform.isIOS || Platform.isAndroid);
      expect(kIsWeb, isFalse);
    });
  });
}
