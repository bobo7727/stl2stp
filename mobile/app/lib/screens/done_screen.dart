import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:path/path.dart' as p;

import '../core/estimate.dart';
import '../core/models.dart';
import '../l10n/strings.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';
import 'flow.dart';

/// Done screen (ui-spec §4.9).
class DoneScreen extends StatelessWidget {
  const DoneScreen({super.key, required this.request, required this.result, required this.bytes});
  final ConvertRequest request;
  final ConvertResult result;
  final int bytes;

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final fmt = request.settings.format;
    final iges = fmt == ExportFormat.iges;
    final secs = result.elapsed.inMilliseconds / 1000;
    final banners = <Widget>[
      if (iges)
        InfoBanner(kind: BannerKind.info, text: s.t('done.iges_surfaces'))
      else if (result.verified == true)
        InfoBanner(kind: BannerKind.success, text: s.t('done.verify_ok'))
      else if (result.verified == false)
        InfoBanner(kind: BannerKind.warn, text: s.t('x.verify_failed'))
      else
        InfoBanner(kind: BannerKind.neutral, text: s.t('done.verify_off')),
      if (result.warnings.contains(CoreStatus.warningTargetNotReached))
        InfoBanner(
            kind: BannerKind.warn,
            text: s.t('done.target_not_reached', {'faces': formatCount(result.facesAfterSimplify ?? 0)})),
      if (result.warnings.contains(CoreStatus.warningPartial))
        InfoBanner(kind: BannerKind.warn, text: s.t('done.partial', {'n': '?'})),
      if (result.warnings.contains(CoreStatus.warningNotSolid)) InfoBanner(kind: BannerKind.warn, text: s.t('x.not_solid')),
      if (result.warnings.contains(CoreStatus.warningRepaired)) InfoBanner(kind: BannerKind.info, text: s.t('x.repaired')),
    ];
    return Scaffold(
      appBar: AppBar(automaticallyImplyLeading: false, actions: [
        TextButton(
            key: const Key('doneButton'),
            onPressed: () => Navigator.of(context).popUntil((r) => r.isFirst),
            child: Text(s.t('common.done'))),
      ]),
      body: ListView(padding: const EdgeInsets.fromLTRB(16, 8, 16, 24), children: [
        Center(child: StatusIcon(icon: Icons.check, color: t.success)),
        const SizedBox(height: 12),
        Center(
            child: Text(s.t('done.title'),
                key: const Key('doneTitle'),
                style: TextStyle(fontSize: 28, fontWeight: FontWeight.w700, color: t.label))),
        const SizedBox(height: 20),
        Surface(
          radius: 16,
          padding: const EdgeInsets.all(16),
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Row(children: [
              FormatTag(fmt.label),
              const SizedBox(width: 12),
              Expanded(
                child: Text(p.basename(result.outputPath),
                    style: TextStyle(fontSize: 17, fontWeight: FontWeight.w600, color: t.label)),
              ),
            ]),
            const SizedBox(height: 16),
            Row(children: [
              _kv(t, s.t('done.size'), formatBytes(bytes)),
              _kv(t, s.t('done.time'), '${secs.toStringAsFixed(secs < 10 ? 1 : 0)} s'),
              _kv(t, s.t('done.solids'), iges ? '—' : '${result.topology.solids}'),
              _kv(t, s.t('done.faces'), formatCount(result.topology.faces)),
            ]),
          ]),
        ),
        const SizedBox(height: 12),
        for (final b in banners) Padding(padding: const EdgeInsets.only(bottom: 8), child: b),
        const SizedBox(height: 16),
        Builder(
          builder: (bctx) => FilledButton.icon(
            key: const Key('shareButton'),
            onPressed: () {
              final box = bctx.findRenderObject() as RenderBox?;
              final origin = box == null ? null : box.localToGlobal(Offset.zero) & box.size;
              shareFile(context, result.outputPath, origin: origin);
            },
            icon: const Icon(Icons.ios_share),
            label: Text(s.t('done.share')),
          ),
        ),
        const SizedBox(height: 10),
        FilledButton.tonalIcon(
          onPressed: () => saveToFiles(context, result.outputPath),
          icon: const Icon(Icons.folder_outlined),
          label: Text(s.t('done.save')),
        ),
        const SizedBox(height: 6),
        TextButton(
            onPressed: () => Navigator.of(context).popUntil((r) => r.isFirst), child: Text(s.t('done.another'))),
        const SizedBox(height: 8),
        Text(s.t('done.tip_pc'), textAlign: TextAlign.center, style: TextStyle(fontSize: 12, color: t.label2)),
        const SizedBox(height: 8),
        InkWell(
          onTap: () {
            Clipboard.setData(ClipboardData(text: result.outputPath));
            ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(s.t('x.copied'))));
          },
          child: Text('${s.t('x.output_path')}: ${result.outputPath}',
              textAlign: TextAlign.center, style: TextStyle(fontSize: 11, color: t.label3)),
        ),
      ]),
    );
  }

  Widget _kv(Tokens t, String k, String v) => Expanded(
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Text(k, style: TextStyle(fontSize: 12, color: t.label2)),
          const SizedBox(height: 2),
          Text(v, style: TextStyle(fontSize: 17, fontWeight: FontWeight.w600, color: t.label, fontFeatures: tabular)),
        ]),
      );
}
