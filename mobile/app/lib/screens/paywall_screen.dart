import 'package:flutter/material.dart';

import '../core/estimate.dart';
import '../core/models.dart';
import '../l10n/strings.dart';
import '../purchase/purchase.dart';
import '../state/app_state.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';

/// Shows a purchase result as a snackbar. Returns true if Pro is now unlocked.
bool showPurchaseResult(BuildContext context, PurchaseResult r) {
  final s = S.of(context);
  final msg = switch (r.outcome) {
    PurchaseOutcome.success => s.t('purchase.success'),
    PurchaseOutcome.restored => s.t('purchase.restored'),
    PurchaseOutcome.nothingToRestore => s.t('purchase.nothing_to_restore'),
    PurchaseOutcome.cancelled => s.t('purchase.cancelled'),
    PurchaseOutcome.pending => s.t('purchase.buying'),
    PurchaseOutcome.unavailable => s.t('purchase.unavailable'),
    PurchaseOutcome.failed => s.t('purchase.failed', {'msg': r.message ?? ''}),
  };
  ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
  return r.unlocked;
}

/// Paywall (ui-spec §4.14). Prices come only from the store's product info;
/// until the store reports one, the design placeholder “购买（价格待定）” is shown.
/// Pops with `true` when Pro got unlocked.
class PaywallScreen extends StatefulWidget {
  const PaywallScreen({super.key, this.reason});

  /// The free-tier limit that triggered the paywall, if any.
  final LimitHit? reason;

  @override
  State<PaywallScreen> createState() => _PaywallScreenState();
}

class _PaywallScreenState extends State<PaywallScreen> {
  bool _busy = false;
  String? _busyLabel;

  @override
  void initState() {
    super.initState();
    // After the first frame: init() notifies listeners, which must not happen during build.
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      final p = AppScope.read(context).purchase;
      if (p.product == null) p.init().catchError((Object _) {});
    });
  }

  Future<void> _run(Future<PurchaseResult> Function() action, String label) async {
    setState(() {
      _busy = true;
      _busyLabel = label;
    });
    PurchaseResult r;
    try {
      r = await action();
    } catch (e) {
      r = PurchaseResult(PurchaseOutcome.failed, '$e');
    }
    if (!mounted) return;
    setState(() => _busy = false);
    if (showPurchaseResult(context, r)) Navigator.pop(context, true);
  }

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final st = AppScope.of(context);
    final p = st.purchase;
    final price = p.product?.priceLabel;
    final limits = st.limits;
    final reason = widget.reason;
    final freeLimit = limits.isNone
        ? s.t('paywall.free_limit')
        : [
            if (limits.maxTriangles != null) '≤ ${formatCount(limits.maxTriangles!)}',
            if (limits.dailyConversions != null) '${limits.dailyConversions} / ${s.isZh ? '天' : 'day'}',
          ].join(' · ');
    return Scaffold(
      appBar: AppBar(),
      body: ListView(padding: const EdgeInsets.all(16), children: [
        InfoBanner(kind: BannerKind.warn, text: s.t('paywall.placeholder_banner')),
        if (p.isMock) ...[
          const SizedBox(height: 8),
          InfoBanner(kind: BannerKind.info, text: s.t('purchase.mock_note')),
        ],
        if (reason != null) ...[
          const SizedBox(height: 8),
          InfoBanner(
              kind: BannerKind.neutral,
              text: s.t(reason.messageKey, {'limit': formatCount(reason.limit), 'faces': formatCount(reason.value)})),
        ],
        const SizedBox(height: 24),
        Center(
          child: Container(
            width: 72,
            height: 72,
            decoration: BoxDecoration(color: t.accent, borderRadius: BorderRadius.circular(16)),
            child: const Icon(Icons.view_in_ar, color: Colors.white, size: 40),
          ),
        ),
        const SizedBox(height: 16),
        Center(child: Text(s.t('paywall.title'), style: TextStyle(fontSize: 28, fontWeight: FontWeight.w700, color: t.label))),
        const SizedBox(height: 16),
        for (final k in ['unlimited', 'large', 'iges', 'meta'])
          ListTile(
            leading: Icon(Icons.check_circle, color: t.success),
            title: Text(s.t('paywall.feature.$k')),
            trailing: Text(s.t('common.placeholder_tag'), style: TextStyle(color: t.warning)),
          ),
        const SizedBox(height: 8),
        Center(child: Text(freeLimit, style: TextStyle(color: t.label2))),
        const SizedBox(height: 16),
        if (p.isPro)
          InfoBanner(kind: BannerKind.success, text: s.t('purchase.status.pro'))
        else
          Container(
            decoration: price == null
                ? BoxDecoration(border: Border.all(color: t.warning), borderRadius: BorderRadius.circular(14))
                : null,
            child: FilledButton(
              key: const Key('buyButton'),
              onPressed: _busy ? null : () => _run(p.buyPro, s.t('purchase.buying')),
              child: _busy && _busyLabel == s.t('purchase.buying')
                  ? Text(_busyLabel!)
                  : Text(price == null ? s.t('paywall.buy') : s.t('purchase.buy_price', {'price': price})),
            ),
          ),
        const SizedBox(height: 4),
        TextButton(
          key: const Key('restoreButton'),
          onPressed: _busy ? null : () => _run(p.restore, s.t('purchase.restoring')),
          child: Text(_busy && _busyLabel == s.t('purchase.restoring') ? _busyLabel! : s.t('paywall.restore')),
        ),
        TextButton(onPressed: () => Navigator.pop(context, false), child: Text(s.t('paywall.later'))),
      ]),
    );
  }
}

/// Checks free-tier limits before converting. Returns true if the conversion may
/// proceed: no limit hit (always the case with [FreeTierLimits.none]), already Pro,
/// or the user unlocked Pro on the paywall that this opens.
Future<bool> ensureAllowed(BuildContext context, int faces, ConvertSettings settings) async {
  final hit = AppScope.read(context).limitFor(faces, settings);
  if (hit == null) return true;
  final unlocked = await Navigator.push<bool>(
      context, MaterialPageRoute(builder: (_) => PaywallScreen(reason: hit)));
  return unlocked ?? false;
}
