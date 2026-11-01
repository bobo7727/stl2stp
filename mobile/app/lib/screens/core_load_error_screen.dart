import 'package:flutter/material.dart';

import '../l10n/strings.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';

/// Shown in release when stl2stp-core failed to load or initialize.
/// There is no path from here into a conversion: the demo engine is not a fallback.
class CoreLoadErrorScreen extends StatelessWidget {
  const CoreLoadErrorScreen({super.key, this.detail = ''});

  /// Load/init failure text. Empty when the caller has no extra detail.
  final String detail;

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    return Scaffold(
      key: const Key('coreLoadError'),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(children: [
            const Spacer(),
            StatusIcon(icon: Icons.warning_amber_rounded, color: t.danger),
            const SizedBox(height: 20),
            Text(s.t('x.core_unavailable.title'),
                key: const Key('coreLoadErrorTitle'),
                textAlign: TextAlign.center,
                style: TextStyle(fontSize: 22, fontWeight: FontWeight.w700, color: t.label)),
            const SizedBox(height: 8),
            Text(s.t('x.core_unavailable.body'),
                textAlign: TextAlign.center,
                style: TextStyle(fontSize: 15, color: t.label2)),
            if (detail.isNotEmpty) ...[
              const SizedBox(height: 16),
              Text(detail,
                  key: const Key('coreLoadErrorDetail'),
                  textAlign: TextAlign.center,
                  maxLines: 6,
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(fontSize: 12, color: t.label3)),
            ],
            const Spacer(flex: 2),
          ]),
        ),
      ),
    );
  }
}
