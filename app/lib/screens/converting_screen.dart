import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:path/path.dart' as p;

import '../core/engine.dart';
import '../core/models.dart';
import '../l10n/strings.dart';
import '../state/app_state.dart';
import '../state/history.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';
import 'done_screen.dart';
import 'error_screen.dart';
import 'flow.dart';
import 'preview_screen.dart';

/// Full-screen progress (ui-spec §4.7), not dismissible; cancel asks first (§4.8).
class ConvertingScreen extends StatefulWidget {
  const ConvertingScreen({super.key, required this.request, required this.model});
  final ConvertRequest request;
  final LoadedModel model;
  @override
  State<ConvertingScreen> createState() => _ConvertingScreenState();
}

class _ConvertingScreenState extends State<ConvertingScreen> {
  late final CancelToken _cancel;
  late final List<Stage> _stages = widget.request.stages;
  ConvertProgress? _progress;
  final _sw = Stopwatch();
  Timer? _tick;
  DateTime _lastUi = DateTime.fromMillisecondsSinceEpoch(0);

  @override
  void initState() {
    super.initState();
    final state = AppScope.read(context);
    _cancel = state.engine.newCancelToken();
    _sw.start();
    _tick = Timer.periodic(const Duration(milliseconds: 500), (_) => setState(() {}));
    WidgetsBinding.instance.addPostFrameCallback((_) => _run(state));
  }

  @override
  void dispose() {
    _tick?.cancel();
    super.dispose();
  }

  Future<void> _run(AppState state) async {
    final r = await state.engine.convert(widget.request, cancel: _cancel, onProgress: (pr) {
      // Throttle UI updates to ≤ 20 per second.
      final now = DateTime.now();
      if (now.difference(_lastUi).inMilliseconds >= 50 || pr.stage != _progress?.stage) {
        _lastUi = now;
        if (mounted) setState(() => _progress = pr);
      }
    });
    _cancel.dispose();
    _sw.stop();
    if (!mounted) return;
    final nav = Navigator.of(context);
    if (r.ok) {
      final bytes = File(r.outputPath).existsSync() ? File(r.outputPath).lengthSync() : 0;
      state.recordConversion();
      state.addHistory(HistoryEntry(
        outputPath: r.outputPath,
        format: widget.request.settings.format,
        bytes: bytes,
        time: DateTime.now(),
        sourceName: p.basename(widget.request.inputPath),
        settings: widget.request.settings,
      ));
      nav.pushReplacement(MaterialPageRoute(builder: (_) => DoneScreen(request: widget.request, result: r, bytes: bytes)));
      return;
    }
    final kind = switch (r.status) {
      CoreStatus.errorCancelled => ErrorKind.cancelled,
      CoreStatus.errorMemory => ErrorKind.memory,
      CoreStatus.errorEmpty => ErrorKind.empty,
      CoreStatus.errorFormat => ErrorKind.format,
      CoreStatus.errorIo => r.failedStage == Stage.read ? ErrorKind.format : ErrorKind.io,
      _ => ErrorKind.generic,
    };
    final model = widget.model;
    final settings = widget.request.settings;
    nav.pushReplacement(MaterialPageRoute(
      builder: (_) => ErrorScreen(
        kind: kind,
        code: r.status,
        details: r.message,
        faces: model.info.faces,
        onPrimary: (ctx) {
          switch (kind) {
            case ErrorKind.cancelled:
              Navigator.of(ctx).pop();
            case ErrorKind.memory:
              Navigator.of(ctx).pushReplacement(MaterialPageRoute(
                  builder: (_) => PreviewScreen(model: model, initialSettings: settings, forceSimplifyPercent: 50)));
            case ErrorKind.io:
            case ErrorKind.generic:
              Navigator.of(ctx).pushReplacement(
                  MaterialPageRoute(builder: (_) => ConvertingScreen(request: widget.request, model: model)));
            default:
              pickStl(ctx).then((path) {
                if (path != null && ctx.mounted) openModel(ctx, path, replace: true);
              });
          }
        },
      ),
    ));
  }

  Future<void> _askCancel() async {
    final s = S.of(context);
    final t = Tokens.of(context);
    final yes = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(s.t('cancel.confirm.title')),
        content: Text(s.t('cancel.confirm.body')),
        actions: [
          TextButton(
              key: const Key('cancelYes'),
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(s.t('cancel.confirm.yes'), style: TextStyle(color: t.danger))),
          FilledButton(
              style: FilledButton.styleFrom(minimumSize: const Size(64, 40)),
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(s.t('cancel.confirm.no'))),
        ],
      ),
    );
    if (yes == true) _cancel.cancel();
  }

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final pr = _progress;
    final secs = _sw.elapsed.inSeconds;
    final elapsed = secs < 60
        ? (s.isZh ? '$secs 秒' : '$secs s')
        : '${secs ~/ 60}:${(secs % 60).toString().padLeft(2, '0')}';
    final currentIdx = pr == null ? 0 : _stages.indexOf(pr.stage);
    return PopScope(
      canPop: false,
      child: Scaffold(
        appBar: AppBar(title: Text(s.t('converting.title')), automaticallyImplyLeading: false),
        body: SafeArea(
          child: Column(children: [
            const SizedBox(height: 24),
            ProgressRing(value: pr?.overall ?? 0),
            const SizedBox(height: 16),
            Text(p.basename(widget.request.outputPath),
                style: TextStyle(fontSize: 17, fontWeight: FontWeight.w600, color: t.label)),
            Text(s.t('converting.elapsed', {'t': elapsed}),
                style: TextStyle(fontSize: 13, color: t.label2, fontFeatures: tabular)),
            const SizedBox(height: 24),
            Expanded(
              child: Align(
                alignment: Alignment.topCenter,
                child: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 320),
                  child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                    for (final (i, st) in _stages.indexed)
                      Padding(
                        padding: const EdgeInsets.symmetric(vertical: 6),
                        child: Row(children: [
                          if (i < currentIdx)
                            Icon(Icons.check_circle, color: t.success, size: 22)
                          else if (i == currentIdx)
                            Icon(Icons.radio_button_checked, color: t.accent, size: 22)
                          else
                            Icon(Icons.radio_button_unchecked, color: t.label3, size: 22),
                          const SizedBox(width: 10),
                          Text(s.t('converting.stage.${st.name}'),
                              style: TextStyle(
                                  fontSize: 16,
                                  fontWeight: i == currentIdx ? FontWeight.w600 : FontWeight.normal,
                                  color: i > currentIdx ? t.label3 : t.label)),
                        ]),
                      ),
                  ]),
                ),
              ),
            ),
            Text(s.t('converting.keep_open'), style: TextStyle(fontSize: 12, color: t.label2)),
            Padding(
              padding: const EdgeInsets.all(16),
              child: SizedBox(
                width: double.infinity,
                child: TextButton.icon(
                  key: const Key('cancelButton'),
                  style: TextButton.styleFrom(
                      foregroundColor: t.danger, backgroundColor: t.fill, minimumSize: const Size.fromHeight(50)),
                  onPressed: _askCancel,
                  icon: const Icon(Icons.stop_circle_outlined),
                  label: Text(s.t('converting.cancel')),
                ),
              ),
            ),
          ]),
        ),
      ),
    );
  }
}
