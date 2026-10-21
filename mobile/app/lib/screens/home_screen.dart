import 'dart:io';

import 'package:flutter/material.dart';
import 'package:path/path.dart' as p;

import '../core/sample_mesh.dart';
import '../l10n/strings.dart';
import '../state/app_state.dart';
import '../state/history.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';
import '../widgets/history_list.dart';
import '../widgets/stl_drop.dart';
import 'flow.dart';
import 'help_screen.dart';
import 'history_screen.dart';
import 'settings_screen.dart';

/// Split layout (ui-spec §4.16) from this width, or on tablets (shortest side
/// ≥ 600) from [splitTabletWidth]; narrower windows (iPhone, Split View /
/// Slide Over) fall back to the phone layout.
const splitMinWidth = 900.0;
const splitTabletWidth = 700.0;

/// Width of the left (home) column in the split layout.
const splitLeftWidth = 340.0;

bool useSplitLayout(Size size) =>
    size.width >= splitMinWidth || (size.shortestSide >= 600 && size.width >= splitTabletWidth);

/// Home (ui-spec §4.3): import card + recent conversions. On wide screens
/// (iPad) a two-pane layout: home + full history on the left, preview /
/// converting / result in a nested navigator on the right (§4.16).
class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.sizeOf(context);
    if (useSplitLayout(size)) return _SplitHome(size: size);
    return _phone(context);
  }

  Widget _phone(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final state = AppScope.of(context);
    final recent = state.history.take(3).toList();
    return Scaffold(
      appBar: AppBar(actions: [
        IconButton(
          key: const Key('helpButton'),
          tooltip: s.t('help.open'),
          icon: Icon(Icons.help_outline, color: t.accent),
          onPressed: () => openHelp(context),
        ),
        IconButton(
          key: const Key('settingsButton'),
          tooltip: s.t('home.settings'),
          icon: Icon(Icons.settings_outlined, color: t.accent),
          onPressed: () => Navigator.push(context, MaterialPageRoute(builder: (_) => const SettingsScreen())),
        ),
      ]),
      body: StlDropTarget(
        onFile: (path) => openModel(context, path),
        child: ListView(
        padding: const EdgeInsets.fromLTRB(16, 0, 16, 24),
        children: [
          Text(s.t('home.title'), style: TextStyle(fontSize: 34, fontWeight: FontWeight.w700, color: t.label)),
          Text(s.t('app.tagline'), style: TextStyle(fontSize: 15, color: t.label2)),
          const SizedBox(height: 16),
          if (!state.engine.isReal) ...[
            InfoBanner(kind: BannerKind.warn, text: s.t('x.demo_banner')),
            const SizedBox(height: 12),
          ],
          _ImportCard(onTap: () async {
            final path = await pickStl(context);
            if (path != null && context.mounted) await openModel(context, path);
          }),
          const SizedBox(height: 8),
          Center(
            child: TextButton.icon(
              icon: const Icon(Icons.chat_bubble_outline, size: 18),
              label: Text(s.t('home.other_ways'), textAlign: TextAlign.center),
              onPressed: () => _showImportHelp(context),
            ),
          ),
          if (Platform.isLinux || Platform.isWindows || Platform.isMacOS)
            Center(
              child: TextButton(
                key: const Key('sampleButton'),
                onPressed: () async {
                  final path = await writeSampleStl(
                      p.join(Directory.systemTemp.path, s.isZh ? '示例_支架.stl' : 'sample_bracket.stl'));
                  if (context.mounted) await openModel(context, path);
                },
                child: Text(s.t('x.sample')),
              ),
            ),
          SectionHeader(
            s.t('home.recent'),
            trailing: recent.isEmpty
                ? null
                : TextButton(
                    onPressed: () => Navigator.push(context, MaterialPageRoute(builder: (_) => const HistoryScreen())),
                    child: Text(s.t('home.see_all'))),
          ),
          if (recent.isEmpty)
            Surface(
              padding: const EdgeInsets.symmetric(vertical: 28, horizontal: 16),
              child: Column(children: [
                Icon(Icons.history, size: 40, color: t.label3),
                const SizedBox(height: 8),
                Text(s.t('home.empty.title'), style: TextStyle(fontSize: 17, fontWeight: FontWeight.w600, color: t.label)),
                const SizedBox(height: 4),
                Text(s.t('home.empty.body'), textAlign: TextAlign.center, style: TextStyle(fontSize: 13, color: t.label2)),
              ]),
            )
          else
            Surface(padding: EdgeInsets.zero, child: HistoryList(entries: recent)),
        ],
      ),
      ),
    );
  }

  static void showImportHelp(BuildContext context) => const HomeScreen()._showImportHelp(context);

  void _showImportHelp(BuildContext context) {
    final s = S.of(context);
    showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      builder: (ctx) {
        final t = Tokens.of(ctx);
        return SafeArea(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(20, 0, 20, 20),
            child: Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.start, children: [
              Row(children: [
                Expanded(
                    child: Text(s.t('import.help.title'),
                        style: const TextStyle(fontSize: 20, fontWeight: FontWeight.w600))),
                TextButton(onPressed: () => Navigator.pop(ctx), child: Text(s.t('common.done'))),
              ]),
              const SizedBox(height: 8),
              for (var i = 1; i <= 3; i++)
                Padding(
                  padding: const EdgeInsets.symmetric(vertical: 6),
                  child: Row(children: [
                    CircleAvatar(radius: 13, backgroundColor: t.accent, child: Text('$i', style: const TextStyle(color: Colors.white, fontSize: 13))),
                    const SizedBox(width: 12),
                    Expanded(child: Text(s.t('import.help.step$i'), style: const TextStyle(fontSize: 16))),
                  ]),
                ),
              const SizedBox(height: 8),
              Text(s.t('import.help.files'), style: TextStyle(fontSize: 13, color: t.label2)),
            ]),
          ),
        );
      },
    );
  }
}

class _ImportCard extends StatelessWidget {
  const _ImportCard({required this.onTap});
  final VoidCallback onTap;
  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    return Material(
      color: t.surface,
      borderRadius: BorderRadius.circular(22),
      child: InkWell(
        key: const Key('importCard'),
        borderRadius: BorderRadius.circular(22),
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.symmetric(vertical: 28, horizontal: 16),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(22),
            border: Border.all(color: t.accent.withValues(alpha: 0.35), width: 1.5),
          ),
          child: Column(children: [
            Container(
              width: 56,
              height: 56,
              decoration: BoxDecoration(color: t.accent, borderRadius: BorderRadius.circular(14)),
              child: const Icon(Icons.file_download_outlined, color: Colors.white, size: 30),
            ),
            const SizedBox(height: 12),
            Text(s.t('home.import'), style: TextStyle(fontSize: 20, fontWeight: FontWeight.w600, color: t.label)),
            const SizedBox(height: 4),
            Text(s.t('home.import_hint'), style: TextStyle(fontSize: 13, color: t.label2)),
          ]),
        ),
      ),
    );
  }
}

class HistoryTile extends StatelessWidget {
  const HistoryTile({super.key, required this.entry});
  final HistoryEntry entry;
  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final exists = entry.exists;
    return ListTile(
      leading: Opacity(opacity: exists ? 1 : 0.4, child: FormatTag(entry.format.label)),
      title: Text(entry.fileName,
          maxLines: 1, overflow: TextOverflow.ellipsis, style: TextStyle(color: exists ? t.label : t.label3)),
      subtitle: Text(exists ? historySubtitle(context, entry) : s.t('history.missing'),
          style: TextStyle(fontSize: 13, color: t.label2)),
      trailing: Icon(Icons.chevron_right, color: t.label3),
      onTap: () => shareHistoryEntry(context, entry),
    );
  }
}

/// iPad / wide-window layout: left column = home content + full history
/// (swipe to share/delete), right = nested navigator for preview → converting
/// → done/error. [openModel] targets the right pane via [detailNavigatorKey].
class _SplitHome extends StatelessWidget {
  const _SplitHome({required this.size});
  final Size size;

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final state = AppScope.of(context);
    final mq = MediaQuery.of(context);
    final rightWidth = size.width - splitLeftWidth - 1;
    final left = Scaffold(
      key: const Key('splitLeft'),
      appBar: AppBar(actions: [
        IconButton(
          key: const Key('helpButton'),
          tooltip: s.t('help.open'),
          icon: Icon(Icons.help_outline, color: t.accent),
          onPressed: () => openHelp(context),
        ),
        IconButton(
          key: const Key('settingsButton'),
          tooltip: s.t('home.settings'),
          icon: Icon(Icons.settings_outlined, color: t.accent),
          onPressed: () => Navigator.push(context, MaterialPageRoute(builder: (_) => const SettingsScreen())),
        ),
      ]),
      body: ListView(padding: const EdgeInsets.fromLTRB(16, 0, 16, 24), children: [
        Text(s.t('home.title'), style: TextStyle(fontSize: 30, fontWeight: FontWeight.w700, color: t.label)),
        Text(s.t('app.tagline'), style: TextStyle(fontSize: 14, color: t.label2)),
        const SizedBox(height: 16),
        if (!state.engine.isReal) ...[
          InfoBanner(kind: BannerKind.warn, text: s.t('x.demo_banner')),
          const SizedBox(height: 12),
        ],
        _ImportCard(onTap: () async {
          final path = await pickStl(context);
          if (path != null && context.mounted) await openModel(context, path);
        }),
        const SizedBox(height: 4),
        Center(
          child: TextButton.icon(
            icon: const Icon(Icons.chat_bubble_outline, size: 18),
            label: Text(s.t('home.other_ways'), textAlign: TextAlign.center),
            onPressed: () => HomeScreen.showImportHelp(context),
          ),
        ),
        if (Platform.isLinux || Platform.isWindows || Platform.isMacOS)
          Center(
            child: TextButton(
              key: const Key('sampleButton'),
              onPressed: () async {
                final path = await writeSampleStl(
                    p.join(Directory.systemTemp.path, s.isZh ? '示例_支架.stl' : 'sample_bracket.stl'));
                if (context.mounted) await openModel(context, path);
              },
              child: Text(s.t('x.sample')),
            ),
          ),
        SectionHeader(s.t('history.title')),
        if (state.history.isEmpty)
          Surface(
            padding: const EdgeInsets.symmetric(vertical: 24, horizontal: 16),
            child: Column(children: [
              Icon(Icons.history, size: 36, color: t.label3),
              const SizedBox(height: 8),
              Text(s.t('home.empty.title'), style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600, color: t.label)),
            ]),
          )
        else ...[
          Surface(padding: EdgeInsets.zero, child: HistoryList(entries: state.history)),
          Padding(
            padding: const EdgeInsets.all(8),
            child: Text(s.t('x.history.swipe_hint'),
                textAlign: TextAlign.center, style: TextStyle(fontSize: 12, color: t.label2)),
          ),
        ],
      ]),
    );
    return StlDropTarget(
      onFile: (path) => openModel(context, path),
      child: Row(children: [
        SizedBox(width: splitLeftWidth, child: left),
        VerticalDivider(width: 1, thickness: 1, color: t.separator),
        Expanded(
          // Screens in the right pane lay out for the pane's width, not the window's.
          child: MediaQuery(
            data: mq.copyWith(size: Size(rightWidth, size.height)),
            // Own messenger: snack bars raised in the right pane show only there,
            // and the left column's do not appear twice.
            child: ScaffoldMessenger(
              child: HeroControllerScope.none(
                child: Navigator(
                  key: detailNavigatorKey,
                  onGenerateInitialRoutes: (_, _) => [MaterialPageRoute<void>(builder: (_) => const _DetailPlaceholder())],
                ),
              ),
            ),
          ),
        ),
      ]),
    );
  }
}

class _DetailPlaceholder extends StatelessWidget {
  const _DetailPlaceholder();
  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    return Scaffold(
      key: const Key('detailPlaceholder'),
      body: Center(
        child: Column(mainAxisSize: MainAxisSize.min, children: [
          Icon(Icons.view_in_ar_outlined, size: 64, color: t.label3),
          const SizedBox(height: 12),
          Text(s.t('x.split.placeholder'), style: TextStyle(fontSize: 17, color: t.label2)),
        ]),
      ),
    );
  }
}
