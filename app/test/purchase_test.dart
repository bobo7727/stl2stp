import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/core/models.dart';
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/purchase/purchase.dart';
import 'package:stl2stp_app/screens/paywall_screen.dart';
import 'package:stl2stp_app/state/app_state.dart';

Future<SharedPreferences> prefs([Map<String, Object> init = const {}]) async {
  SharedPreferences.setMockInitialValues({'onboardingSeen': true, ...init});
  return SharedPreferences.getInstance();
}

void main() {
  setUpAll(() async {
    TestWidgetsFlutterBinding.ensureInitialized();
    await S.preload();
  });

  group('free tier', () {
    test('default limits are none and never block', () {
      expect(FreeTierLimits.none.isNone, isTrue);
      expect(
          FreeTierLimits.none.check(
              triangles: 5000000, settings: const ConvertSettings(format: ExportFormat.iges, author: 'x'), conversionsToday: 999),
          isNull);
    });

    test('each limit triggers', () {
      const l = FreeTierLimits(maxTriangles: 1000, dailyConversions: 3, igesRequiresPro: true, metaRequiresPro: true);
      expect(l.check(triangles: 2000, settings: const ConvertSettings(), conversionsToday: 0)!.kind, LimitKind.triangles);
      // Simplifying below the limit is allowed.
      expect(l.check(triangles: 2000, settings: const ConvertSettings(simplify: true, keepPercent: 40), conversionsToday: 0),
          isNull);
      expect(l.check(triangles: 10, settings: const ConvertSettings(), conversionsToday: 3)!.kind, LimitKind.daily);
      expect(l.check(triangles: 10, settings: const ConvertSettings(format: ExportFormat.iges), conversionsToday: 0)!.kind,
          LimitKind.iges);
      expect(l.check(triangles: 10, settings: const ConvertSettings(product: 'p'), conversionsToday: 0)!.messageKey,
          'purchase.limit.meta');
    });
  });

  group('mock purchase service', () {
    test('buy, persist, reset, restore', () async {
      final p = await prefs();
      final m = MockPurchaseService(p, delay: Duration.zero);
      await m.init();
      expect(m.product!.priceLabel, isNull, reason: 'no hard-coded price');
      expect(m.isPro, isFalse);
      expect((await m.restore()).outcome, PurchaseOutcome.nothingToRestore);
      expect((await m.buyPro()).outcome, PurchaseOutcome.success);
      expect(MockPurchaseService(p).isPro, isTrue, reason: 'persisted');
      await m.reset();
      expect(m.isPro, isFalse);
      expect((await m.restore()).outcome, PurchaseOutcome.restored);
      expect(m.isPro, isTrue);
      await m.reset(forgetOwned: true);
      expect((await m.restore()).outcome, PurchaseOutcome.nothingToRestore);
    });

    test('simulated failures do not unlock', () async {
      final m = MockPurchaseService(await prefs(), delay: Duration.zero);
      for (final o in [PurchaseOutcome.cancelled, PurchaseOutcome.failed, PurchaseOutcome.unavailable]) {
        m.simulate = o;
        final r = await m.buyPro();
        expect(r.outcome, o);
        expect(m.isPro, isFalse);
      }
    });

    test('AppState: Pro bypasses limits; daily counter', () async {
      final p = await prefs();
      final dir = Directory.systemTemp.createTempSync('stl2stp_purchase');
      addTearDown(() => dir.deleteSync(recursive: true));
      final m = MockPurchaseService(p, delay: Duration.zero);
      final st = AppState(engine: DemoEngine(), prefs: p, outputDir: dir, purchase: m,
          limits: const FreeTierLimits(dailyConversions: 1));
      expect(st.limitFor(10, const ConvertSettings()), isNull);
      st.recordConversion();
      expect(st.conversionsToday, 1);
      expect(st.limitFor(10, const ConvertSettings())!.kind, LimitKind.daily);
      await m.buyPro();
      expect(st.limitFor(10, const ConvertSettings()), isNull);
    });
  });

  group('paywall', () {
    Future<AppState> state(WidgetTester tester, {String? price, FreeTierLimits limits = FreeTierLimits.none}) async {
      final p = await prefs({'language': 'en'});
      final dir = Directory.systemTemp.createTempSync('stl2stp_paywall');
      addTearDown(() => dir.deleteSync(recursive: true));
      final m = MockPurchaseService(p, priceLabel: price, delay: Duration.zero);
      tester.view.physicalSize = const Size(430, 1400);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.reset);
      return AppState(engine: DemoEngine(), prefs: p, outputDir: dir, purchase: m, limits: limits);
    }

    testWidgets('placeholder price when the store reports none, then buy', (tester) async {
      final st = await state(tester);
      // Host page so the paywall can pop back after unlocking.
      await tester.pumpWidget(Stl2StpApp(
          state: st,
          homeOverride: Scaffold(
              body: Builder(
                  builder: (c) => TextButton(
                      onPressed: () => Navigator.push(c, MaterialPageRoute(builder: (_) => const PaywallScreen())),
                      child: const Text('open'))))));
      await tester.tap(find.text('open'));
      await tester.pumpAndSettle();
      expect(find.text('Buy (price TBD)'), findsOneWidget);
      expect(find.text('Development mock store: no charge'), findsOneWidget);
      await tester.tap(find.byKey(const Key('buyButton')));
      await tester.pumpAndSettle();
      expect(st.isPro, isTrue);
      expect(find.text('Pro unlocked. Thank you.'), findsOneWidget);
      expect(find.byType(PaywallScreen), findsNothing);
    });

    testWidgets('price label comes from product info', (tester) async {
      final st = await state(tester, price: 'STORE-PRICE');
      await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: const PaywallScreen()));
      await tester.pumpAndSettle();
      expect(find.text('Buy for STORE-PRICE'), findsOneWidget);
      await tester.tap(find.byKey(const Key('restoreButton')));
      await tester.pumpAndSettle();
      expect(find.text('No purchases to restore'), findsOneWidget);
    });
  });
}
