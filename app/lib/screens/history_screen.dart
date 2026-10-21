import 'package:flutter/material.dart';

import '../l10n/strings.dart';
import '../state/app_state.dart';
import '../theme/tokens.dart';
import 'home_screen.dart';

/// Conversion history (ui-spec §4.12). Swipe left to delete; tap to share again.
class HistoryScreen extends StatelessWidget {
  const HistoryScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final state = AppScope.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(s.t('history.title')), actions: [
        if (state.history.isNotEmpty)
          TextButton(
            onPressed: () => confirmClearHistory(context),
            child: Text(s.t('history.clear'), style: TextStyle(color: t.danger)),
          ),
      ]),
      body: state.history.isEmpty
          ? Center(child: Text(s.t('history.empty'), style: TextStyle(color: t.label2)))
          : ListView(padding: const EdgeInsets.all(16), children: [
              ClipRRect(
                borderRadius: BorderRadius.circular(12),
                child: Column(children: [
                  for (final e in state.history)
                    Dismissible(
                      key: ValueKey(e.outputPath),
                      direction: DismissDirection.endToStart,
                      background: Container(
                        color: t.danger,
                        alignment: Alignment.centerRight,
                        padding: const EdgeInsets.only(right: 20),
                        child: Text(s.t('history.delete'), style: const TextStyle(color: Colors.white)),
                      ),
                      onDismissed: (_) => state.deleteHistory(e),
                      child: Column(children: [
                        HistoryTile(entry: e),
                        const Divider(indent: 64),
                      ]),
                    ),
                ]),
              ),
              Padding(
                padding: const EdgeInsets.all(12),
                child: Text(s.t('history.note'), textAlign: TextAlign.center, style: TextStyle(fontSize: 12, color: t.label2)),
              ),
            ]),
    );
  }
}

Future<void> confirmClearHistory(BuildContext context) async {
  final s = S.of(context);
  final state = AppScope.read(context);
  final ok = await showDialog<bool>(
    context: context,
    builder: (ctx) => AlertDialog(
      content: Text(s.t('x.clear_confirm')),
      actions: [
        TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(s.t('common.cancel'))),
        TextButton(onPressed: () => Navigator.pop(ctx, true), child: Text(s.t('history.clear'))),
      ],
    ),
  );
  if (ok == true) state.clearHistory();
}

