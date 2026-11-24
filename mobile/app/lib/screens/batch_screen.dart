import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:path/path.dart' as p;
import 'package:share_plus/share_plus.dart';

import '../batch/batch_runner.dart';
import '../core/estimate.dart' show formatBytes, formatCount;
import '../core/models.dart';
import '../l10n/strings.dart';
import '../state/app_state.dart';
import '../state/history.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';
import 'flow.dart';
import 'paywall_screen.dart';

/// Home entry for batch conversion: checks the entitlement, then picks files
/// and opens [BatchScreen] (PLAN §8.1).
Future<void> startBatchFlow(BuildContext context) async {
  if (!await ensureBatchAllowed(context) || !context.mounted) return;
  final session = PickSession();
  final paths = await pickStls(context, session: session);
  if (paths.isEmpty || !context.mounted) return;
  await Navigator.of(context).push(MaterialPageRoute(builder: (_) => BatchScreen(paths: paths, session: session)));
}

/// Shows the paywall until batch is allowed. A purchase is followed by a fresh
/// check (§14c P10). Returns false when the user stays out.
Future<bool> ensureBatchAllowed(BuildContext context) async {
  for (var i = 0; i < 3; i++) {
    final hit = AppScope.read(context).batchLimit();
    if (hit == null) return true;
    final r = await Navigator.push<PaywallResult>(context, MaterialPageRoute(builder: (_) => PaywallScreen(reason: hit)));
    if (!context.mounted) return false;
    if (r is! PaywallPurchased) return false;
  }
  return false;
}

/// Setup page: file list, shared settings, start (PLAN §8.2).
class BatchScreen extends StatefulWidget {
  const BatchScreen({super.key, required this.paths, this.session});
  final List<String> paths;

  /// Content identities of the first pick; later "Add more" picks continue it.
  final PickSession? session;
  @override
  State<BatchScreen> createState() => _BatchScreenState();
}

class _BatchScreenState extends State<BatchScreen> {
  late List<BatchInput> _inputs;
  bool _reading = true;
  late final PickSession _session = widget.session ?? PickSession();
  bool _inspecting = false;
  final _inspected = <String>{};
  bool _disposed = false;
  bool _truncated = false;
  late ConvertSettings _settings;
  final _meta = List.generate(4, (_) => TextEditingController());

  @override
  void initState() {
    super.initState();
    final state = AppScope.read(context);
    _inputs = [];
    _add(widget.paths);
    final base = state.initialSettings();
    _settings = ConvertSettings.forPreset(Preset.recommended, format: base.format).copyWith(
        author: base.author, organization: base.organization, product: base.product, description: base.description);
    _meta[0].text = base.author;
    _meta[1].text = base.organization;
    _meta[2].text = base.product;
    _meta[3].text = base.description;
    WidgetsBinding.instance.addPostFrameCallback((_) => _inspectAll(state));
  }

  @override
  void dispose() {
    _disposed = true;
    for (final c in _meta) {
      c.dispose();
    }
    super.dispose();
  }

  /// Adds paths (de-duplicated, capped at [maxBatchFiles]); returns whether any were new.
  bool _add(List<String> paths) {
    var added = false;
    for (final path in paths) {
      if (_inputs.any((i) => i.path == path)) continue;
      if (_inputs.length >= maxBatchFiles) {
        _truncated = true;
        break;
      }
      _inputs.add(BatchInput(path: path, name: p.basename(path), size: File(path).existsSync() ? File(path).lengthSync() : 0));
      added = true;
    }
    return added;
  }

  Future<void> _addMore() async {
    final paths = await pickStls(context, session: _session);
    if (!mounted || paths.isEmpty) return;
    if (_add(paths)) {
      setState(() => _reading = true);
      _inspectAll(AppScope.read(context));
    } else {
      setState(() {});
    }
  }

  /// One at a time; stops as soon as the page is left (§14c P12). Safe to call
  /// again after files were added: only new files are read, never two loops at once.
  Future<void> _inspectAll(AppState state) async {
    if (_inspecting) return;
    _inspecting = true;
    for (var i = 0; i < _inputs.length; i++) {
      if (_disposed) return;
      if (!_inspected.add(_inputs[i].path)) continue;
      int? faces;
      try {
        faces = (await state.engine.inspect(_inputs[i].path)).faces;
      } catch (_) {
        faces = null;
      }
      if (_disposed) return;
      setState(() => _inputs[i] = _inputs[i].withFaces(faces));
    }
    _inspecting = false;
    if (_disposed) return;
    // Files added while the loop ran its last pass are picked up here.
    if (_inputs.any((i) => !_inspected.contains(i.path))) return _inspectAll(state);
    setState(() => _reading = false);
  }

  ConvertSettings get _final => _settings.copyWith(
      author: _meta[0].text, organization: _meta[1].text, product: _meta[2].text, description: _meta[3].text);

  Future<void> _start() async {
    // Entitlement is checked again at the moment of starting (§14c P10).
    if (!await ensureBatchAllowed(context) || !mounted) return;
    final state = AppScope.read(context);
    final settings = _final;
    state.rememberSettings(settings);
    await Navigator.of(context).pushReplacement(MaterialPageRoute(
        builder: (_) => BatchRunningScreen(inputs: _inputs, settings: settings)));
  }

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final ready = _inputs.where((i) => i.faces != null).length;
    return Scaffold(
      appBar: AppBar(title: Text(s.t('batch.title'))),
      body: ListView(padding: const EdgeInsets.all(16), children: [
        Text(s.t('batch.count', {'n': _inputs.length}),
            style: TextStyle(fontSize: 15, color: t.label2)),
        if (_truncated) ...[
          const SizedBox(height: 8),
          InfoBanner(kind: BannerKind.warn, text: s.t('batch.too_many', {'max': maxBatchFiles})),
        ],
        if (_reading) ...[
          const SizedBox(height: 8),
          Row(children: [
            const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2)),
            const SizedBox(width: 8),
            Text(s.t('batch.reading'), style: TextStyle(color: t.label2)),
          ]),
        ],
        const SizedBox(height: 8),
        Surface(
          padding: EdgeInsets.zero,
          child: Column(children: [
            for (final i in _inputs)
              ListTile(
                dense: true,
                title: Text(i.name, maxLines: 1, overflow: TextOverflow.ellipsis),
                subtitle: Text(formatBytes(i.size), style: TextStyle(color: t.label2)),
                trailing: Text(
                    i.faces != null
                        ? s.t('batch.faces', {'faces': formatCount(i.faces!)})
                        : (_reading ? '…' : s.t('batch.unreadable')),
                    style: TextStyle(color: i.faces == null && !_reading ? t.danger : t.label2)),
              ),
          ]),
        ),
        Align(
          alignment: Alignment.centerLeft,
          child: TextButton.icon(
            key: const Key('batchAddMore'),
            icon: const Icon(Icons.add, size: 18),
            label: Text(s.t('batch.add_more')),
            onPressed: _reading || _inputs.length >= maxBatchFiles ? null : _addMore,
          ),
        ),
        SectionHeader(s.t('batch.settings')),
        Text(s.t('batch.format'), style: TextStyle(color: t.label2)),
        const SizedBox(height: 4),
        SegmentedButton<ExportFormat>(
          key: const Key('batchFormat'),
          segments: [for (final f in ExportFormat.values) ButtonSegment(value: f, label: Text(f.label))],
          selected: {_settings.format},
          onSelectionChanged: (v) => setState(() => _settings = _settings.copyWith(format: v.first)),
        ),
        const SizedBox(height: 12),
        Text(s.t('batch.preset'), style: TextStyle(color: t.label2)),
        const SizedBox(height: 4),
        SegmentedButton<Preset>(
          key: const Key('batchPreset'),
          segments: [for (final pr in Preset.values) ButtonSegment(value: pr, label: Text(s.t('preset.${pr.name}')))],
          selected: {_settings.preset ?? Preset.recommended},
          onSelectionChanged: (v) => setState(() => _settings = _settings.withPreset(v.first)),
        ),
        const SizedBox(height: 8),
        ExpansionTile(
          title: Text(s.t('batch.meta')),
          tilePadding: EdgeInsets.zero,
          children: [
            for (final (n, k) in ['author', 'org', 'product', 'description'].indexed)
              Padding(
                padding: const EdgeInsets.only(bottom: 8),
                child: TextField(
                    controller: _meta[n],
                    decoration: InputDecoration(labelText: s.t('advanced.meta.$k'), hintText: s.t('advanced.meta.placeholder'))),
              ),
          ],
        ),
      ]),
      bottomNavigationBar: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: FilledButton(
            key: const Key('batchStart'),
            style: FilledButton.styleFrom(minimumSize: const Size.fromHeight(50)),
            onPressed: _reading || ready == 0 ? null : _start,
            child: Text(ready == 0 && !_reading ? s.t('batch.none_ready') : s.t('batch.start', {'n': ready})),
          ),
        ),
      ),
    );
  }
}

/// Runs the queue (PLAN §8.4). Not dismissible; stopping asks first.
class BatchRunningScreen extends StatefulWidget {
  const BatchRunningScreen({super.key, required this.inputs, required this.settings});
  final List<BatchInput> inputs;
  final ConvertSettings settings;
  @override
  State<BatchRunningScreen> createState() => _BatchRunningScreenState();
}

class _BatchRunningScreenState extends State<BatchRunningScreen> with WidgetsBindingObserver {
  late final BatchRunner _runner;
  late final AppState _state;
  int? _backgroundTask;
  bool _ended = false;

  @override
  void initState() {
    super.initState();
    final state = _state = AppScope.read(context);
    _runner = BatchRunner(
      engine: state.engine,
      inputs: widget.inputs, // unreadable files end as failed, so totals match the selection
      settings: widget.settings,
      memoryGuard: state.memoryGuard,
      budgetBytes: state.memoryBudgetBytes,
      outputPathFor: state.outputPathFor,
      isAllowed: () => state.batchLimit() == null,
      maxFacesNow: () => state.isPro ? null : state.limits.maxTriangles,
      onFileDone: (item, r) {
        final bytes = File(r.outputPath).existsSync() ? File(r.outputPath).lengthSync() : 0;
        state.recordConversion();
        state.addHistory(HistoryEntry(
          outputPath: r.outputPath,
          format: widget.settings.format,
          bytes: bytes,
          time: DateTime.now(),
          sourceName: item.input.name,
          settings: widget.settings,
        ));
      },
    )..addListener(_changed);
    WidgetsBinding.instance.addObserver(this);
    state.haptics.start();
    // One background task for the whole batch.
    state.background
        .begin('stl2stp-convert', onExpire: () => _runner.stop(BatchStopReason.background))
        .then((id) => _ended ? state.background.end(id) : _backgroundTask = id);
    WidgetsBinding.instance.addPostFrameCallback((_) => _go());
  }

  void _changed() {
    if (mounted) setState(() {});
  }

  Future<void> _go() async {
    try {
      await _runner.run();
    } catch (e) {
      debugPrint('batch run failed: $e');
    }
    _ended = true;
    final id = _backgroundTask;
    _backgroundTask = null;
    if (id != null) await _state.background.end(id);
    if (_runner.summary.done > 0) {
      _state.haptics.success();
    } else {
      _state.haptics.failure();
    }
    if (mounted) setState(() {});
  }

  @override
  void didHaveMemoryPressure() => _runner.stop(BatchStopReason.memory);

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _runner.removeListener(_changed);
    if (!_runner.finished) _runner.stop(BatchStopReason.cancelled);
    final id = _backgroundTask;
    if (id != null) _state.background.end(id);
    _runner.dispose();
    super.dispose();
  }

  Future<void> _askStop() async {
    final s = S.of(context);
    final t = Tokens.of(context);
    final yes = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(s.t('batch.cancel.title')),
        content: Text(s.t('batch.cancel.body')),
        actions: [
          TextButton(
              key: const Key('batchStopYes'),
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(s.t('batch.cancel.yes'), style: TextStyle(color: t.danger))),
          FilledButton(
              style: FilledButton.styleFrom(minimumSize: const Size(64, 40)),
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(s.t('batch.cancel.no'))),
        ],
      ),
    );
    if (yes == true) _runner.stop(BatchStopReason.cancelled);
  }

  String _reason(S s, BatchItem it) {
    switch (it.state) {
      case BatchItemState.skipped:
        return s.t(it.reasonKey ?? 'batch.state.skipped');
      case BatchItemState.notRun:
        return s.t('batch.stop.${(it.stopReason ?? BatchStopReason.cancelled).name}');
      case BatchItemState.failed:
        final key = switch (it.status) {
          CoreStatus.errorMemory => 'error.memory.title',
          CoreStatus.errorEmpty => 'error.empty.title',
          CoreStatus.errorFormat || CoreStatus.errorIo => 'error.format.title',
          _ => 'error.generic.title',
        };
        return s.t(key);
      default:
        return '';
    }
  }

  Future<void> _shareAll() async {
    final s = S.of(context);
    final messenger = ScaffoldMessenger.of(context);
    final files = [for (final i in _runner.items) if (i.state == BatchItemState.done) XFile(i.result!.outputPath)];
    if (files.isEmpty) return;
    final box = context.findRenderObject();
    final origin = box is RenderBox && box.hasSize ? box.localToGlobal(Offset.zero) & box.size : null;
    try {
      if (!(Platform.isIOS || Platform.isAndroid || Platform.isMacOS)) throw UnsupportedError('desktop');
      await SharePlus.instance.share(ShareParams(files: files, sharePositionOrigin: origin));
    } catch (_) {
      messenger.showSnackBar(SnackBar(content: Text(s.t('x.share_unavailable'))));
    }
  }

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final fin = _runner.finished;
    final sum = _runner.summary;
    final k = (_runner.currentIndex + 1).clamp(1, _runner.items.length);
    return PopScope(
      canPop: false,
      child: Scaffold(
        appBar: AppBar(title: Text(s.t('batch.title')), automaticallyImplyLeading: false),
        body: SafeArea(
          child: Column(children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 8),
              child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                Text(
                    fin
                        ? s.t('batch.summary', {
                            'done': sum.done,
                            'failed': sum.failed,
                            'skipped': sum.skipped,
                            'notRun': sum.notRun
                          })
                        : s.t('batch.progress', {'k': k, 'n': _runner.items.length}),
                    key: const Key('batchStatus'),
                    style: TextStyle(fontSize: 17, fontWeight: FontWeight.w600, color: t.label)),
                const SizedBox(height: 8),
                LinearProgressIndicator(value: _runner.overall),
              ]),
            ),
            Expanded(
              child: ListView(children: [
                for (final it in _runner.items)
                  ListTile(
                    dense: true,
                    leading: switch (it.state) {
                      BatchItemState.done => Icon(Icons.check_circle, color: t.success),
                      BatchItemState.failed => Icon(Icons.error, color: t.danger),
                      BatchItemState.skipped => Icon(Icons.skip_next, color: t.warning),
                      BatchItemState.notRun => Icon(Icons.remove_circle_outline, color: t.label3),
                      BatchItemState.running => Icon(Icons.radio_button_checked, color: t.accent),
                      BatchItemState.waiting => Icon(Icons.radio_button_unchecked, color: t.label3),
                    },
                    title: Text(it.input.name, maxLines: 1, overflow: TextOverflow.ellipsis),
                    subtitle: Text(
                        it.state == BatchItemState.running && it.stage != null
                            ? '${s.t('batch.state.running')} · ${s.t('converting.stage.${it.stage!.name}')}'
                            : [s.t('batch.state.${it.state.name}'), _reason(s, it)].where((e) => e.isNotEmpty).join(' · '),
                        style: TextStyle(color: t.label2)),
                  ),
              ]),
            ),
            Padding(
              padding: const EdgeInsets.all(16),
              child: fin
                  ? Column(children: [
                      if (sum.done > 0)
                        OutlinedButton(
                            key: const Key('batchShareAll'),
                            style: OutlinedButton.styleFrom(minimumSize: const Size.fromHeight(50)),
                            onPressed: _shareAll,
                            child: Text(s.t('batch.share_all'))),
                      const SizedBox(height: 8),
                      FilledButton(
                          key: const Key('batchFinish'),
                          style: FilledButton.styleFrom(minimumSize: const Size.fromHeight(50)),
                          onPressed: () => Navigator.of(context).popUntil((r) => r.isFirst),
                          child: Text(s.t('batch.finish'))),
                    ])
                  : SizedBox(
                      width: double.infinity,
                      child: TextButton.icon(
                        key: const Key('batchStop'),
                        style: TextButton.styleFrom(
                            foregroundColor: t.danger, backgroundColor: t.fill, minimumSize: const Size.fromHeight(50)),
                        onPressed: _askStop,
                        icon: const Icon(Icons.stop_circle_outlined),
                        label: Text(s.t('batch.cancel')),
                      ),
                    ),
            ),
          ]),
        ),
      ),
    );
  }
}
