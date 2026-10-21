import 'package:flutter/material.dart';

import '../l10n/strings.dart';
import '../screens/flow.dart';
import '../screens/home_screen.dart';
import '../state/app_state.dart';
import '../state/history.dart';
import '../theme/tokens.dart';

/// Swipe actions on a history row (ui-spec §4.12):
/// swipe right → share the output again (row stays), swipe left → delete
/// (removes the record and the output file). A missing output file gets a
/// clear message instead of a share sheet, with an action to remove the record.
class HistoryRow extends StatelessWidget {
  const HistoryRow({super.key, required this.entry, this.showDivider = true});
  final HistoryEntry entry;
  final bool showDivider;

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final state = AppScope.of(context);
    Widget label(IconData icon, String text, Alignment align) => Container(
          alignment: align,
          padding: const EdgeInsets.symmetric(horizontal: 20),
          child: Row(mainAxisSize: MainAxisSize.min, children: [
            Icon(icon, color: Colors.white, size: 20),
            const SizedBox(width: 6),
            Text(text, style: const TextStyle(color: Colors.white, fontWeight: FontWeight.w600)),
          ]),
        );
    return Dismissible(
      key: ValueKey('history:${entry.outputPath}'),
      background: Container(color: t.accent, child: label(Icons.ios_share, s.t('x.history.share'), Alignment.centerLeft)),
      secondaryBackground:
          Container(color: t.danger, child: label(Icons.delete_outline, s.t('history.delete'), Alignment.centerRight)),
      confirmDismiss: (direction) async {
        if (direction == DismissDirection.startToEnd) {
          await shareHistoryEntry(context, entry);
          return false; // sharing keeps the row
        }
        return true;
      },
      onDismissed: (_) => state.deleteHistory(entry),
      child: Column(children: [
        HistoryTile(entry: entry),
        if (showDivider) const Divider(indent: 64),
      ]),
    );
  }
}

/// Shares [entry]'s output file again, or explains that it no longer exists.
Future<void> shareHistoryEntry(BuildContext context, HistoryEntry entry) async {
  final s = S.of(context);
  if (!entry.exists) {
    final state = AppScope.read(context);
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(
      key: const Key('historyMissingSnack'),
      content: Text(s.t('x.history.missing_file', {'name': entry.fileName})),
      action: SnackBarAction(
          label: s.t('x.history.remove'), onPressed: () => state.deleteHistory(entry, deleteFile: false)),
    ));
    return;
  }
  await shareFile(context, entry.outputPath);
}

/// The full history as swipeable rows inside a rounded card.
class HistoryList extends StatelessWidget {
  const HistoryList({super.key, required this.entries});
  final List<HistoryEntry> entries;

  @override
  Widget build(BuildContext context) {
    return ClipRRect(
      borderRadius: BorderRadius.circular(12),
      child: Column(children: [
        for (final (i, e) in entries.indexed) HistoryRow(entry: e, showDivider: i < entries.length - 1),
      ]),
    );
  }
}
