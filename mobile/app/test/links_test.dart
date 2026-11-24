import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/core/links.dart';
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/screens/settings_screen.dart';
import 'package:stl2stp_app/state/app_state.dart';

Map<String, String> _load(String n) => (jsonDecode(File('assets/i18n/$n.json').readAsStringSync()) as Map).cast<String, String>();

void main() {
  setUpAll(() async {
    TestWidgetsFlutterBinding.ensureInitialized();
    await S.preload();
  });

  test('tip text no longer says every conversion is free; it names what Pro covers (zh + en)', () {
    final en = _load('app_en')['support.body']!;
    final zh = _load('app_zh')['support.body']!;
    expect(en, isNot(contains('remain free')));
    expect(zh, isNot(contains('所有现有转换功能仍可免费')));
    expect(en, allOf(contains('IGES'), contains('batch'), contains('Pro'), contains('does not renew')));
    expect(zh, allOf(contains('IGES'), contains('批量'), contains('Pro'), contains('不会自动续费')));
  });

  test('links are the published https pages, not placeholders', () {
    expect(privacyPolicyUrl, 'https://bobo7727.github.io/stl2stp-support/privacy.html');
    expect(supportUrl, 'https://bobo7727.github.io/stl2stp-support/');
    expect(Uri.parse(privacyPolicyUrl).scheme, 'https');
    expect(Uri.parse(supportUrl).scheme, 'https');
    expect(linksArePlaceholders, isFalse);
  });

  test('openExternalUrl: https only, launcher result and errors are reported', () async {
    Uri? seen;
    expect(await openExternalUrl('https://example.org/p', launcher: (u) async {
      seen = u;
      return true;
    }), isTrue);
    expect(seen.toString(), 'https://example.org/p');
    expect(await openExternalUrl('http://example.org', launcher: (u) async => true), isFalse);
    expect(await openExternalUrl('javascript:alert(1)', launcher: (u) async => true), isFalse);
    expect(await openExternalUrl('https://example.org', launcher: (u) async => false), isFalse);
    expect(await openExternalUrl('https://example.org', launcher: (u) async => throw StateError('x')), isFalse);
  });

  testWidgets('Settings has privacy policy and support rows; a failed open copies the link', (tester) async {
    await tester.binding.setSurfaceSize(const Size(430, 2400));
    SharedPreferences.setMockInitialValues({'onboardingSeen': true, 'language': 'en'});
    final prefs = await SharedPreferences.getInstance();
    final dir = Directory.systemTemp.createTempSync('stl2stp_links');
    addTearDown(() => dir.deleteSync(recursive: true));
    String? clip;
    tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(SystemChannels.platform, (c) async {
      if (c.method == 'Clipboard.setData') clip = (c.arguments as Map)['text'] as String;
      return null;
    });
    addTearDown(() => tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(SystemChannels.platform, null));
    final st = AppState(engine: DemoEngine(), prefs: prefs, outputDir: dir);
    await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: const SettingsScreen()));
    await tester.pumpAndSettle();
    expect(find.text('Privacy Policy'), findsOneWidget);
    expect(find.text('Support & Feedback'), findsOneWidget);
    await tester.ensureVisible(find.byKey(const Key('settingsPrivacy')));
    await tester.tap(find.byKey(const Key('settingsPrivacy')));
    for (var i = 0; i < 5; i++) {
      await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 50)));
      await tester.pump(const Duration(milliseconds: 100));
    }
    // No browser in the test host (and the placeholder never resolves): the link is copied instead.
    expect(clip, privacyPolicyUrl);
    expect(find.textContaining('Could not open the browser'), findsOneWidget);
  });
}
