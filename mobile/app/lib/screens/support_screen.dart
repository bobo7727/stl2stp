import 'dart:async';

import 'package:flutter/material.dart';

import '../l10n/strings.dart';
import '../purchase/tip_service.dart';
import '../state/app_state.dart';
import '../theme/tokens.dart';

/// Settings → Support Development. A voluntary, repeatable tip: it never
/// unlocks anything and is not a paywall. Prices come only from the store.
class SupportScreen extends StatefulWidget {
  const SupportScreen({super.key});

  @override
  State<SupportScreen> createState() => _SupportScreenState();
}

class _SupportScreenState extends State<SupportScreen> {
  bool _busy = false;
  StreamSubscription<TipOutcome>? _sub;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      final tips = AppScope.read(context).tips;
      _sub = tips.background.listen((o) {
        if (mounted) _say(o);
      });
      if (tips.loadState == TipLoad.idle || tips.loadState == TipLoad.unavailable) tips.loadProducts();
    });
  }

  @override
  void dispose() {
    _sub?.cancel();
    super.dispose();
  }

  void _say(TipOutcome o, [String? message]) {
    final s = S.of(context);
    final msg = switch (o) {
      TipOutcome.thanks => s.t('support.thanks'),
      TipOutcome.pending => s.t('support.pending'),
      TipOutcome.unverified => s.t('support.unverified'),
      TipOutcome.cancelled => s.t('support.cancelled'),
      TipOutcome.unavailable => s.t('support.unavailable'),
      TipOutcome.failed => s.t('support.failed', {'msg': message ?? ''}),
      TipOutcome.busy => null,
    };
    if (msg != null) ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
  }

  Future<void> _buy(TipService tips, TipProduct p) async {
    if (_busy) return;
    setState(() => _busy = true);
    TipResult r;
    try {
      r = await tips.buyTip(p.id);
    } catch (e) {
      r = TipResult(TipOutcome.failed, '$e');
    }
    if (!mounted) return;
    setState(() => _busy = false);
    _say(r.outcome, r.message);
  }

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final tips = AppScope.of(context).tips;
    return Scaffold(
      appBar: AppBar(title: Text(s.t('support.title'))),
      body: ListenableBuilder(
        listenable: tips,
        builder: (context, _) => ListView(padding: const EdgeInsets.all(16), children: [
          Text(s.t('support.body'), key: const Key('supportBody'), style: TextStyle(fontSize: 15, color: t.label)),
          const SizedBox(height: 24),
          if (tips.loadState == TipLoad.ready)
            for (final p in tips.products)
              Padding(
                padding: const EdgeInsets.only(bottom: 8),
                child: FilledButton(
                  key: Key('tipButton.${p.id}'),
                  onPressed: _busy ? null : () => _buy(tips, p),
                  child: Text(s.t('support.button', {'price': p.priceLabel})),
                ),
              )
          else if (tips.loadState == TipLoad.loading || tips.loadState == TipLoad.idle)
            Center(child: Text(s.t('support.loading'), style: TextStyle(color: t.label2)))
          else ...[
            Text(s.t('support.unavailable'), key: const Key('tipUnavailable'), style: TextStyle(color: t.label2)),
            TextButton(key: const Key('tipRetry'), onPressed: tips.loadProducts, child: Text(s.t('support.retry'))),
          ],
        ]),
      ),
    );
  }
}
