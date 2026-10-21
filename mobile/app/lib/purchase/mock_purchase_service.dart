import 'package:shared_preferences/shared_preferences.dart';

import 'purchase_service.dart';

/// In-memory store for Linux/desktop development and tests. Never charges.
/// The unlock is persisted in SharedPreferences so restarts behave like a
/// real store; [reset] clears it (Settings → "Reset purchase (dev only)").
class MockPurchaseService extends PurchaseService {
  MockPurchaseService(
    this._prefs, {
    this.config = const PurchaseConfig(),
    this.priceLabel,
    this.delay = const Duration(milliseconds: 400),
    this.simulate = PurchaseOutcome.success,
  });

  static const _kOwned = 'mock.proOwned';
  static const _kPro = 'mock.proActive';

  final SharedPreferences _prefs;
  final PurchaseConfig config;

  /// Price label the mock "store" reports; null = unknown (placeholder shown).
  final String? priceLabel;
  final Duration delay;

  /// Outcome of the next [buyPro] (success, cancelled, failed, unavailable …).
  PurchaseOutcome simulate;

  ProductInfo? _product;

  /// "Owned" = bought on this account (restorable); "active" = unlocked on this install.
  bool get _owned => _prefs.getBool(_kOwned) ?? false;

  @override
  bool get isPro => _prefs.getBool(_kPro) ?? false;

  @override
  bool get available => simulate != PurchaseOutcome.unavailable;

  @override
  bool get isMock => true;

  @override
  ProductInfo? get product => _product;

  @override
  Future<void> init() async {
    _product = ProductInfo(id: config.proProductId, title: 'Pro', description: 'Mock product', priceLabel: priceLabel);
    notifyListeners();
  }

  @override
  Future<PurchaseResult> buyPro() async {
    await Future<void>.delayed(delay);
    switch (simulate) {
      case PurchaseOutcome.success:
        await _prefs.setBool(_kOwned, true);
        await _prefs.setBool(_kPro, true);
        notifyListeners();
        return const PurchaseResult(PurchaseOutcome.success);
      case PurchaseOutcome.failed:
        return const PurchaseResult(PurchaseOutcome.failed, 'mock failure');
      default:
        return PurchaseResult(simulate);
    }
  }

  @override
  Future<PurchaseResult> restore() async {
    await Future<void>.delayed(delay);
    if (!available) return const PurchaseResult(PurchaseOutcome.unavailable);
    if (!_owned) return const PurchaseResult(PurchaseOutcome.nothingToRestore);
    await _prefs.setBool(_kPro, true);
    notifyListeners();
    return const PurchaseResult(PurchaseOutcome.restored);
  }

  /// Dev only: forget the active unlock on this install. With [forgetOwned]
  /// also forget the "account" purchase, so Restore finds nothing.
  Future<void> reset({bool forgetOwned = false}) async {
    await _prefs.remove(_kPro);
    if (forgetOwned) await _prefs.remove(_kOwned);
    notifyListeners();
  }
}
