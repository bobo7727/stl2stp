import 'package:flutter/foundation.dart';

import '../core/engine.dart';
import '../core/memory_guard.dart';
import '../core/models.dart';

/// One file picked for a batch.
class BatchInput {
  const BatchInput({required this.path, required this.name, required this.size, this.faces});
  final String path;
  final String name;
  final int size;

  /// Face count from `inspect`; null = could not be read (not part of the run).
  final int? faces;

  BatchInput withFaces(int? f) => BatchInput(path: path, name: name, size: size, faces: f);
}

enum BatchItemState { waiting, running, done, failed, skipped, notRun }

/// Why the queue stopped before every file was handled.
enum BatchStopReason { cancelled, memory, background, entitlement }

/// Mutable per-file status. Every item ends in exactly one terminal state
/// (done / failed / skipped / notRun); see [BatchRunner].
class BatchItem {
  BatchItem(this.input);
  final BatchInput input;
  BatchItemState state = BatchItemState.waiting;

  /// i18n key for the skip reason (skipped only).
  String? reasonKey;

  /// Why the queue stopped (notRun only).
  BatchStopReason? stopReason;

  /// Engine status and message (failed only).
  int? status;
  String message = '';
  ConvertResult? result;
  double progress = 0;
  Stage? stage;

  bool get isTerminal =>
      state == BatchItemState.done ||
      state == BatchItemState.failed ||
      state == BatchItemState.skipped ||
      state == BatchItemState.notRun;
}

class BatchSummary {
  const BatchSummary(this.done, this.failed, this.skipped, this.notRun);
  final int done, failed, skipped, notRun;
  int get total => done + failed + skipped + notRun;
}

/// Sequential batch queue (PLAN §8.3, §14c P10/P14). Pure Dart, no UI.
///
/// - Files run one at a time (no stacked memory peaks).
/// - A failing file is recorded and the queue continues.
/// - [stop] ends the queue: the running file is cancelled and recorded as
///   `notRun` with the reason, remaining files become `notRun`. If the engine
///   already finished the running file successfully, it stays `done`.
/// - Entitlement is re-checked between files ([isAllowed]); a revoked
///   entitlement lets the current file finish and stops the queue.
/// - Every item gets exactly one terminal state, so the summary total always
///   equals the number of selected files.
class BatchRunner extends ChangeNotifier {
  BatchRunner({
    required this.engine,
    required List<BatchInput> inputs,
    required this.settings,
    required this.memoryGuard,
    required this.budgetBytes,
    required this.outputPathFor,
    required this.onFileDone,
    this.isAllowed,
    this.maxFacesNow,
    this.limitFaces = 2000000,
  }) : items = [for (final i in inputs) BatchItem(i)];

  final ConversionEngine engine;
  final List<BatchItem> items;
  final ConvertSettings settings;
  final MemoryGuard memoryGuard;
  final int budgetBytes;

  /// Output path for a source file (must not collide with earlier outputs).
  final String Function(String sourcePath, ExportFormat f) outputPathFor;

  /// Called once for each successfully converted file (history, counters).
  final void Function(BatchItem item, ConvertResult result) onFileDone;

  /// False when Pro is no longer active. Null = always allowed.
  final bool Function()? isAllowed;

  /// Free-tier face limit to enforce on each file's real mesh right now (null = none).
  final int? Function()? maxFacesNow;
  final int limitFaces;

  BatchStopReason? _stopReason;
  CancelToken? _current;
  int _index = -1;
  bool _started = false;
  bool _finished = false;

  /// 0-based index of the file being processed, or -1.
  int get currentIndex => _index;
  bool get finished => _finished;
  BatchStopReason? get stopReason => _stopReason;

  BatchSummary get summary {
    var d = 0, f = 0, s = 0, n = 0;
    for (final i in items) {
      switch (i.state) {
        case BatchItemState.done:
          d++;
        case BatchItemState.failed:
          f++;
        case BatchItemState.skipped:
          s++;
        case BatchItemState.notRun:
          n++;
        case BatchItemState.waiting:
        case BatchItemState.running:
          break;
      }
    }
    return BatchSummary(d, f, s, n);
  }

  /// Fraction of files that reached a terminal state, plus the running one's progress.
  double get overall {
    if (items.isEmpty) return 1;
    var sum = 0.0;
    for (final i in items) {
      sum += i.isTerminal ? 1 : (i.state == BatchItemState.running ? i.progress : 0);
    }
    return sum / items.length;
  }

  bool _disposed = false;

  @override
  void notifyListeners() {
    if (!_disposed) super.notifyListeners();
  }

  /// Safe while the queue is still running: later updates are dropped.
  @override
  void dispose() {
    _disposed = true;
    super.dispose();
  }

  /// Ends the queue for [reason]. The first reason wins.
  void stop(BatchStopReason reason) {
    if (_finished) return;
    _stopReason ??= reason;
    _current?.cancel();
  }

  void _terminal(BatchItem it, BatchItemState s) {
    if (it.isTerminal) return; // exactly one terminal state per file
    it.state = s;
    notifyListeners();
  }

  Future<void> run() async {
    assert(!_started, 'BatchRunner.run() may be called once');
    if (_started) return;
    _started = true;
    try {
      for (var i = 0; i < items.length; i++) {
        final it = items[i];
        _index = i;
        if (_stopReason == null && isAllowed != null && !isAllowed!()) _stopReason = BatchStopReason.entitlement;
        if (_stopReason != null) {
          it.stopReason = _stopReason;
          _terminal(it, BatchItemState.notRun);
          continue;
        }
        final faces = it.input.faces;
        if (faces == null) {
          it.status = CoreStatus.errorFormat;
          it.message = 'unreadable';
          _terminal(it, BatchItemState.failed);
          continue;
        }
        if (faces > limitFaces) {
          it.reasonKey = 'batch.skip.faces';
          _terminal(it, BatchItemState.skipped);
          continue;
        }
        if (memoryGuard.check(faces, settings, budgetBytes).level == MemoryLevel.refuse) {
          it.reasonKey = 'batch.skip.memory';
          _terminal(it, BatchItemState.skipped);
          continue;
        }
        try {
          await _runOne(it, faces);
        } catch (e) {
          // Whatever went wrong, the file gets its one terminal state and the queue goes on.
          it.status = CoreStatus.errorInternal;
          it.message = '$e';
          _terminal(it, BatchItemState.failed);
        }
      }
    } finally {
      // Even on an unexpected error the queue ends with every file in a terminal state.
      for (final it in items) {
        if (!it.isTerminal) {
          it.stopReason ??= _stopReason ?? BatchStopReason.cancelled;
          it.state = BatchItemState.notRun;
        }
      }
      _index = -1;
      _finished = true;
      notifyListeners();
    }
  }

  Future<void> _runOne(BatchItem it, int faces) async {
    CancelToken? token;
    it.state = BatchItemState.running;
    notifyListeners();
    try {
      // Everything that can throw (output directory, token, engine) is inside this try.
      final request = ConvertRequest(
        inputPath: it.input.path,
        outputPath: outputPathFor(it.input.path, settings.format),
        settings: settings,
        faces: faces,
        maxFaces: maxFacesNow?.call(),
      );
      token = _current = engine.newCancelToken();
      ConvertResult r;
      try {
        r = await engine.convert(request, cancel: token, onProgress: (p) {
          it.stage = p.stage;
          it.progress = p.overall;
          notifyListeners();
        });
      } catch (e) {
        r = ConvertResult(status: CoreStatus.errorInternal, outputPath: request.outputPath, message: '$e');
      }
      it.result = r;
      if (r.ok) {
        // A finished conversion stays done even if a stop arrived at the boundary.
        try {
          onFileDone(it, r);
        } catch (e) {
          debugPrint('batch onFileDone failed: $e');
        }
        _terminal(it, BatchItemState.done);
      } else if (r.status == CoreStatus.errorCancelled) {
        // A cancelled file always ends the queue, even if the engine reported it unprompted.
        it.stopReason = _stopReason ??= BatchStopReason.cancelled;
        _terminal(it, BatchItemState.notRun);
      } else {
        it.status = r.status;
        it.message = r.message;
        _terminal(it, BatchItemState.failed);
      }
    } finally {
      token?.dispose();
      _current = null;
    }
  }
}
