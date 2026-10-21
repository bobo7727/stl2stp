import 'package:flutter/material.dart';

import '../core/memory_guard.dart';
import '../core/models.dart';
import '../l10n/strings.dart';
import '../state/app_state.dart';
import '../theme/tokens.dart';
import '../core/estimate.dart' show formatBytes;

/// Memory pre-check before converting. Returns the settings to convert with
/// (possibly with simplification switched on), or null to stay on the preview.
Future<ConvertSettings?> ensureMemory(BuildContext context, int faces, ConvertSettings settings) async {
  final state = AppScope.read(context);
  final check = state.memoryGuard.check(faces, settings, state.memoryBudgetBytes);
  if (check.level == MemoryLevel.ok) return settings;
  final s = S.of(context);
  final t = Tokens.of(context);
  final refuse = check.level == MemoryLevel.refuse;
  final suggest = check.suggestPercent;
  final params = {
    'need': formatBytes(check.needBytes),
    'budget': formatBytes(check.budgetBytes),
    'percent': suggest ?? 5,
  };
  final choice = await showDialog<String>(
    context: context,
    builder: (ctx) => AlertDialog(
      key: const Key('memoryDialog'),
      icon: Icon(Icons.memory, color: refuse ? t.danger : t.warning),
      title: Text(s.t(refuse ? 'x.memory.refuse.title' : 'x.memory.warn.title')),
      content: Text(s.t(refuse ? 'x.memory.refuse.body' : 'x.memory.warn.body', params)),
      actions: [
        TextButton(onPressed: () => Navigator.pop(ctx, 'cancel'), child: Text(s.t('common.cancel'))),
        if (!refuse)
          TextButton(
              key: const Key('memoryContinue'),
              onPressed: () => Navigator.pop(ctx, 'continue'),
              child: Text(s.t('x.memory.continue'))),
        if (suggest != null)
          FilledButton(
              key: const Key('memorySimplify'),
              style: FilledButton.styleFrom(minimumSize: const Size(64, 40)),
              onPressed: () => Navigator.pop(ctx, 'simplify'),
              child: Text(s.t('x.memory.simplify', params))),
      ],
    ),
  );
  return switch (choice) {
    'continue' => settings,
    'simplify' => settings.copyWith(simplify: true, keepPercent: suggest),
    _ => null,
  };
}
