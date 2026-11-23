import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/purchase/ios_store_service.dart';
import 'package:stl2stp_app/purchase/purchase.dart';
import 'package:stl2stp_app/screens/settings_screen.dart';
import 'package:stl2stp_app/screens/support_screen.dart';
import 'package:stl2stp_app/state/app_state.dart';

/// Stands in for ios/Runner/StoreChannel.swift.
class FakeNative {
  FakeNative() {
    final m = TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
    m.setMockMethodCallHandler(const MethodChannel('stl2stp/store'), _call);
    m.setMockStreamHandler(
        const EventChannel('stl2stp/store/events'),
        MockStreamHandler.inline(onListen: (_, sink) {
          events = sink;
        }));
  }

  MockStreamHandlerEventSink? events;
  final calls = <MethodCall>[];
  Map<String, Object?> startReply = {'type': 'pro', 'pro': false};
  Map<String, Object?> productsReply = {
    'canPay': true,
    'tips': [
      {'id': tipSmallProductId, 'title': 'Small Tip', 'description': 'd', 'price': r'$0.99'}
    ],
    'pro': {'id': 'com.stl2stp.app.pro', 'title': 'STL2STP Pro', 'description': 'd', 'price': r'$4.99'},
  };
  Map<String, Object?> buyProReply = {'outcome': 'success'};
  Map<String, Object?> buyReply = {'outcome': 'success'};
  Map<String, Object?> restoreReply = {'outcome': 'nothingToRestore'};
  Completer<void>? buyGate;
  Object? failProducts;

  Future<Object?> _call(MethodCall c) async {
    calls.add(c);
    switch (c.method) {
      case 'start':
        return startReply;
      case 'loadProducts':
        if (failProducts != null) throw PlatformException(code: 'x');
        return productsReply;
      case 'buyTip':
        await buyGate?.future;
        return buyReply;
      case 'buyPro':
        await buyGate?.future;
        return buyProReply;
      case 'restore':
        return restoreReply;
      case 'refresh':
        return startReply;
    }
    return null;
  }

  void dispose() {
    final m = TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
    m.setMockMethodCallHandler(const MethodChannel('stl2stp/store'), null);
    m.setMockStreamHandler(const EventChannel('stl2stp/store/events'), null);
  }
}

Future<SharedPreferences> prefs([Map<String, Object> init = const {}]) async {
  SharedPreferences.setMockInitialValues({'onboardingSeen': true, 'language': 'en', ...init});
  return SharedPreferences.getInstance();
}

void main() {
  setUpAll(() async {
    TestWidgetsFlutterBinding.ensureInitialized();
    await S.preload();
  });

  group('IosStoreService', () {
    late FakeNative native;
    setUp(() => native = FakeNative());
    tearDown(() => native.dispose());

    test('product id and iOS-only policy', () {
      expect(tipSmallProductId, 'com.stl2stp.app.tip.small');
      expect(const NoTipService().supported, isFalse);
    });

    test('init starts native, loads tip and Pro price; native answer replaces the cache', () async {
      final p = await prefs({'store.proActive': true});
      final s = IosStoreService(p);
      await s.init();
      expect(native.calls.map((c) => c.method), contains('start'));
      expect(s.proForSale, isTrue);
      expect(s.product!.priceLabel, r'$4.99');
      expect(s.products.single.priceLabel, r'$0.99');
      await s.restore(); // flush any queued mirror writes
      // Native said "not Pro" at start (e.g. refunded while the app was closed): cache turns off.
      expect(s.isPro, isFalse);
      s.dispose();
    });

    test('missing / failing products are unavailable, not ready', () async {
      native.productsReply = {'canPay': true, 'tips': []};
      var s = IosStoreService(await prefs());
      await s.init();
      expect(s.loadState, TipLoad.unavailable);
      native.productsReply = {
        'canPay': false,
        'tips': [
          {'id': tipSmallProductId, 'title': 't', 'description': 'd', 'price': r'$0.99'}
        ]
      };
      await s.loadProducts();
      expect(s.loadState, TipLoad.unavailable, reason: 'payments restricted');
      native.failProducts = true;
      await s.loadProducts();
      expect(s.loadState, TipLoad.unavailable);
      s.dispose();
    });

    test('a tip never changes Pro', () async {
      final s = IosStoreService(await prefs());
      await s.init();
      for (final (reply, outcome) in [
        ('success', TipOutcome.thanks),
        ('cancelled', TipOutcome.cancelled),
        ('pending', TipOutcome.pending),
        ('unverified', TipOutcome.unverified),
        ('unavailable', TipOutcome.unavailable),
        ('weird', TipOutcome.failed),
      ]) {
        native.buyReply = {'outcome': reply};
        expect((await s.buyTip(tipSmallProductId)).outcome, outcome);
        expect(s.isPro, isFalse);
      }
      s.dispose();
    });

    test('second buy while one is open is busy (no double sheet)', () async {
      final s = IosStoreService(await prefs());
      await s.init();
      native.buyGate = Completer();
      final first = s.buyTip(tipSmallProductId);
      expect((await s.buyTip(tipSmallProductId)).outcome, TipOutcome.busy);
      native.buyGate!.complete();
      expect((await first).outcome, TipOutcome.thanks);
      expect(native.calls.where((c) => c.method == 'buyTip').length, 1);
      s.dispose();
    });

    test('background thanks / unverified are surfaced', () async {
      final s = IosStoreService(await prefs());
      await s.init();
      final got = <TipOutcome>[];
      final sub = s.background.listen(got.add);
      native.events!.success({'type': 'tip', 'status': 'thanks'});
      native.events!.success({'type': 'tip', 'status': 'unverified'});
      await Future<void>.delayed(Duration.zero);
      await Future<void>.delayed(Duration.zero);
      expect(got, [TipOutcome.thanks, TipOutcome.unverified]);
      expect(s.isPro, isFalse);
      await sub.cancel();
      s.dispose();
    });

    test('store failure at start never throws out of init', () async {
      native.dispose();
      final s = IosStoreService(await prefs());
      await s.init();
      expect(s.available, isFalse);
      expect(s.loadState, TipLoad.unavailable);
      s.dispose();
      native = FakeNative();
    });
  });

  group('screens', () {
    late FakeNative native;
    setUp(() => native = FakeNative());
    tearDown(() => native.dispose());

    Future<AppState> state(WidgetTester tester, PurchaseService Function(SharedPreferences) make,
        [Map<String, Object> init = const {}]) async {
      final p = await prefs(init);
      final dir = Directory.systemTemp.createTempSync('stl2stp_tip');
      addTearDown(() => dir.deleteSync(recursive: true));
      tester.view.physicalSize = const Size(1600, 2400);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.reset);
      return AppState(engine: DemoEngine(), prefs: p, outputDir: dir, purchase: make(p));
    }

    testWidgets('support screen: store price, thanks, Pro and limits unchanged', (tester) async {
      final st = await state(tester, IosStoreService.new);
      addTearDown(st.purchase.dispose);
      await tester.runAsync(() => st.purchase.init());
      await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: const SupportScreen()));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('supportBody')), findsOneWidget);
      expect(find.text(r'Tip $0.99'), findsOneWidget);
      await tester.tap(find.byKey(Key('tipButton.$tipSmallProductId')));
      await tester.pumpAndSettle();
      expect(find.text('Thank you for your support! Features and limits are unchanged.'), findsOneWidget);
      expect(st.isPro, isFalse);
      expect(st.limits.isNone, isTrue);
      // Repeatable.
      expect(find.byKey(Key('tipButton.$tipSmallProductId')), findsOneWidget);
    });

    testWidgets('support screen: pending, unverified, cancel messages; unavailable has retry', (tester) async {
      final st = await state(tester, IosStoreService.new);
      addTearDown(st.purchase.dispose);
      await tester.runAsync(() => st.purchase.init());
      await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: const SupportScreen()));
      await tester.pumpAndSettle();
      native.buyReply = {'outcome': 'pending'};
      await tester.tap(find.byKey(Key('tipButton.$tipSmallProductId')));
      await tester.pumpAndSettle();
      expect(find.textContaining('pending approval or processing'), findsOneWidget);
      await tester.pump(const Duration(seconds: 6)); // let the first snack bar leave
      await tester.pumpAndSettle();
      native.buyReply = {'outcome': 'unverified'};
      await tester.tap(find.byKey(Key('tipButton.$tipSmallProductId')));
      await tester.pumpAndSettle();
      expect(find.textContaining('no need to pay again'), findsOneWidget);
      expect(st.isPro, isFalse);

      native.productsReply = {'canPay': true, 'tips': []};
      await tester.runAsync(() => st.tips.loadProducts());
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('tipUnavailable')), findsOneWidget);
      expect(find.byKey(const Key('tipRetry')), findsOneWidget);
      expect(find.textContaining('Tip \$'), findsNothing);
    });

    testWidgets('settings on iOS: tip, Pro status and restore', (tester) async {
      final st = await state(tester, IosStoreService.new);
      addTearDown(st.purchase.dispose);
      await tester.runAsync(() => st.purchase.init());
      await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: const SettingsScreen()));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('supportRow')), findsOneWidget);
      expect(find.byKey(const Key('purchaseStatus')), findsOneWidget);
      expect(find.byKey(const Key('settingsRestore')), findsOneWidget);
    });

    testWidgets('settings off iOS (mock): no tip entry, Pro row as before', (tester) async {
      final st = await state(tester, (p) => MockPurchaseService(p, delay: Duration.zero));
      await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: const SettingsScreen()));
      await tester.pumpAndSettle();
      expect(st.tips.supported, isFalse);
      expect(find.byKey(const Key('supportRow')), findsNothing);
      expect(find.byKey(const Key('purchaseStatus')), findsOneWidget);
      expect(find.byKey(const Key('settingsRestore')), findsOneWidget);
    });
  });
}
