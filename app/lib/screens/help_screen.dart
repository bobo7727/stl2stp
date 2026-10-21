import 'package:flutter/material.dart';

import '../l10n/strings.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';

/// Help topics in display order. Each has `help.<id>.title` / `help.<id>.body`
/// in assets/i18n/app_*.json (tool/app_strings.py).
const helpTopics = ['formats', 'step_iges', 'presets', 'mode', 'merge', 'simplify', 'units', 'estimate', 'meta'];
const helpFaqCount = 6;

/// Opens the help page, optionally scrolled to [topic] (one of [helpTopics],
/// 'faq' or 'privacy').
Future<void> openHelp(BuildContext context, {String? topic}) =>
    Navigator.push(context, MaterialPageRoute(builder: (_) => HelpScreen(topic: topic)));

/// Small “?” button that opens help at a topic (used next to option groups).
class HelpButton extends StatelessWidget {
  const HelpButton({super.key, required this.topic});
  final String topic;
  @override
  Widget build(BuildContext context) {
    final t = Tokens.of(context);
    return IconButton(
      tooltip: S.of(context).t('help.open'),
      visualDensity: VisualDensity.compact,
      icon: Icon(Icons.help_outline, size: 20, color: t.label3),
      onPressed: () => openHelp(context, topic: topic),
    );
  }
}

/// Help page (zh/en): formats, STEP vs IGES, presets, modes, merging,
/// simplification, units, estimates, file info, FAQ and privacy.
class HelpScreen extends StatefulWidget {
  const HelpScreen({super.key, this.topic});
  final String? topic;
  @override
  State<HelpScreen> createState() => _HelpScreenState();
}

class _HelpScreenState extends State<HelpScreen> {
  final _keys = {for (final k in [...helpTopics, 'faq', 'privacy']) k: GlobalKey()};

  @override
  void initState() {
    super.initState();
    final topic = widget.topic;
    if (topic != null) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        final ctx = _keys[topic]?.currentContext;
        if (ctx != null) Scrollable.ensureVisible(ctx, duration: const Duration(milliseconds: 250));
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final h = TextStyle(fontSize: 17, fontWeight: FontWeight.w600, color: t.label);
    final b = TextStyle(fontSize: 14, height: 1.5, color: t.label2);
    Widget section(String id) => Padding(
          key: _keys[id],
          padding: const EdgeInsets.only(bottom: 12),
          child: Surface(
            padding: const EdgeInsets.all(14),
            child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
              Text(s.t('help.$id.title'), style: h),
              const SizedBox(height: 6),
              for (final para in s.t('help.$id.body').split('\n'))
                Padding(padding: const EdgeInsets.only(top: 4), child: Text(para, style: b)),
            ]),
          ),
        );
    return Scaffold(
      appBar: AppBar(title: Text(s.t('help.title'))),
      // Not a lazy ListView: every topic must be built so ensureVisible can reach it.
      body: SingleChildScrollView(
        padding: const EdgeInsets.fromLTRB(16, 4, 16, 32),
        child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
          Padding(
            padding: const EdgeInsets.only(bottom: 12),
            child: InfoBanner(kind: BannerKind.success, text: s.t('help.privacy.body')),
          ),
          for (final id in helpTopics) section(id),
          Padding(
            key: _keys['faq'],
            padding: const EdgeInsets.fromLTRB(4, 8, 4, 8),
            child: Text(s.t('help.faq.title'), style: h),
          ),
          Surface(
            padding: EdgeInsets.zero,
            child: Column(children: [
              for (var i = 1; i <= helpFaqCount; i++) ...[
                if (i > 1) const Divider(height: 1, indent: 16),
                ExpansionTile(
                  key: Key('faq$i'),
                  initiallyExpanded: i == 1,
                  shape: const Border(),
                  collapsedShape: const Border(),
                  title: Text(s.t('help.faq.q$i'), style: TextStyle(fontSize: 15, color: t.label)),
                  childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 12),
                  expandedAlignment: Alignment.centerLeft,
                  children: [Text(s.t('help.faq.a$i'), style: b)],
                ),
              ],
            ]),
          ),
          const SizedBox(height: 12),
          section('privacy'),
        ]),
      ),
    );
  }
}
