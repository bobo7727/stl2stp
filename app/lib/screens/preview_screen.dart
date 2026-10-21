import 'package:flutter/material.dart';

import '../core/estimate.dart';
import '../core/models.dart';
import '../l10n/strings.dart';
import '../state/app_state.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';
import '../widgets/mesh_viewer.dart';
import 'converting_screen.dart';
import 'flow.dart';

/// Preview + conversion settings (ui-spec §4.5/§4.6).
class PreviewScreen extends StatefulWidget {
  const PreviewScreen({super.key, required this.model, this.forceSimplifyPercent, this.initialSettings});
  final LoadedModel model;

  /// Set when coming from "too big" / "out of memory" (simplify forced on).
  final int? forceSimplifyPercent;
  final ConvertSettings? initialSettings;

  @override
  State<PreviewScreen> createState() => _PreviewScreenState();
}

class _PreviewScreenState extends State<PreviewScreen> {
  late ConvertSettings _s;
  bool _advanced = false;
  bool _wire = false;
  final _viewer = MeshViewerController();
  late final List<TextEditingController> _meta;

  @override
  void initState() {
    super.initState();
    _s = widget.initialSettings ?? AppScope.read(context).initialSettings();
    if (widget.forceSimplifyPercent != null) {
      _s = _s.copyWith(simplify: true, keepPercent: widget.forceSimplifyPercent);
      _advanced = true;
    }
    _meta = [
      TextEditingController(text: _s.author),
      TextEditingController(text: _s.organization),
      TextEditingController(text: _s.product),
      TextEditingController(text: _s.description),
    ];
  }

  @override
  void dispose() {
    for (final c in _meta) {
      c.dispose();
    }
    _viewer.dispose();
    super.dispose();
  }

  MeshInfo get info => widget.model.info;

  void _set(ConvertSettings s) => setState(() => _s = s);

  void _syncMeta() => _s = _s.copyWith(
      author: _meta[0].text, organization: _meta[1].text, product: _meta[2].text, description: _meta[3].text);

  Future<void> _convert() async {
    _syncMeta();
    final state = AppScope.read(context);
    state.rememberSettings(_s);
    final req = ConvertRequest(
      inputPath: info.path,
      outputPath: state.outputPathFor(info.path, _s.format),
      settings: _s,
      faces: info.faces,
    );
    await Navigator.push(
        context, MaterialPageRoute(builder: (_) => ConvertingScreen(request: req, model: widget.model)));
  }

  String _time(S s, double secs) {
    if (secs < 1) return s.t('estimate.under_1s');
    if (secs < 90) return s.t('estimate.seconds', {'n': secs.round()});
    return s.t('estimate.minutes', {'n': (secs / 60).round()});
  }

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final wide = MediaQuery.sizeOf(context).width >= 900;
    final est = estimate(info.faces, _s);
    final viewer = _viewerCard(s, t, wide ? 420 : 250);
    final stats = _statsSection(s, t);
    final options = _optionsSection(s, t);
    return Scaffold(
      appBar: AppBar(
        title: Text(info.fileName, overflow: TextOverflow.ellipsis),
        actions: [
          IconButton(
            tooltip: s.t('preview.replace'),
            icon: Icon(Icons.description_outlined, color: t.accent),
            onPressed: () async {
              final path = await pickStl(context);
              if (path != null && context.mounted) await openModel(context, path, replace: true);
            },
          ),
        ],
      ),
      body: wide
          ? Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
              Expanded(
                  flex: 5,
                  child: ListView(padding: const EdgeInsets.all(16), children: [viewer, const SizedBox(height: 12), ...stats])),
              Expanded(flex: 4, child: ListView(padding: const EdgeInsets.fromLTRB(0, 0, 16, 16), children: options)),
            ])
          : ListView(
              padding: const EdgeInsets.fromLTRB(16, 4, 16, 16),
              children: [viewer, const SizedBox(height: 12), ...stats, ...options],
            ),
      bottomNavigationBar: SafeArea(
        child: Container(
          decoration: BoxDecoration(color: t.bg, border: Border(top: BorderSide(color: t.separator, width: 0.5))),
          padding: const EdgeInsets.fromLTRB(16, 8, 16, 8),
          child: Column(mainAxisSize: MainAxisSize.min, children: [
            Row(children: [
              Expanded(
                child: Text(
                  s.t('estimate.line', {'time': _time(s, est.seconds), 'size': formatBytes(est.bytes)}),
                  style: TextStyle(fontSize: 12, color: t.label2, fontFeatures: tabular),
                ),
              ),
              Text(s.t('estimate.note'), style: TextStyle(fontSize: 11, color: t.label3)),
            ]),
            const SizedBox(height: 8),
            FilledButton.icon(
              key: const Key('convertButton'),
              onPressed: _convert,
              icon: const Icon(Icons.bolt),
              label: Text(s.t('convert.button', {'fmt': _s.format.label})),
            ),
          ]),
        ),
      ),
    );
  }

  Widget _viewerCard(S s, Tokens t, double height) {
    final preview = widget.model.preview;
    return Semantics(
      label: s.t('a11y.viewer', {'name': info.fileName}),
      child: ClipRRect(
        borderRadius: BorderRadius.circular(16),
        child: Container(
          height: height,
          decoration: BoxDecoration(
            gradient: LinearGradient(
                begin: Alignment.topCenter, end: Alignment.bottomCenter, colors: [t.viewerTop, t.viewerBottom]),
          ),
          child: Stack(children: [
            if (preview != null)
              Positioned.fill(child: MeshViewer(positions: preview.positions, wireframe: _wire, controller: _viewer)),
            Positioned(
              left: 10,
              top: 10,
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                decoration: BoxDecoration(color: t.surface.withValues(alpha: 0.85), borderRadius: BorderRadius.circular(8)),
                child: Text(info.isAscii ? s.t('preview.format.ascii') : s.t('preview.format.binary'),
                    style: TextStyle(fontSize: 12, color: t.label2)),
              ),
            ),
            Positioned(
              right: 8,
              top: 8,
              child: Column(children: [
                _floatBtn(t, Icons.restart_alt, s.t('preview.reset_view'), _viewer.reset, false),
                const SizedBox(height: 8),
                _floatBtn(t, Icons.grid_on, s.t('preview.wireframe'), () => setState(() => _wire = !_wire), _wire),
              ]),
            ),
            Positioned(
              left: 0,
              right: 0,
              bottom: 8,
              child: IgnorePointer(
                child: Text(s.t('preview.gesture_hint'),
                    textAlign: TextAlign.center, style: TextStyle(fontSize: 12, color: t.label2)),
              ),
            ),
          ]),
        ),
      ),
    );
  }

  Widget _floatBtn(Tokens t, IconData icon, String tip, VoidCallback onTap, bool active) => Material(
        color: active ? t.accent : t.surface.withValues(alpha: 0.9),
        shape: const CircleBorder(),
        child: IconButton(
          tooltip: tip,
          iconSize: 20,
          constraints: const BoxConstraints.tightFor(width: 40, height: 40),
          icon: Icon(icon, color: active ? Colors.white : t.label),
          onPressed: onTap,
        ),
      );

  List<Widget> _statsSection(S s, Tokens t) {
    final sz = info.size;
    return [
      StatGrid(cells: [
        StatCell(label: s.t('preview.stats.triangles'), value: formatCount(info.faces)),
        StatCell(label: s.t('preview.stats.vertices'), value: formatCount(info.vertices)),
        StatCell(label: s.t('preview.stats.size'), value: sz.map(formatDim).join(' × ')),
        StatCell(label: s.t('preview.stats.file_size'), value: formatBytes(info.fileSize)),
      ]),
      Padding(
        padding: const EdgeInsets.fromLTRB(4, 6, 4, 0),
        child: Text(s.t('preview.stats.unit_note'), style: TextStyle(fontSize: 12, color: t.label2)),
      ),
      if (info.faces > AppState.largeFaces && !_s.simplify) ...[
        const SizedBox(height: 12),
        InfoBanner(
          kind: BannerKind.warn,
          text: s.t('warn.large.body', {
            'faces': formatCount(info.faces),
            'size': formatBytes(estimate(info.faces, const ConvertSettings()).bytes),
            'time': _time(s, estimate(info.faces, const ConvertSettings()).seconds),
          }),
          action: s.t('warn.large.action'),
          onAction: () => setState(() {
            _s = _s.copyWith(simplify: true, keepPercent: 50);
            _advanced = true;
          }),
        ),
      ],
    ];
  }

  List<Widget> _optionsSection(S s, Tokens t) {
    final preset = _s.preset;
    final iges = _s.format == ExportFormat.iges;
    return [
      SectionHeader(s.t('export.format')),
      SizedBox(
        width: double.infinity,
        child: SegmentedButton<ExportFormat>(
          key: const Key('formatSegment'),
          showSelectedIcon: false,
          segments: [
            ButtonSegment(value: ExportFormat.step, label: Text(s.t('export.format.step'))),
            ButtonSegment(value: ExportFormat.iges, label: Text(s.t('export.format.iges'))),
          ],
          selected: {_s.format},
          onSelectionChanged: (v) => _set(_s.copyWith(format: v.first)),
        ),
      ),
      Padding(
        padding: const EdgeInsets.fromLTRB(4, 6, 4, 0),
        child: Text(s.t(iges ? 'export.format.iges.desc' : 'export.format.step.desc'),
            style: TextStyle(fontSize: 13, color: t.label2)),
      ),
      if (iges) ...[
        const SizedBox(height: 8),
        InfoBanner(kind: BannerKind.info, text: s.t('warn.iges.surfaces')),
      ],
      SectionHeader(
        s.t('preset.title'),
        trailing: preset == null
            ? Container(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
                decoration: BoxDecoration(color: t.fill, borderRadius: BorderRadius.circular(8)),
                child: Text(s.t('preset.custom'), style: TextStyle(fontSize: 12, color: t.label2)),
              )
            : null,
      ),
      Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
        for (final (i, p) in Preset.values.indexed) ...[
          if (i > 0) const SizedBox(width: 8),
          Expanded(child: _presetCard(s, t, p, preset == p)),
        ],
      ]),
      const SizedBox(height: 12),
      Surface(
        padding: EdgeInsets.zero,
        child: Column(children: [
          InkWell(
            key: const Key('advancedToggle'),
            borderRadius: BorderRadius.circular(12),
            onTap: () => setState(() => _advanced = !_advanced),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
              child: Row(children: [
                AnimatedRotation(
                    turns: _advanced ? 0.25 : 0,
                    duration: const Duration(milliseconds: 150),
                    child: Icon(Icons.chevron_right, color: t.label2)),
                const SizedBox(width: 6),
                Expanded(child: Text(s.t('advanced.title'), style: TextStyle(fontSize: 17, color: t.label))),
                Text(preset == null ? s.t('preset.custom') : s.t('preset.${preset.name}'),
                    style: TextStyle(fontSize: 15, color: t.label2)),
              ]),
            ),
          ),
          if (_advanced) _advancedBody(s, t),
        ]),
      ),
    ];
  }

  Widget _presetCard(S s, Tokens t, Preset p, bool selected) {
    final icon = switch (p) {
      Preset.recommended => Icons.star_outline,
      Preset.smaller => Icons.compress,
      Preset.strict => Icons.verified_user_outlined,
    };
    return Semantics(
      selected: selected,
      button: true,
      child: Material(
        color: t.surface,
        borderRadius: BorderRadius.circular(12),
        child: InkWell(
          key: Key('preset_${p.name}'),
          borderRadius: BorderRadius.circular(12),
          onTap: () => _set(_s.withPreset(p)),
          child: Container(
            constraints: const BoxConstraints(minHeight: 104),
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: selected ? t.accent : t.separator, width: selected ? 2 : 0.5),
            ),
            child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
              Row(children: [
                Icon(icon, size: 18, color: selected ? t.accent : t.label2),
                const SizedBox(width: 4),
                Flexible(
                  child: Text(s.t('preset.${p.name}'),
                      style: TextStyle(fontSize: 15, fontWeight: FontWeight.w600, color: selected ? t.accent : t.label)),
                ),
              ]),
              const SizedBox(height: 4),
              Text(s.t('preset.${p.name}.desc'), style: TextStyle(fontSize: 12, color: t.label2)),
            ]),
          ),
        ),
      ),
    );
  }

  Widget _advancedBody(S s, Tokens t) {
    final iges = _s.format == ExportFormat.iges;
    final small = TextStyle(fontSize: 12, color: t.label2);
    Widget group(String title) => Padding(
          padding: const EdgeInsets.fromLTRB(12, 14, 12, 4),
          child: Align(alignment: Alignment.centerLeft, child: Text(title, style: TextStyle(fontSize: 13, color: t.label2))),
        );
    return Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
      const Divider(),
      group(s.t('advanced.simplify')),
      SwitchListTile(
        key: const Key('simplifySwitch'),
        title: Text(s.t('advanced.simplify')),
        value: _s.simplify,
        onChanged: (v) => _set(_s.copyWith(simplify: v)),
      ),
      if (_s.simplify) ...[
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 16),
          child: Text(
              s.t('advanced.simplify.keep',
                  {'pct': _s.keepPercent, 'faces': formatCount(_s.targetFaces(info.faces))}),
              style: TextStyle(fontSize: 15, color: t.label, fontFeatures: tabular)),
        ),
        Slider(
          value: _s.keepPercent.toDouble(),
          min: 5,
          max: 95,
          divisions: 18,
          label: '${_s.keepPercent}%',
          onChanged: (v) => _set(_s.copyWith(keepPercent: v.round())),
        ),
      ],
      Padding(padding: const EdgeInsets.symmetric(horizontal: 16), child: Text(s.t('advanced.simplify.desc'), style: small)),
      group(s.t('advanced.mode')),
      Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12),
        child: SegmentedButton<ConvertMode>(
          showSelectedIcon: false,
          segments: [
            ButtonSegment(value: ConvertMode.original, label: Text(s.t('advanced.mode.original'))),
            ButtonSegment(value: ConvertMode.sewing, label: Text(s.t('advanced.mode.sewing'))),
          ],
          selected: {_s.mode},
          onSelectionChanged: (v) => _set(_s.copyWith(mode: v.first)),
        ),
      ),
      Padding(
        padding: const EdgeInsets.fromLTRB(16, 6, 16, 0),
        child: Text(s.t('advanced.mode.${_s.mode.name}.desc'), style: small),
      ),
      SwitchListTile(
        title: Text(s.t('advanced.merge')),
        subtitle: Text(s.t('advanced.merge.desc'), style: small),
        value: _s.mergeCoplanar,
        onChanged: (v) => _set(_s.copyWith(mergeCoplanar: v)),
      ),
      SwitchListTile(
        title: Text(s.t('advanced.verify')),
        subtitle: Text(iges ? s.t('advanced.verify.iges_na') : s.t('advanced.verify.desc'), style: small),
        value: _s.verify && !iges,
        onChanged: iges ? null : (v) => _set(_s.copyWith(verify: v)),
      ),
      group(s.t('advanced.meta')),
      for (final (i, key) in ['author', 'org', 'product', 'description'].indexed)
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
          child: TextField(
            controller: _meta[i],
            maxLength: 200,
            onChanged: (_) => setState(_syncMeta),
            decoration: InputDecoration(
              labelText: s.t('advanced.meta.$key'),
              hintText: s.t('advanced.meta.placeholder'),
              counterText: '',
              isDense: true,
              border: const OutlineInputBorder(),
            ),
          ),
        ),
      if (iges && _s.hasNonAsciiMeta)
        Padding(
          padding: const EdgeInsets.fromLTRB(12, 4, 12, 0),
          child: InfoBanner(kind: BannerKind.warn, text: s.t('advanced.meta.iges_warn')),
        ),
      Padding(padding: const EdgeInsets.fromLTRB(16, 6, 16, 0), child: Text(s.t('advanced.meta.desc'), style: small)),
      Align(
        alignment: Alignment.centerLeft,
        child: Padding(
          padding: const EdgeInsets.all(4),
          child: TextButton(
            onPressed: () => setState(() {
              for (final c in _meta) {
                c.clear();
              }
              _s = ConvertSettings.forPreset(Preset.recommended, format: _s.format);
            }),
            child: Text(s.t('advanced.reset')),
          ),
        ),
      ),
    ]);
  }
}
