import 'package:flutter/material.dart';

import '../core/estimate.dart';
import '../core/models.dart';
import '../l10n/strings.dart';
import '../purchase/purchase.dart';
import '../state/app_state.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';

/// Shows a purchase/restore result as a snackbar. Returns true if Pro is now unlocked.
bool showPurchaseResult(BuildContext context, PurchaseResult r) {
  final s = S.of(context);
  final msg = switch (r.outcome) {
    PurchaseOutcome.success => s.t('pro.success'),
    PurchaseOutcome.restored => s.t('pro.restored'),
    PurchaseOutcome.nothingToRestore => s.t('pro.nothing_to_restore'),
    PurchaseOutcome.cancelled => s.t('pro.cancelled'),
    PurchaseOutcome.pending => s.t('pro.pending'),
    PurchaseOutcome.unavailable => s.t('pro.unavailable'),
    PurchaseOutcome.unverified => s.t('pro.unverified'),
    PurchaseOutcome.failed => s.t('pro.failed', {'msg': r.message ?? ''}),
    PurchaseOutcome.busy => null,
  };
  if (msg != null) ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
  return r.unlocked;
}

/// How the paywall ended.
sealed class PaywallResult {
  const PaywallResult();
}

/// Pro was bought or restored; the caller re-checks everything and carries on.
class PaywallPurchased extends PaywallResult {
  const PaywallPurchased();
}

/// "Not now": nothing changes.
class PaywallCancelled extends PaywallResult {
  const PaywallCancelled();
}

/// The user chose a free alternative; the caller must re-check these settings.
class PaywallUseFree extends PaywallResult {
  const PaywallUseFree(this.settings);
  final ConvertSettings settings;
}

/// Largest reduce percentage (multiple of 5, within the 5–95 % slider) that fits
/// the free face limit, or null when even 5 % would not fit.
int? freeReducePercent(int faces, {int limit = FreeTierLimits.freeMaxTriangles}) {
  if (faces <= limit) return null;
  final pct = (limit * 100 / faces).floor() ~/ 5 * 5;
  return pct < 5 ? null : pct.clamp(5, 95);
}

/// Paywall for Pro (one-time, non-consumable). The price is only ever the
/// store's own display price. Pops with a [PaywallResult].
class PaywallScreen extends StatefulWidget {
  const PaywallScreen({super.key, this.reason, this.faces, this.settings});

  /// The free-tier limit that triggered the paywall, if any.
  final LimitHit? reason;

  /// Face count / current settings of the conversion that hit [reason] (for the free alternatives).
  final int? faces;
  final ConvertSettings? settings;

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
    if (showPurchaseResult(context, r)) Navigator.pop(context, const PaywallPurchased());
  }

  /// Free alternative for this trigger, or null (see PLAN §5): never offered
  /// when it could not actually satisfy the limits.
  ({String labelKey, Map<String, Object?> params, ConvertSettings settings})? _freeOption() {
    final reason = widget.reason, faces = widget.faces, cur = widget.settings;
    if (reason == null || faces == null || cur == null) return null;
    if (reason.kind == LimitKind.triangles) {
      final pct = freeReducePercent(faces, limit: reason.limit);
      if (pct == null) return null;
      final toStep = cur.format == ExportFormat.iges;
      return (
        labelKey: toStep ? 'pro.reduce.step' : 'pro.reduce',
        params: {'percent': pct},
        settings: cur.copyWith(format: ExportFormat.step, simplify: true, keepPercent: pct),
      );
    }
    if (reason.kind == LimitKind.iges) {
      return (labelKey: 'pro.use_step', params: const {}, settings: cur.copyWith(format: ExportFormat.step));
    }
    return null;
  }

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final st = AppScope.of(context);
    final p = st.purchase;
    // The mock store has no price; a real store without one means "not loaded".
    final price = p.product?.priceLabel ?? (p.isMock ? '—' : null);
    final reason = widget.reason;
    final free = _freeOption();
    final limit = st.limits.maxTriangles ?? FreeTierLimits.freeMaxTriangles;
    return Scaffold(
      appBar: AppBar(),
      body: ListView(padding: const EdgeInsets.all(16), children: [
        if (p.isMock) InfoBanner(kind: BannerKind.info, text: s.t('purchase.mock_note')),
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
        Center(
            child: Text(s.t('pro.title'),
                key: const Key('proTitle'),
                textAlign: TextAlign.center,
                style: TextStyle(fontSize: 26, fontWeight: FontWeight.w700, color: t.label))),
        const SizedBox(height: 8),
        Center(child: Text(s.t('pro.subtitle'), textAlign: TextAlign.center, style: TextStyle(color: t.label2))),
        const SizedBox(height: 16),
        for (final k in ['iges', 'batch', 'large'])
          ListTile(
            leading: Icon(Icons.check_circle, color: t.success),
            title: Text(s.t('pro.feature.$k', {'limit': formatCount(limit)})),
          ),
        const SizedBox(height: 4),
        Center(child: Text(s.t('pro.free_note', {'limit': formatCount(limit)}), style: TextStyle(color: t.label2))),
        const SizedBox(height: 16),
        if (p.isPro)
          InfoBanner(kind: BannerKind.success, text: s.t('pro.status.pro'))
        else ...[
          FilledButton(
            key: const Key('buyButton'),
            onPressed: _busy || price == null ? null : () => _run(p.buyPro, s.t('purchase.buying')),
            child: Text(_busy && _busyLabel == s.t('purchase.buying')
                ? _busyLabel!
                : price == null
                    ? s.t('pro.price_loading')
                    : s.t('pro.buy', {'price': price})),
          ),
          if (price == null)
            TextButton(
                key: const Key('priceRetry'),
                onPressed: _busy ? null : () => p.init().catchError((Object _) {}),
                child: Text(s.t('pro.price_retry'))),
          if (free != null)
            OutlinedButton(
              key: const Key('freeOption'),
              onPressed: _busy ? null : () => Navigator.pop(context, PaywallUseFree(free.settings)),
              child: Text(s.t(free.labelKey, free.params), textAlign: TextAlign.center),
            ),
        ],
        const SizedBox(height: 4),
        TextButton(
          key: const Key('restoreButton'),
          onPressed: _busy ? null : () => _run(p.restore, s.t('purchase.restoring')),
          child: Text(_busy && _busyLabel == s.t('purchase.restoring') ? _busyLabel! : s.t('pro.restore')),
        ),
        TextButton(
            key: const Key('laterButton'),
            onPressed: () => Navigator.pop(context, const PaywallCancelled()),
            child: Text(s.t('pro.later'))),
      ]),
    );
  }
}

/// Checks the free-tier limits for [settings] before converting. Returns the
/// settings to convert with, or null if the user stays on the preview. A free
/// alternative or a purchase is always followed by a fresh check of ALL limits
/// on the final settings, so nothing is waved through by having tapped a button.
Future<ConvertSettings?> ensureAllowed(BuildContext context, int faces, ConvertSettings settings) async {
  var cur = settings;
  for (var i = 0; i < 4; i++) {
    final hit = AppScope.read(context).limitFor(faces, cur);
    if (hit == null) return cur;
    final r = await Navigator.push<PaywallResult>(
        context, MaterialPageRoute(builder: (_) => PaywallScreen(reason: hit, faces: faces, settings: cur)));
    if (!context.mounted) return null;
    switch (r) {
      case PaywallUseFree(:final settings):
        cur = settings;
      case PaywallPurchased():
        break; // the next loop pass re-checks (Pro now bypasses the limits)
      case PaywallCancelled() || null:
        return null;
    }
  }
  return null;
}
