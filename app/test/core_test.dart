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
      const e = Estimator();
      const m = MeshStats(20000);
      final base = e.estimate(m, const ConvertSettings());
      expect(e.estimate(m, const ConvertSettings(mode: ConvertMode.sewing)).seconds.mid, greaterThan(base.seconds.mid));
      expect(e.estimate(m, const ConvertSettings(simplify: true, keepPercent: 50)).bytes.mid, lessThan(base.bytes.mid));
      expect(e.estimate(m, const ConvertSettings(verify: false)).seconds.mid, lessThan(base.seconds.mid));
      expect(e.estimate(m, const ConvertSettings(format: ExportFormat.iges)).bytes.mid, lessThan(base.bytes.mid));
    });
  });

  group('estimate (calibrated)', () {
    // Measured with stl2stp-cli on the reference PC (calib.csv, 2026-10-21):
    // (triangles, planeGroups, format, mode, merge, simplify%, seconds, bytes).
    // Spheres: planes = seg*(rings-2) + 2*seg; boxes: 6 planes.
    const rows = [
      (4992, 2560, ExportFormat.step, ConvertMode.original, false, 100, 6.013, 11944484),
      (4992, 2560, ExportFormat.step, ConvertMode.original, true, 100, 6.798, 7725064),
      (4992, 2560, ExportFormat.step, ConvertMode.sewing, false, 100, 6.992, 12480362),
      (4992, 2560, ExportFormat.iges, ConvertMode.original, false, 100, 3.188, 10131885),
      (20224, 10240, ExportFormat.step, ConvertMode.original, false, 100, 21.921, 49118546),
      (20224, 10240, ExportFormat.step, ConvertMode.original, true, 100, 19.924, 31776656),
      (20224, 10240, ExportFormat.step, ConvertMode.sewing, false, 100, 22.936, 51394860),
      (20224, 10240, ExportFormat.iges, ConvertMode.original, false, 100, 11.172, 41094540),
      (20172, 6, ExportFormat.step, ConvertMode.original, false, 100, 19.563, 44572644),
      (20172, 6, ExportFormat.step, ConvertMode.original, true, 100, 7.232, 15962),
      (20172, 6, ExportFormat.step, ConvertMode.sewing, false, 100, 28.774, 47356834),
      (20172, 6, ExportFormat.iges, ConvertMode.original, false, 100, 10.908, 34498791),
      (50000, 25200, ExportFormat.step, ConvertMode.original, false, 100, 66.814, 124198488),
      (50000, 25200, ExportFormat.step, ConvertMode.original, true, 100, 63.62, 79456083),
      (50000, 25200, ExportFormat.step, ConvertMode.sewing, false, 100, 55.616, 129763545),
      (50000, 25200, ExportFormat.iges, ConvertMode.original, false, 100, 25.199, 101638314),
      (50000, 25200, ExportFormat.step, ConvertMode.original, false, 50, 19.689, 61082693),
      (50000, 25200, ExportFormat.step, ConvertMode.original, true, 50, 25.461, 48118832),
      (50000, 25200, ExportFormat.step, ConvertMode.original, false, 25, 9.418, 30475779),
      (50000, 25200, ExportFormat.step, ConvertMode.original, true, 25, 15.101, 28052932),
      (20172, 6, ExportFormat.step, ConvertMode.original, false, 50, 9.89, 22178786),
      (20172, 6, ExportFormat.step, ConvertMode.original, true, 50, 5.168, 16189),
      (101760, 51200, ExportFormat.step, ConvertMode.original, false, 100, 116.975, 255428633),
      (101760, 51200, ExportFormat.step, ConvertMode.original, true, 100, 108.462, 164179946),
      (101760, 51200, ExportFormat.iges, ConvertMode.original, false, 100, 55.973, 206929971),
      (99372, 6, ExportFormat.step, ConvertMode.original, false, 100, 134.004, 227487049),
      (99372, 6, ExportFormat.step, ConvertMode.original, true, 100, 31.08, 15991),
      (99372, 6, ExportFormat.step, ConvertMode.sewing, false, 100, 131.064, 241037513),
      (99372, 6, ExportFormat.iges, ConvertMode.original, false, 100, 50.162, 169964811),
    ];

    ConvertSettings settingsFor(ExportFormat f, ConvertMode mode, bool merge, int pct) => ConvertSettings(
        format: f, mode: mode, mergeCoplanar: merge, simplify: pct < 100, keepPercent: pct < 100 ? pct : 50);

    test('every measurement lies inside the predicted range (device factor 1)', () {
      const e = Estimator(deviceFactor: 1, deviceLo: 1, deviceHi: 1);
      for (final (n, planes, f, mode, merge, pct, secs, bytes) in rows) {
        final est = e.estimate(MeshStats(n, planeGroups: planes), settingsFor(f, mode, merge, pct));
        final tag = '$n $f $mode merge=$merge $pct%';
        expect(secs, inInclusiveRange(est.seconds.lo, est.seconds.hi), reason: 'time $tag: ${est.seconds}');
        expect(bytes.toDouble(), inInclusiveRange(est.bytes.lo, est.bytes.hi), reason: 'size $tag: ${est.bytes}');
      }
    });

    test('central values are within 35 % (time) / 20 % (size) for large meshes', () {
      const e = Estimator(deviceFactor: 1);
      for (final (n, planes, f, mode, merge, pct, secs, bytes) in rows) {
        if (bytes < 1e6) continue; // merged boxes: a few KB, relative error meaningless
        // Simplified sphere: the core emitted an open shell (faster than a solid); covered by the range only.
        if (n == 50000 && pct < 100) continue;
        final est = e.estimate(MeshStats(n, planeGroups: planes), settingsFor(f, mode, merge, pct));
        expect(est.seconds.mid / secs, inInclusiveRange(0.65, 1.35), reason: '$n $f $mode $merge $pct');
        expect(est.bytes.mid / bytes, inInclusiveRange(0.8, 1.2), reason: '$n $f $mode $merge $pct');
      }
    });

    test('device factor scales time but not size', () {
      const m = MeshStats(30000);
      final a = const Estimator(deviceFactor: 1).estimate(m, const ConvertSettings());
      final b = const Estimator(deviceFactor: 2).estimate(m, const ConvertSettings());
      expect(b.seconds.mid, closeTo(2 * a.seconds.mid, 1e-9));
      expect(b.bytes.mid, a.bytes.mid);
    });

    test('unknown planarity widens the merge range', () {
      const e = Estimator();
      const s = ConvertSettings(mergeCoplanar: true);
      final known = e.estimate(const MeshStats(50000, planeGroups: 25200), s);
      final unknown = e.estimate(const MeshStats(50000), s);
      expect(unknown.bytes.hi / unknown.bytes.lo, greaterThan(known.bytes.hi / known.bytes.lo));
      expect(e.estimate(const MeshStats(50000, planeGroups: 6), s).bytes.hi, lessThan(1e5));
    });

    test('range wording', () {
      expect(timeRangeKey(const Range(0.1, 0.2, 0.5)).$1, 'estimate.under_1s');
      void check(Range r, String key, int lo, int hi) {
        final (k, p) = timeRangeKey(r);
        expect([k, p['lo'], p['hi']], [key, lo, hi]);
      }

      check(const Range(3.4, 5, 8.2), 'est.range_seconds', 3, 9);
      check(const Range(30, 60, 150), 'est.range_seconds_minutes', 30, 3);
      check(const Range(100, 200, 400), 'est.range_minutes', 1, 7);
      expect(formatBytesRange(const Range(2.3e6, 2.6e6, 3.1e6)), '2.2–3.0 MB');
      expect(formatBytesRange(const Range(9e5, 1e6, 1.3e6)), '879 KB–1.2 MB');
    });
  });

  group('plane counter', () {
    test('bracket has 8 planes, sphere ≈ quads + pole triangles', () {
      // 8 outline segments, but two pairs are collinear (split at the inner corner) → 6 walls + 2 caps.
      expect(parseStlBytes(buildBracketStl()).planeGroups, 8);
      final sphere = parseStlBytes(buildSphereStl(seg: 64, rings: 32));
      expect(sphere.planeGroups, closeTo(64 * 30 + 2 * 64, 64 * 32 * 0.02));
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
    });

    test('app-only strings: same keys/placeholders, no clash with design keys', () {
      final zh = load('app_zh'), en = load('app_en'), design = load('zh');
      expect(zh.keys.toSet(), en.keys.toSet());
      expect(zh.keys.where(design.containsKey), isEmpty);
      final ph = RegExp(r'\{(\w+)\}');
      for (final k in zh.keys) {
        expect(ph.allMatches(zh[k]!).map((m) => m[1]).toSet(), ph.allMatches(en[k]!).map((m) => m[1]).toSet(),
            reason: k);
        expect(zh[k]!.contains('!') || zh[k]!.contains('！'), isFalse, reason: 'no exclamation marks: $k');
      }
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
      final zh = {...load('zh'), ...load('app_zh')};
      final used = <String>{};
      final re = RegExp(r"""\.t\('([a-z0-9_.]+)'""");
      for (final f in Directory('lib').listSync(recursive: true).whereType<File>()) {
        for (final m in re.allMatches(f.readAsStringSync())) {
          used.add(m[1]!);
        }
      }
      final missing = used.where((k) => !zh.containsKey(k)).toList();
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
