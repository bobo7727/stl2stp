import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/purchase/ios_store_service.dart';
import 'package:stl2stp_app/purchase/mock_purchase_service.dart';
import 'package:stl2stp_app/state/app_state.dart';

import 'tip_test.dart' show FakeNative, prefs;

class _CountingStore extends MockPurchaseService {
  _CountingStore(super.prefs);
  int refreshes = 0;
  @override
  Future<void> refresh() async => refreshes++;
}

void main() {
  setUpAll(() async {
    TestWidgetsFlutterBinding.ensureInitialized();
    await S.preload();
  });

  group('foreground entitlement refresh (C4)', () {
    late FakeNative native;
    setUp(() => native = FakeNative());
    tearDown(() => native.dispose());

    test('refresh asks native (no restore/sync) and applies a revoked entitlement', () async {
      final s = IosStoreService(await prefs({'store.proActive': true}));
      await s.init();
      native.startReply = {'type': 'pro', 'pro': true};
      await s.refresh();
      expect(s.isPro, isTrue);
      native.calls.clear();
      native.startReply = {'type': 'pro', 'pro': false}; // refunded while away
      await s.refresh();
      expect(s.isPro, isFalse);
      expect(native.calls.map((c) => c.method), ['refresh']);
      s.dispose();
    });

    test('a failing refresh leaves the cache alone', () async {
      final p = await prefs({'store.proActive': true});
      final s = IosStoreService(p);
      await s.init();
      native.startReply = {'type': 'pro', 'pro': true};
      await s.refresh();
      final m = TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
      m.setMockMethodCallHandler(const MethodChannel('stl2stp/store'), (c) async => throw PlatformException(code: 'x'));
      await s.refresh();
      expect(s.isPro, isTrue);
      s.dispose();
    });
  });

  testWidgets('returning to the foreground refreshes the entitlement', (tester) async {
    SharedPreferences.setMockInitialValues({'onboardingSeen': true, 'language': 'en'});
    final p = await SharedPreferences.getInstance();
    final dir = Directory.systemTemp.createTempSync('stl2stp_rf');
    addTearDown(() => dir.deleteSync(recursive: true));
    final store = _CountingStore(p);
    final st = AppState(engine: DemoEngine(), prefs: p, outputDir: dir, purchase: store);
    await tester.pumpWidget(Stl2StpApp(state: st));
    await tester.pump();
    expect(store.refreshes, 0);
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.inactive);
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.hidden);
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.paused);
    expect(store.refreshes, 0);
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.hidden);
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.inactive);
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
    expect(store.refreshes, 1);
  });
}
