import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:stl2stp_app/app.dart';
import 'package:stl2stp_app/batch/batch_runner.dart';
import 'package:stl2stp_app/core/demo_engine.dart';
import 'package:stl2stp_app/core/engine.dart';
import 'package:stl2stp_app/core/ffi_engine.dart' show resultAfterExit;
import 'package:stl2stp_app/screens/converting_screen.dart';
import 'package:stl2stp_app/core/memory_guard.dart';
import 'package:stl2stp_app/core/models.dart';
import 'package:stl2stp_app/core/sample_mesh.dart';
import 'package:stl2stp_app/l10n/strings.dart';
import 'package:stl2stp_app/purchase/free_tier.dart';
import 'package:stl2stp_app/purchase/mock_purchase_service.dart';
import 'package:stl2stp_app/screens/batch_screen.dart';
import 'package:stl2stp_app/screens/flow.dart';
import 'package:stl2stp_app/state/app_state.dart';

/// Engine whose conversions finish only when the test says so.
class GateEngine extends DemoEngine {
  GateEngine() : super(stepDelay: Duration.zero);
  final calls = <String>[];
  final gates = <String, Completer<int>>{};
  Completer<int> gate(String name) => gates.putIfAbsent(name, Completer<int>.new);
  final failNames = <String>{};

  @override
  Future<ConvertResult> convert(ConvertRequest request,
      {required void Function(ConvertProgress) onProgress, required CancelToken cancel}) async {
    final name = request.inputPath.split('/').last;
    calls.add(name);
    if (failNames.contains(name)) return ConvertResult(status: CoreStatus.errorFormat, outputPath: request.outputPath, message: 'bad');
    final g = gates[name];
    if (g != null) {
      final status = await g.future;
      if (status == CoreStatus.ok && !cancel.isCancelled) File(request.outputPath).writeAsStringSync('x');
      return ConvertResult(status: cancel.isCancelled && status != CoreStatus.ok ? CoreStatus.errorCancelled : status, outputPath: request.outputPath);
    }
    File(request.outputPath).writeAsStringSync('x');
    return ConvertResult(status: CoreStatus.ok, outputPath: request.outputPath);
  }
}

BatchInput _in(String name, [int? faces = 100]) => BatchInput(path: '/tmp/$name', name: name, size: 10, faces: faces);

BatchRunner _runner(GateEngine e, List<BatchInput> ins, Directory out,
    {bool Function()? allowed, int budget = 1 << 40, void Function(BatchItem, ConvertResult)? done}) {
  var n = 0;
  return BatchRunner(
    engine: e,
    inputs: ins,
    settings: const ConvertSettings(),
    memoryGuard: const MemoryGuard(),
    budgetBytes: budget,
    outputPathFor: (src, f) => '${out.path}/${n++}.${f.extension}',
    onFileDone: done ?? (_, _) {},
    isAllowed: allowed,
  );
}

class _SpyEngine extends GateEngine {
  _SpyEngine(this.seen);
  final List<int?> seen;
  @override
  Future<ConvertResult> convert(ConvertRequest request,
      {required void Function(ConvertProgress) onProgress, required CancelToken cancel}) {
    seen.add(request.maxFaces);
    return super.convert(request, onProgress: onProgress, cancel: cancel);
  }
}

void _expectUnique(BatchRunner r) {
  expect(r.items.every((i) => i.isTerminal), isTrue);
  expect(r.summary.total, r.items.length);
}

final class _FakePicked extends PlatformFile {
  _FakePicked(this.name, this.data, {String? path, String? source})
      : uri = path != null ? Uri.file(path) : Uri.parse(source ?? 'content://doc/$name');
  @override
  final String name;
  final List<int> data;
  @override
  final Uri uri;
  @override
  int? lengthSync() => data.length;
  @override
  Future<int?> length() async => data.length;
  @override
  Future<Uint8List> readAsBytes() async => Uint8List.fromList(data);
  @override
  Stream<Uint8List> readAsByteStream() => Stream.value(Uint8List.fromList(data));
  @override
  get xFile => throw UnimplementedError();
}

void main() {
  late Directory out;
  setUp(() => out = Directory.systemTemp.createTempSync('stl2stp_batch_t'));
  tearDown(() => out.deleteSync(recursive: true));

  group('BatchRunner', () {
    test('all succeed, in order, one at a time', () async {
      final e = GateEngine();
      final done = <String>[];
      final r = _runner(e, [_in('a.stl'), _in('b.stl'), _in('c.stl')], out, done: (i, _) => done.add(i.input.name));
      await r.run();
      expect(e.calls, ['a.stl', 'b.stl', 'c.stl']);
      expect(done, ['a.stl', 'b.stl', 'c.stl']);
      expect(r.summary.done, 3);
      _expectUnique(r);
    });

    test('a failure and a skip do not stop the queue', () async {
      final e = GateEngine()..failNames.add('b.stl');
      final r = _runner(e, [_in('a.stl'), _in('b.stl'), _in('big.stl', 3000000), _in('c.stl'), _in('bad.stl', null)], out);
      await r.run();
      expect(r.items.map((i) => i.state), [
        BatchItemState.done,
        BatchItemState.failed,
        BatchItemState.skipped,
        BatchItemState.done,
        BatchItemState.failed,
      ]);
      expect(r.items[2].reasonKey, 'batch.skip.faces');
      expect(e.calls, ['a.stl', 'b.stl', 'c.stl']);
      _expectUnique(r);
    });

    test('memory refuse is skipped, never silently reduced', () async {
      final e = GateEngine();
      final r = _runner(e, [_in('a.stl', 1000000)], out, budget: 100 * 1000 * 1000);
      await r.run();
      expect(r.items.single.state, BatchItemState.skipped);
      expect(r.items.single.reasonKey, 'batch.skip.memory');
      expect(e.calls, isEmpty);
    });

    test('stop while a file runs: it and the rest are notRun with the reason', () async {
      final e = GateEngine();
      e.gate('b.stl');
      final r = _runner(e, [_in('a.stl'), _in('b.stl'), _in('c.stl')], out);
      final f = r.run();
      await Future<void>.delayed(const Duration(milliseconds: 20));
      expect(r.items[1].state, BatchItemState.running);
      r.stop(BatchStopReason.memory);
      r.stop(BatchStopReason.background); // first reason wins
      e.gate('b.stl').complete(CoreStatus.errorCancelled);
      await f;
      expect(r.items.map((i) => i.state), [BatchItemState.done, BatchItemState.notRun, BatchItemState.notRun]);
      expect(r.items[1].stopReason, BatchStopReason.memory);
      expect(r.items[2].stopReason, BatchStopReason.memory);
      _expectUnique(r);
    });

    test('success that arrives after stop stays done (boundary)', () async {
      final e = GateEngine();
      e.gate('a.stl');
      final r = _runner(e, [_in('a.stl'), _in('b.stl')], out);
      final f = r.run();
      await Future<void>.delayed(const Duration(milliseconds: 20));
      r.stop(BatchStopReason.cancelled);
      e.gate('a.stl').complete(CoreStatus.ok);
      await f;
      expect(r.items[0].state, BatchItemState.done);
      expect(r.items[1].state, BatchItemState.notRun);
      _expectUnique(r);
    });

    test('stop between files', () async {
      final e = GateEngine();
      late BatchRunner r;
      r = _runner(e, [_in('a.stl'), _in('b.stl')], out, done: (i, _) => r.stop(BatchStopReason.cancelled));
      await r.run();
      expect(r.items.map((i) => i.state), [BatchItemState.done, BatchItemState.notRun]);
      _expectUnique(r);
    });

    test('entitlement revoked mid-run: current file finishes, the rest stop', () async {
      final e = GateEngine();
      e.gate('a.stl');
      var allowed = true;
      final r = _runner(e, [_in('a.stl'), _in('b.stl')], out, allowed: () => allowed);
      final f = r.run();
      await Future<void>.delayed(const Duration(milliseconds: 20));
      allowed = false;
      e.gate('a.stl').complete(CoreStatus.ok);
      await f;
      expect(r.items[0].state, BatchItemState.done);
      expect(r.items[1].state, BatchItemState.notRun);
      expect(r.items[1].stopReason, BatchStopReason.entitlement);
      expect(e.calls, ['a.stl']);
    });

    test('entitlement already gone at the first file: nothing runs', () async {
      final e = GateEngine();
      final r = _runner(e, [_in('a.stl')], out, allowed: () => false);
      await r.run();
      expect(e.calls, isEmpty);
      expect(r.summary.notRun, 1);
    });

    test('output path creation fails for one file: it fails, the queue goes on and ends', () async {
      final e = GateEngine();
      var n = 0;
      final r = BatchRunner(
        engine: e,
        inputs: [_in('a.stl'), _in('b.stl'), _in('c.stl')],
        settings: const ConvertSettings(),
        memoryGuard: const MemoryGuard(),
        budgetBytes: 1 << 40,
        outputPathFor: (src, f) {
          if (src.endsWith('b.stl')) throw const FileSystemException('disk full');
          return '${out.path}/${n++}.${f.extension}';
        },
        onFileDone: (_, _) {},
      );
      await r.run();
      expect(r.finished, isTrue);
      expect(r.items.map((i) => i.state), [BatchItemState.done, BatchItemState.failed, BatchItemState.done]);
      expect(e.calls, ['a.stl', 'c.stl']);
      _expectUnique(r);
    });

    test('the free-tier face limit is handed to every conversion', () async {
      final seen = <int?>[];
      final e = _SpyEngine(seen);
      var allowedMax = 50000;
      final r = BatchRunner(
        engine: e,
        inputs: [_in('a.stl'), _in('b.stl')],
        settings: const ConvertSettings(),
        memoryGuard: const MemoryGuard(),
        budgetBytes: 1 << 40,
        outputPathFor: (src, f) => '${out.path}/${src.split('/').last}.step',
        onFileDone: (_, _) => allowedMax = 10,
        maxFacesNow: () => allowedMax,
      );
      await r.run();
      expect(seen, [50000, 10]);
    });

    test('dispose while running does not throw', () async {
      final e = GateEngine();
      e.gate('a.stl');
      final r = _runner(e, [_in('a.stl')], out);
      final f = r.run();
      await Future<void>.delayed(const Duration(milliseconds: 20));
      r.stop(BatchStopReason.cancelled);
      r.dispose();
      e.gate('a.stl').complete(CoreStatus.errorCancelled);
      await f;
      expect(r.items.single.state, BatchItemState.notRun);
    });
  });

  group('real-mesh free limit (C1) and engine exit (C2)', () {
    test('DemoEngine enforces maxFaces on the mesh it would build; no file is written', () async {
      final e = DemoEngine(stepDelay: Duration.zero);
      const s = ConvertSettings(simplify: true, keepPercent: 50);
      final over = await e.convert(
          ConvertRequest(inputPath: 'x', outputPath: '${out.path}/o.step', settings: s, faces: 100, maxFaces: 40),
          onProgress: (_) {},
          cancel: e.newCancelToken());
      expect(over.status, CoreStatus.errorFreeLimit);
      expect(over.facesAfterSimplify, 50);
      expect(File('${out.path}/o.step').existsSync(), isFalse);
      final ok = await e.convert(
          ConvertRequest(inputPath: 'x', outputPath: '${out.path}/o2.step', settings: s, faces: 100, maxFaces: 50),
          onProgress: (_) {},
          cancel: e.newCancelToken());
      expect(ok.ok, isTrue);
    });

    testWidgets('converting screen shows the free-limit result, not a success', (tester) async {
      await tester.binding.setSurfaceSize(const Size(430, 1000));
      await S.preload();
      SharedPreferences.setMockInitialValues({'onboardingSeen': true, 'language': 'zh'});
      final prefs = await SharedPreferences.getInstance();
      final st = AppState(engine: DemoEngine(stepDelay: Duration.zero), prefs: prefs, outputDir: out);
      final model = LoadedModel(
          const MeshInfo(
              path: 'x', fileName: 'x.stl', fileSize: 1, faces: 100, vertices: 1, edges: 1, min: [0, 0, 0], max: [1, 1, 1], isAscii: false),
          null);
      final req = ConvertRequest(
          inputPath: 'x', outputPath: '${out.path}/o.step', settings: const ConvertSettings(), faces: 100, maxFaces: 40);
      await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: ConvertingScreen(request: req, model: model)));
      for (var i = 0; i < 20; i++) {
        await tester.pump(const Duration(milliseconds: 100));
      }
      expect(find.text('简化后仍超过免费上限'), findsOneWidget);
      expect(st.history, isEmpty);
    });

    test('a conversion is not finished until the worker has exited, however long that takes', () async {
      final exited = Completer<void>();
      var finished = false;
      final f = resultAfterExit(Future.value(1), exited.future).then((v) => finished = true);
      await Future<void>.delayed(const Duration(milliseconds: 80));
      expect(finished, isFalse);
      exited.complete();
      await f;
      expect(finished, isTrue);
    });
  });

  group('materializePicked (§14c P13)', () {
    test('same name, different sources and content: separate sub-directories', () async {
      final r = await materializePicked([
        _FakePicked('part.stl', [1, 2, 3]),
        _FakePicked('part.stl', [9, 9, 9, 9]),
      ], tempRoot: out);
      expect(r.length, 2);
      expect(r[0] == r[1], isFalse);
      expect(File(r[0]).parent.path == File(r[1]).parent.path, isFalse);
      expect(File(r[0]).readAsBytesSync(), [1, 2, 3]);
      expect(File(r[1]).readAsBytesSync(), [9, 9, 9, 9]);
      expect(r.every((p) => p.endsWith('/part.stl')), isTrue);
    });

    test('different source URIs with identical name and content are one file (content identity)', () async {
      final a = _FakePicked('p.stl', [4, 5, 6], source: 'content://provider-a/1');
      final b = _FakePicked('p.stl', [4, 5, 6], source: 'content://provider-b/9');
      expect(a.uri == b.uri, isFalse); // identity is the bytes, not the uri
      final r = await materializePicked([a, b, _FakePicked('p.stl', [4, 5, 7])], tempRoot: out);
      expect(r.length, 2);
    });

    test('same source twice is kept once; local paths dedupe by path', () async {
      final r = await materializePicked([
        _FakePicked('a.stl', [1, 2]),
        _FakePicked('a.stl', [1, 2]),
        _FakePicked('b.stl', [], path: '/x/b.stl'),
        _FakePicked('b.stl', [], path: '/x/b.stl'),
        _FakePicked('b.stl', [], path: '/y/b.stl'),
      ], tempRoot: out);
      expect(r.length, 3);
      expect(r.where((p) => p.startsWith('/x/') || p.startsWith('/y/')).length, 2);
    });
  });

  group('batch flow', () {
    Future<(AppState, GateEngine)> makeState({bool pro = false, FreeTierLimits limits = FreeTierLimits.release}) async {
      // Pro is seeded in the prefs: a delayed buyPro() would hang under testWidgets' fake clock.
      SharedPreferences.setMockInitialValues(
          {'onboardingSeen': true, 'language': 'en', if (pro) ...{'mock.proOwned': true, 'mock.proActive': true}});
      final prefs = await SharedPreferences.getInstance();
      final dir = Directory.systemTemp.createTempSync('stl2stp_batch_s');
      addTearDown(() => dir.deleteSync(recursive: true));
      final e = GateEngine();
      final m = MockPurchaseService(prefs, delay: Duration.zero);
      final st = AppState(engine: e, prefs: prefs, outputDir: dir, purchase: m, limits: limits);
      return (st, e);
    }

    test('free user is walled; Pro and the none-limits are not', () async {
      final (free, _) = await makeState();
      expect(free.batchLimit()!.kind, LimitKind.batch);
      final (pro, _) = await makeState(pro: true);
      expect(pro.batchLimit(), isNull);
      final (none, _) = await makeState(limits: FreeTierLimits.none);
      expect(none.batchLimit(), isNull);
    });

    test('entitlement lost after the setup page opened is seen at start', () async {
      final (st, _) = await makeState(pro: true);
      expect(st.batchLimit(), isNull);
      // Revoke through the same prefs key the mock reads.
      await st.prefs.setBool('mock.proActive', false);
      expect(st.batchLimit()!.kind, LimitKind.batch);
    });

    testWidgets('free user tapping the batch entry sees the paywall, not the picker', (tester) async {
      await tester.binding.setSurfaceSize(const Size(430, 1400));
      await S.preload();
      final (st, _) = await makeState();
      await tester.pumpWidget(Stl2StpApp(state: st));
      await tester.pumpAndSettle();
      await tester.ensureVisible(find.byKey(const Key('batchButton')));
      await tester.tap(find.byKey(const Key('batchButton')));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('proTitle')), findsOneWidget);
      expect(find.text('Converting several files at once is part of Pro.'), findsOneWidget);
    });

    Future<(AppState, GateEngine, List<String>)> setupWithFiles(WidgetTester tester, {bool pro = true}) async {
      await tester.binding.setSurfaceSize(const Size(430, 1400));
      await S.preload();
      final (st, e) = await makeState(pro: pro);
      final tmp = Directory.systemTemp.createTempSync('stl_in');
      addTearDown(() => tmp.deleteSync(recursive: true));
      final paths = <String>[];
      for (final n in ['one.stl', 'two.stl']) {
        File('${tmp.path}/$n').writeAsBytesSync(buildBracketStl());
        paths.add('${tmp.path}/$n');
      }
      await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: BatchScreen(paths: [paths[0]])));
      await tester.pump();
      await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 300)));
      await tester.pump(const Duration(milliseconds: 100));
      return (st, e, paths);
    }

    testWidgets('picker unavailable: a second file can still be added (path dialog)', (tester) async {
      final (_, _, paths) = await setupWithFiles(tester);
      expect(find.text('1 files'), findsOneWidget);
      await tester.tap(find.byKey(const Key('batchAddMore')));
      await tester.pumpAndSettle();
      await tester.enterText(find.byKey(const Key('pathField')), paths[1]);
      await tester.tap(find.text('OK'));
      await tester.pump();
      await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 300)));
      await tester.pump(const Duration(milliseconds: 100));
      expect(find.text('2 files'), findsOneWidget);
      expect(find.text('two.stl'), findsOneWidget);
    });

    testWidgets('Start re-checks Pro: revoked after the page opened → paywall, nothing runs, selection stays', (tester) async {
      final (st, e, _) = await setupWithFiles(tester);
      await st.prefs.setBool('mock.proActive', false); // refund while on the setup page
      await tester.tap(find.byKey(const Key('batchStart')));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('proTitle')), findsOneWidget);
      expect(e.calls, isEmpty);
      await tester.tap(find.byKey(const Key('laterButton')));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('batchStart')), findsOneWidget); // still on setup, files kept
      expect(find.text('one.stl'), findsOneWidget);
      expect(e.calls, isEmpty);
    });

    testWidgets('Start re-checks Pro: buying on that paywall continues into the run', (tester) async {
      final (st, e, _) = await setupWithFiles(tester);
      await st.prefs.setBool('mock.proActive', false);
      await tester.tap(find.byKey(const Key('batchStart')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('buyButton')));
      for (var i = 0; i < 40 && find.byKey(const Key('batchFinish')).evaluate().isEmpty; i++) {
        await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 20)));
        await tester.pump(const Duration(milliseconds: 50));
      }
      expect(find.byKey(const Key('batchFinish')), findsOneWidget);
      expect(e.calls, ['one.stl']);
    });

    testWidgets('setup page → run → summary lists history', (tester) async {
      await tester.binding.setSurfaceSize(const Size(430, 1400));
      await S.preload();
      final (st, e) = await makeState(pro: true);
      final tmp = Directory.systemTemp.createTempSync('stl_in');
      addTearDown(() => tmp.deleteSync(recursive: true));
      final paths = <String>[];
      for (final n in ['one.stl', 'two.stl']) {
        File('${tmp.path}/$n').writeAsBytesSync(buildBracketStl());
        paths.add('${tmp.path}/$n');
      }
      paths.add(paths.first); // duplicate
      paths.add('${tmp.path}/missing.stl');
      await tester.pumpWidget(Stl2StpApp(state: st, homeOverride: BatchScreen(paths: paths)));
      await tester.pump(); // first frame starts the inspections
      await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 300)));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 100));
      expect(find.text('3 files'), findsOneWidget); // duplicate removed
      expect(find.text('Can’t read'), findsOneWidget);
      await tester.tap(find.byKey(const Key('batchStart')));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 400));
      for (var i = 0; i < 50 && find.byKey(const Key('batchFinish')).evaluate().isEmpty; i++) {
        await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 20)));
        await tester.pump();
      }
      expect(find.byKey(const Key('batchFinish')), findsOneWidget);
      expect(e.calls, ['one.stl', 'two.stl']);
      expect(st.history.length, 2);
      expect(find.textContaining('Done 2 · Failed 1'), findsOneWidget);
    });
  });
}
