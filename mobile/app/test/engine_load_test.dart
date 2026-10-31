import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/core/engine.dart';
import 'package:stl2stp_app/core/ffi_engine.dart';
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/main.dart' show createEngine;
import 'package:stl2stp_app/state/app_state.dart';

void main() {
  setUpAll(() async {
    TestWidgetsFlutterBinding.ensureInitialized();
    await S.preload();
  });

  ConversionEngine Function() countingDemo(List<int> constructed) {
    return () {
      constructed.add(1);
      return DemoEngine();
    };
  }

  group('createEngine', () {
    test('release load failure does not instantiate DemoEngine', () {
      final constructed = <int>[];
      final engine = createEngine(
        releaseMode: true,
        environment: const {},
        loadNative: () => null,
        createDemo: countingDemo(constructed),
      );
      expect(constructed, isEmpty);
      expect(engine, isA<UnavailableEngine>());
      expect(engine, isNot(isA<DemoEngine>()));
      expect(engine.isReal, isFalse);
    });

    test('release init failure does not instantiate DemoEngine', () {
      final constructed = <int>[];
      final engine = createEngine(
        releaseMode: true,
        environment: const {'STL2STP_DEMO': '1'},
        loadNative: () => throw StateError('stl2stp_create failed'),
        createDemo: countingDemo(constructed),
      );
      expect(constructed, isEmpty);
      expect(engine, isA<UnavailableEngine>());
      expect((engine as UnavailableEngine).detail, 'stl2stp_create failed');
    });

    test('successful load uses the native engine', () {
      final constructed = <int>[];
      final native = FfiEngine.testInstance();
      final engine = createEngine(
        releaseMode: true,
        environment: const {'STL2STP_DEMO': '1'},
        loadNative: () => native,
        createDemo: countingDemo(constructed),
      );
      expect(identical(engine, native), isTrue);
      expect(engine, isA<FfiEngine>());
      expect(engine.isReal, isTrue);
      expect(constructed, isEmpty);
    });

    test('release dart-define still selects the demo engine explicitly', () {
      var loaded = false;
      final engine = createEngine(
        releaseMode: true,
        allowDemoDefine: true,
        environment: const {},
        loadNative: () {
          loaded = true;
          return null;
        },
      );
      expect(loaded, isFalse);
      expect(engine, isA<DemoEngine>());
    });

    test('debug STL2STP_DEMO=1 forces the demo engine without loading the core', () {
      var loaded = false;
      final engine = createEngine(
        releaseMode: false,
        environment: const {'STL2STP_DEMO': '1'},
        loadNative: () {
          loaded = true;
          return FfiEngine.testInstance();
        },
      );
      expect(loaded, isFalse);
      expect(engine, isA<DemoEngine>());
    });

    test('debug falls back to the demo engine when the native core is missing', () {
      final engine = createEngine(
        releaseMode: false,
        environment: const {},
        loadNative: () => null,
      );
      expect(engine, isA<DemoEngine>());
    });
  });

  group('core load error screen', () {
    Future<AppState> stateFor(ConversionEngine engine) async {
      SharedPreferences.setMockInitialValues({'onboardingSeen': false, 'language': 'en'});
      final prefs = await SharedPreferences.getInstance();
      final dir = Directory.systemTemp.createTempSync('stl2stp_core_load');
      addTearDown(() => dir.deleteSync(recursive: true));
      return AppState(engine: engine, prefs: prefs, outputDir: dir);
    }

    testWidgets('release-mode load failure shows the error state', (tester) async {
      final constructed = <int>[];
      final engine = createEngine(
        releaseMode: true,
        environment: const {},
        loadNative: () => null,
        createDemo: countingDemo(constructed),
      );
      await tester.pumpWidget(Stl2StpApp(state: await stateFor(engine)));
      await tester.pumpAndSettle();

      expect(constructed, isEmpty);
      expect(engine, isA<UnavailableEngine>());
      expect(find.byKey(const Key('coreLoadError')), findsOneWidget);
      expect(find.byKey(const Key('coreLoadErrorTitle')), findsOneWidget);
      expect(find.text('Conversion engine unavailable'), findsOneWidget);
      expect(find.textContaining('It failed to load'), findsOneWidget);
      expect(find.text('stl2stp-core failed to load'), findsOneWidget);
      expect(find.textContaining('Demo mode'), findsNothing);
      expect(find.byKey(const Key('importCard')), findsNothing);
      expect(find.text('把 STL 变成可编辑的 CAD 文件'), findsNothing);
    });

    testWidgets('successful native load shows home, not the error or demo banner', (tester) async {
      final native = FfiEngine.testInstance();
      final engine = createEngine(
        releaseMode: true,
        environment: const {},
        loadNative: () => native,
        createDemo: () => throw StateError('DemoEngine constructed'),
      );
      SharedPreferences.setMockInitialValues({'onboardingSeen': true, 'language': 'en'});
      final prefs = await SharedPreferences.getInstance();
      final dir = Directory.systemTemp.createTempSync('stl2stp_core_ok');
      addTearDown(() => dir.deleteSync(recursive: true));
      final state = AppState(engine: engine, prefs: prefs, outputDir: dir);

      await tester.pumpWidget(Stl2StpApp(state: state));
      await tester.pumpAndSettle();

      expect(state.engine, isA<FfiEngine>());
      expect(find.byKey(const Key('coreLoadError')), findsNothing);
      expect(find.textContaining('Demo mode'), findsNothing);
      expect(find.byKey(const Key('importCard')), findsOneWidget);
    });
  });
}
