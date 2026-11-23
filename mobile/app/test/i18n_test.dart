import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/screens/paywall_screen.dart';
import 'package:stl2stp_app/screens/settings_screen.dart';
import 'package:stl2stp_app/screens/support_screen.dart';
import 'package:stl2stp_app/state/app_state.dart';

final _cjk = RegExp(r'[　-〿㐀-䶿一-鿿＀-￯]');

Map<String, String> _load(String name) =>
    (jsonDecode(File('assets/i18n/$name.json').readAsStringSync()) as Map).cast<String, String>();

void main() {
  setUpAll(() async {
    TestWidgetsFlutterBinding.ensureInitialized();
    await S.preload();
  });

  group('English coverage', () {
    final en = {..._load('en'), ..._load('app_en')};
    final zh = {..._load('zh'), ..._load('app_zh')};

    test('every Chinese key has an English string, and the reverse', () {
      expect(en.keys.toSet(), zh.keys.toSet());
      for (final k in zh.keys) {
        expect(en[k]!.trim(), isNotEmpty, reason: k);
      }
    });

    test('no Chinese left in English strings (language names excepted)', () {
      // The language picker shows each language in its own name.
      const allowed = {'x.language.zh'};
      final bad = [for (final e in en.entries) if (!allowed.contains(e.key) && _cjk.hasMatch(e.value)) e.key];
      expect(bad, isEmpty);
    });

    test('purchase, restore, tip and error strings are all translated', () {
      final keys = en.keys.where((k) => RegExp(r'^(support|purchase|paywall|settings\.purchase|x\.reset_purchase)').hasMatch(k));
      expect(keys.length, greaterThan(20));
      for (final k in keys) {
        expect(_cjk.hasMatch(en[k]!), isFalse, reason: k);
        if (k != 'purchase.limit.title') expect(en[k] != zh[k], isTrue, reason: 'untranslated: $k');
      }
    });

    test('Chinese only appears in Dart code next to an isZh switch', () {
      final offenders = <String>[];
      for (final f in Directory('lib').listSync(recursive: true).whereType<File>().where((f) => f.path.endsWith('.dart'))) {
        final lines = f.readAsLinesSync();
        for (var i = 0; i < lines.length; i++) {
          final l = lines[i].trim();
          if (!_cjk.hasMatch(l) || l.startsWith('//') || l.startsWith('///') || l.startsWith('*')) continue;
          if (!l.contains('isZh')) offenders.add('${f.path}:${i + 1}: $l');
        }
      }
      expect(offenders, isEmpty);
    });
  });

  group('follows the system language', () {
    Future<AppState> state(WidgetTester tester, [Map<String, Object> init = const {}]) async {
      SharedPreferences.setMockInitialValues({'onboardingSeen': true, ...init});
      final p = await SharedPreferences.getInstance();
      final dir = Directory.systemTemp.createTempSync('stl2stp_i18n');
      addTearDown(() => dir.deleteSync(recursive: true));
      tester.view.physicalSize = const Size(1600, 2400);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.reset);
      addTearDown(tester.platformDispatcher.clearAllTestValues);
      return AppState(engine: DemoEngine(), prefs: p, outputDir: dir);
    }

    String settingsTitle(WidgetTester tester) => S.of(tester.element(find.byType(Scaffold).first)).t('settings.title');

    test('default setting is "system"', () async {
      SharedPreferences.setMockInitialValues({});
      final p = await SharedPreferences.getInstance();
      final dir = Directory.systemTemp.createTempSync('stl2stp_i18n');
      addTearDown(() => dir.deleteSync(recursive: true));
      expect(AppState(engine: DemoEngine(), prefs: p, outputDir: dir).language, 'system');
    });

    for (final (tag, locales, expectZh) in [
      ('English device', [const Locale('en', 'US')], false),
      ('Chinese device', [const Locale('zh', 'CN')], true),
      ('Traditional Chinese device', [const Locale.fromSubtags(languageCode: 'zh', scriptCode: 'Hant')], true),
      ('French device falls back to English', [const Locale('fr')], false),
    ]) {
      testWidgets(tag, (tester) async {
        final st = await state(tester);
        tester.platformDispatcher.localesTestValue = locales;
        tester.platformDispatcher.localeTestValue = locales.first;
        await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: const SettingsScreen()));
        await tester.pumpAndSettle();
        expect(settingsTitle(tester), expectZh ? _load('zh')['settings.title'] : _load('en')['settings.title']);
        final texts = tester.widgetList<Text>(find.byType(Text)).map((t) => t.data ?? '').toList();
        final hasCjk = texts.any((t) => t != '中文' && _cjk.hasMatch(t));
        expect(hasCjk, expectZh, reason: 'visible texts: $texts');
      });
    }

    testWidgets('an explicit choice wins over the device language', (tester) async {
      final st = await state(tester, {'language': 'zh'});
      tester.platformDispatcher.localesTestValue = [const Locale('en', 'US')];
      tester.platformDispatcher.localeTestValue = const Locale('en', 'US');
      await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: const SettingsScreen()));
      await tester.pumpAndSettle();
      expect(settingsTitle(tester), _load('zh')['settings.title']);
    });

    testWidgets('English device: support, paywall and home screens have no Chinese', (tester) async {
      final st = await state(tester);
      tester.platformDispatcher.localesTestValue = [const Locale('en', 'US')];
      tester.platformDispatcher.localeTestValue = const Locale('en', 'US');
      for (final screen in [const SupportScreen(), const PaywallScreen(), null]) {
        await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: screen));
        await tester.pumpAndSettle();
        final texts = tester.widgetList<Text>(find.byType(Text)).map((t) => t.data ?? '').toList();
        expect(texts.where(_cjk.hasMatch), isEmpty, reason: '${screen?.runtimeType ?? 'home'}');
      }
    });
  });
}
