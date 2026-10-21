import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:stl2stp_app/core/estimate.dart';
import 'package:stl2stp_app/core/models.dart';
import 'package:stl2stp_app/core/sample_mesh.dart';
import 'package:stl2stp_app/core/stl_reader.dart';
import 'package:stl2stp_app/l10n/strings.dart';

void main() {
  group('presets', () {
    test('defaults are the recommended preset (original mode, verify on)', () {
      const s = ConvertSettings();
      expect(s.preset, Preset.recommended);
      expect(s.mode, ConvertMode.original);
      expect(s.verify, isTrue);
      expect(s.mergeCoplanar, isFalse);
      expect(s.simplify, isFalse);
    });

    test('each preset round-trips and edits become custom', () {
      for (final p in Preset.values) {
        expect(ConvertSettings.forPreset(p).preset, p);
      }
      final custom = ConvertSettings.forPreset(Preset.smaller).copyWith(keepPercent: 30);
      expect(custom.preset, isNull);
      expect(const ConvertSettings().copyWith(mergeCoplanar: true).preset, isNull);
    });

    test('withPreset keeps format and metadata', () {
      const s = ConvertSettings(format: ExportFormat.iges, author: 'Studio');
      final r = s.withPreset(Preset.strict);
      expect(r.format, ExportFormat.iges);
      expect(r.author, 'Studio');
      expect(r.mode, ConvertMode.sewing);
    });

    test('verify only applies to STEP; IGES strips non-ASCII', () {
      expect(const ConvertSettings(format: ExportFormat.iges).verifyEffective, isFalse);
      expect(const ConvertSettings(author: '致知屋').hasNonAsciiMeta, isTrue);
      expect(stripNonAscii('致知屋 Studio'), 'Studio');
    });

    test('json round trip', () {
      const s = ConvertSettings(
          format: ExportFormat.iges, simplify: true, keepPercent: 35, mode: ConvertMode.sewing, product: 'P');
      final r = ConvertSettings.fromJson(jsonDecode(jsonEncode(s.toJson())) as Map<String, Object?>);
      expect(r.toJson(), s.toJson());
    });

    test('target faces', () {
      expect(const ConvertSettings(keepPercent: 50).targetFaces(116), 58);
      expect(const ConvertSettings(keepPercent: 5).targetFaces(10), 1);
    });
  });

  group('progress', () {
    test('stage weights combine to 0..1', () {
      final req = ConvertRequest(inputPath: 'a', outputPath: 'b', settings: const ConvertSettings(), faces: 1);
      expect(req.stages, [Stage.read, Stage.build, Stage.write, Stage.verify]);
      expect(overallProgress(req.stages, Stage.read, 0), 0);
      expect(overallProgress(req.stages, Stage.build, 0), closeTo(10 / 90, 1e-9));
      expect(overallProgress(req.stages, Stage.verify, 1), 1);
      final iges = ConvertRequest(
          inputPath: 'a',
          outputPath: 'b',
          settings: const ConvertSettings(format: ExportFormat.iges, simplify: true),
          faces: 1);
      expect(iges.stages, [Stage.read, Stage.simplify, Stage.build, Stage.write]);
    });
  });

  group('stl reader', () {
    test('binary sample bracket is closed and sized', () {
      final bytes = buildBracketStl();
      final m = parseStlBytes(bytes);
      expect(m.isAscii, isFalse);
      expect(m.totalTriangles, 28);
      final b = m.bounds();
      expect(b.max[0] - b.min[0], 60);
      expect(b.max[1] - b.min[1], 40);
      expect(b.max[2] - b.min[2], 50);
      // Every undirected edge is used exactly twice (watertight, no T-junctions).
      final edges = <String, int>{};
      for (var t = 0; t < m.triangleCount; t++) {
        final v = [for (var k = 0; k < 3; k++) m.positions.sublist(t * 9 + k * 3, t * 9 + k * 3 + 3).join(',')];
        for (var e = 0; e < 3; e++) {
          final a = v[e], c = v[(e + 1) % 3];
          final key = a.compareTo(c) < 0 ? '$a|$c' : '$c|$a';
          edges[key] = (edges[key] ?? 0) + 1;
        }
      }
      expect(edges.values.every((n) => n == 2), isTrue);
    });

    test('ascii parse and binary file starting with "solid"', () {
      const text = 'solid t\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid t\n';
      final m = parseStlBytes(Uint8List.fromList(text.codeUnits));
      expect(m.isAscii, isTrue);
      expect(m.totalTriangles, 1);
      final bin = buildBracketStl();
      bin.setRange(0, 5, 'solid'.codeUnits);
      expect(looksAscii(bin), isFalse);
    });

    test('garbage is rejected', () {
      expect(() => parseStlBytes(Uint8List.fromList(List.filled(10, 1))), throwsA(isA<StlFormatException>()));
      expect(() => parseStlBytes(Uint8List.fromList('solid x\nendsolid x\n'.codeUnits)),
          throwsA(isA<StlFormatException>()));
    });

    test('display decimation keeps total count', () {
      final m = parseStlBytes(buildSphereStl(seg: 64, rings: 32), maxDisplayTriangles: 500);
      expect(m.totalTriangles, 2 * 64 * 31);
      expect(m.triangleCount, lessThanOrEqualTo(500));
    });
  });

  group('format helpers', () {
    test('bytes and counts', () {
      expect(formatBytes(512), '512 B');
      expect(formatBytes(272 * 1024), '272 KB');
      expect(formatBytes(48 * 1024 * 1024), '48.0 MB');
      expect(formatCount(1234567), '1,234,567');
      expect(formatDim(60), '60.0'.replaceAll('.0', ''));
      expect(formatDim(2.5), '2.5');
    });

    test('estimate scales with options', () {
      final base = estimate(20000, const ConvertSettings());
      expect(estimate(20000, const ConvertSettings(mode: ConvertMode.sewing)).seconds, greaterThan(base.seconds));
      expect(estimate(20000, const ConvertSettings(simplify: true, keepPercent: 50)).bytes, lessThan(base.bytes));
    });
  });

  group('strings', () {
    Map<String, String> load(String code) =>
        (jsonDecode(File('assets/i18n/$code.json').readAsStringSync()) as Map).cast<String, String>();

    test('zh and en have the same 200 keys and placeholders', () {
      final zh = load('zh'), en = load('en');
      expect(zh.length, 200);
      expect(zh.keys.toSet(), en.keys.toSet());
      final ph = RegExp(r'\{(\w+)\}');
      for (final k in zh.keys) {
        expect(ph.allMatches(zh[k]!).map((m) => m[1]).toSet(), ph.allMatches(en[k]!).map((m) => m[1]).toSet(),
            reason: k);
      }
      expect(extraStringsForTest['zh']!.keys.toSet(), extraStringsForTest['en']!.keys.toSet());
    });

    test('assets match design/ui string tables', () {
      for (final code in ['zh', 'en']) {
        final design = File('../design/ui/strings_$code.json');
        if (!design.existsSync()) return;
        expect(load(code), (jsonDecode(design.readAsStringSync()) as Map).cast<String, String>(),
            reason: 'run tool/sync_strings.sh');
      }
    });

    test('every literal key used in lib/ exists', () {
      final zh = load('zh');
      final used = <String>{};
      final re = RegExp(r"""\.t\('([a-z0-9_.]+)'""");
      for (final f in Directory('lib').listSync(recursive: true).whereType<File>()) {
        for (final m in re.allMatches(f.readAsStringSync())) {
          used.add(m[1]!);
        }
      }
      final missing = used.where((k) => !zh.containsKey(k) && !extraStringsForTest['zh']!.containsKey(k)).toList();
      expect(missing, isEmpty);
      expect(used.length, greaterThan(100));
    });

    test('placeholder substitution', () {
      final s = S('zh', {'a': '保留 {pct}%（约 {faces} 面）'});
      expect(s.t('a', {'pct': 50, 'faces': '58'}), '保留 50%（约 58 面）');
      expect(s.t('nope'), 'nope');
    });
  });
}
