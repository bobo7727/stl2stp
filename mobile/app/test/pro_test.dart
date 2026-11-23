import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/core/models.dart';
import 'package:stl2stp_app/main.dart' show limitsFor;
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/purchase/ios_store_service.dart';
import 'package:stl2stp_app/purchase/purchase.dart';
import 'package:stl2stp_app/screens/paywall_screen.dart';
import 'package:stl2stp_app/state/app_state.dart';

import 'tip_test.dart' show FakeNative;

Future<SharedPreferences> _prefs([Map<String, Object> init = const {}]) async {
  SharedPreferences.setMockInitialValues({'onboardingSeen': true, 'language': 'en', ...init});
  return SharedPreferences.getInstance();
}

const _iges = ConvertSettings(format: ExportFormat.iges);

void main() {
  setUpAll(() async {
    TestWidgetsFlutterBinding.ensureInitialized();
    await S.preload();
  });

  group('free tier (release limits)', () {
    const l = FreeTierLimits.release;
    test('50,000 faces is free, 50,001 is Pro; reducing first is free', () {
      expect(l.check(triangles: 50000, settings: const ConvertSettings(), conversionsToday: 0), isNull);
      expect(l.check(triangles: 50001, settings: const ConvertSettings(), conversionsToday: 0)!.kind, LimitKind.triangles);
      expect(l.check(triangles: 100000, settings: const ConvertSettings(simplify: true, keepPercent: 50), conversionsToday: 0),
          isNull);
    });
    test('IGES is Pro, STEP and metadata are free, no daily limit', () {
      expect(l.check(triangles: 10, settings: _iges, conversionsToday: 0)!.kind, LimitKind.iges);
      expect(l.check(triangles: 10, settings: const ConvertSettings(author: 'a', product: 'p'), conversionsToday: 0), isNull);
      expect(l.check(triangles: 10, settings: const ConvertSettings(), conversionsToday: 9999), isNull);
    });
    test('batch is Pro; none() and Pro users are unrestricted', () async {
      expect(l.checkBatch()!.kind, LimitKind.batch);
      expect(FreeTierLimits.none.checkBatch(), isNull);
      final p = await _prefs();
      final dir = Directory.systemTemp.createTempSync('stl2stp_pro');
      addTearDown(() => dir.deleteSync(recursive: true));
      final m = MockPurchaseService(p, delay: Duration.zero);
      final st = AppState(engine: DemoEngine(), prefs: p, outputDir: dir, purchase: m, limits: l);
      expect(st.batchLimit()!.kind, LimitKind.batch);
      expect(st.limitFor(10, _iges)!.kind, LimitKind.iges);
      await m.buyPro();
      expect(st.batchLimit(), isNull);
      expect(st.limitFor(900000, _iges), isNull);
    });
    test('non-iOS (mock) gets no limits', () async {
      final p = await _prefs();
      expect(limitsFor(MockPurchaseService(p)), same(FreeTierLimits.none));
      expect(limitsFor(IosStoreService(p)), same(FreeTierLimits.release));
    });
    test('free reduce percentage: multiple of 5, null when impossible', () {
      expect(freeReducePercent(40000), isNull);
      expect(freeReducePercent(100000), 50);
      expect(freeReducePercent(60000), 80);
      expect(freeReducePercent(1000000), 5);
      expect(freeReducePercent(1000001), isNull);
    });
  });

  group('IosStoreService Pro', () {
    late FakeNative native;
    setUp(() => native = FakeNative());
    tearDown(() => native.dispose());
    Future<void> settle() async {
      await Future<void>.delayed(Duration.zero);
      await Future<void>.delayed(Duration.zero);
    }

    test('buy: native event unlocks and the result is success', () async {
      final s = IosStoreService(await _prefs());
      await s.init();
      expect(s.isPro, isFalse);
      native.events!.success({'type': 'pro', 'pro': true});
      final r = await s.buyPro();
      expect(r.outcome, PurchaseOutcome.success);
      await settle();
      expect(s.isPro, isTrue);
      s.dispose();
    });

    test('buy outcomes map to results and never unlock on their own', () async {
      final s = IosStoreService(await _prefs());
      await s.init();
      for (final (reply, outcome) in [
        ('cancelled', PurchaseOutcome.cancelled),
        ('pending', PurchaseOutcome.pending),
        ('unverified', PurchaseOutcome.unverified),
        ('busy', PurchaseOutcome.busy),
        ('unavailable', PurchaseOutcome.unavailable),
        ('x', PurchaseOutcome.failed),
      ]) {
        native.buyProReply = {'outcome': reply};
        expect((await s.buyPro()).outcome, outcome);
        expect(s.isPro, isFalse);
      }
      s.dispose();
    });

    test('no Pro product from the store: unavailable, no purchase call', () async {
      native.productsReply = {'canPay': true, 'tips': []};
      final s = IosStoreService(await _prefs());
      await s.init();
      expect((await s.buyPro()).outcome, PurchaseOutcome.unavailable);
      expect(native.calls.where((c) => c.method == 'buyPro'), isEmpty);
      s.dispose();
    });

    test('tip and Pro purchases share one lock (never two sheets)', () async {
      final s = IosStoreService(await _prefs());
      await s.init();
      native.buyGate = Completer();
      final first = s.buyPro();
      expect((await s.buyTip(tipSmallProductId)).outcome, TipOutcome.busy);
      expect((await s.buyPro()).outcome, PurchaseOutcome.busy);
      native.buyGate!.complete();
      expect((await first).outcome, PurchaseOutcome.success);
      s.dispose();
    });

    test('cold start: cache true is kept while native says true, replaced when it says false', () async {
      native.startReply = {'type': 'pro', 'pro': true};
      var s = IosStoreService(await _prefs({'store.proActive': true}));
      await s.init();
      await settle();
      expect(s.isPro, isTrue);
      s.dispose();
      // Refunded while the app was closed.
      native.startReply = {'type': 'pro', 'pro': false};
      final p = await _prefs({'store.proActive': true});
      s = IosStoreService(p);
      await s.init();
      await settle();
      expect(s.isPro, isFalse);
      expect(p.getBool('store.proActive'), isFalse);
      s.dispose();
    });

    test('offline / bridge failure at start keeps the cache', () async {
      native.dispose();
      final s = IosStoreService(await _prefs({'store.proActive': true}));
      await s.init();
      expect(s.available, isFalse);
      expect(s.isPro, isTrue);
      s.dispose();
      native = FakeNative();
    });

    test('revocation event turns Pro off; a stale true cannot come back after it', () async {
      final s = IosStoreService(await _prefs({'store.proActive': true}));
      native.startReply = {'type': 'pro', 'pro': true};
      await s.init();
      native.events!.success({'type': 'pro', 'pro': false});
      await settle();
      expect(s.isPro, isFalse);
      s.dispose();
    });

    test('restore: success sets Pro; empty after a successful sync clears the cache; failure leaves it', () async {
      final p = await _prefs({'store.proActive': true});
      native.startReply = {'type': 'pro', 'pro': true};
      final s = IosStoreService(p);
      await s.init();
      await settle();

      native.restoreReply = {'outcome': 'failed', 'message': 'sign-in cancelled'};
      var r = await s.restore();
      expect(r.outcome, PurchaseOutcome.failed);
      expect(r.message, 'sign-in cancelled');
      expect(s.isPro, isTrue, reason: 'a failed sync must not touch the cache');

      native.restoreReply = {'outcome': 'nothingToRestore', 'pro': false};
      r = await s.restore();
      expect(r.outcome, PurchaseOutcome.nothingToRestore);
      expect(s.isPro, isFalse, reason: 'Apple answered: no purchase');
      expect(p.getBool('store.proActive'), isFalse);

      native.restoreReply = {'outcome': 'restored', 'pro': true};
      r = await s.restore();
      expect(r.outcome, PurchaseOutcome.restored);
      expect(s.isPro, isTrue);
      s.dispose();
    });
  });

  group('paywall contract (ensureAllowed)', () {
    Future<(AppState, List<ConvertSettings?>)> pumpHost(WidgetTester tester, int faces, ConvertSettings settings,
        {bool pro = false}) async {
      final p = await _prefs(pro ? {'mock.proOwned': true, 'mock.proActive': true} : {});
      final dir = Directory.systemTemp.createTempSync('stl2stp_pay');
      addTearDown(() => dir.deleteSync(recursive: true));
      tester.view.physicalSize = const Size(900, 2400);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.reset);
      final st = AppState(
          engine: DemoEngine(),
          prefs: p,
          outputDir: dir,
          purchase: MockPurchaseService(p, priceLabel: r'$4.99', delay: Duration.zero),
          limits: FreeTierLimits.release);
      final results = <ConvertSettings?>[];
      await tester.pumpWidget(Stl2StpApp(
          state: st,
          homeOverride: Scaffold(
              body: Builder(
                  builder: (c) => TextButton(
                      onPressed: () async => results.add(await ensureAllowed(c, faces, settings)),
                      child: const Text('go'))))));
      return (st, results);
    }

    testWidgets('allowed conversions never see the paywall', (tester) async {
      final (_, results) = await pumpHost(tester, 1000, const ConvertSettings());
      await tester.tap(find.text('go'));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('proTitle')), findsNothing);
      expect(results.single, const ConvertSettings());
    });

    testWidgets('IGES: reason, three features incl. batch, price; "use STEP" is re-checked and returned', (tester) async {
      final (_, results) = await pumpHost(tester, 1000, _iges);
      await tester.tap(find.text('go'));
      await tester.pumpAndSettle();
      expect(find.text('IGES export is part of Pro.'), findsOneWidget);
      expect(find.text('Unlock IGES, batch and large files'), findsOneWidget);
      expect(find.text('Convert many files at once'), findsOneWidget);
      expect(find.text(r'Unlock Pro — $4.99'), findsOneWidget);
      expect(find.byKey(const Key('restoreButton')), findsOneWidget);
      expect(find.byKey(const Key('freeOption')), findsOneWidget);
      await tester.tap(find.byKey(const Key('freeOption')));
      await tester.pumpAndSettle();
      expect(results.single!.format, ExportFormat.step);
    });

    testWidgets('100,000 faces + IGES: only "reduce and export STEP", never free IGES', (tester) async {
      final (st, results) = await pumpHost(tester, 100000, _iges);
      await tester.tap(find.text('go'));
      await tester.pumpAndSettle();
      expect(find.text('Reduce to 50% and export STEP free'), findsOneWidget);
      await tester.tap(find.byKey(const Key('freeOption')));
      await tester.pumpAndSettle();
      final r = results.single!;
      expect((r.format, r.simplify, r.keepPercent), (ExportFormat.step, true, 50));
      expect(st.limitFor(100000, r), isNull, reason: 'the returned settings pass every limit');
    });

    testWidgets('large file, STEP: reduce-and-convert offered', (tester) async {
      final (_, results) = await pumpHost(tester, 60000, const ConvertSettings());
      await tester.tap(find.text('go'));
      await tester.pumpAndSettle();
      expect(find.text('Reduce to 80% and convert free'), findsOneWidget);
      expect(find.textContaining('This model has 60,000 faces'), findsOneWidget);
      await tester.tap(find.byKey(const Key('freeOption')));
      await tester.pumpAndSettle();
      expect(results.single!.keepPercent, 80);
    });

    testWidgets('over 1,000,000 faces: no free promise, buy or leave', (tester) async {
      await pumpHost(tester, 1000001, const ConvertSettings());
      await tester.tap(find.text('go'));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('freeOption')), findsNothing);
      expect(find.byKey(const Key('buyButton')), findsOneWidget);
    });

    testWidgets('"Not now" changes nothing', (tester) async {
      final (_, results) = await pumpHost(tester, 1000, _iges);
      await tester.tap(find.text('go'));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('laterButton')));
      await tester.pumpAndSettle();
      expect(results.single, isNull);
    });

    testWidgets('buying Pro returns the ORIGINAL settings after a fresh check', (tester) async {
      final (st, results) = await pumpHost(tester, 100000, _iges);
      await tester.tap(find.text('go'));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('buyButton')));
      await tester.pumpAndSettle();
      expect(st.isPro, isTrue);
      expect(results.single, _iges);
    });

    testWidgets('a Pro user is never walled', (tester) async {
      final (_, results) = await pumpHost(tester, 900000, _iges, pro: true);
      await tester.tap(find.text('go'));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('proTitle')), findsNothing);
      expect(results.single, _iges);
    });
  });
}
