import 'dart:async';

import 'package:in_app_purchase/in_app_purchase.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'purchase_service.dart';

/// Real store via `in_app_purchase` (StoreKit on iOS/macOS, Play Billing on
/// Android). Only constructed on those platforms (see createPurchaseService).
///
/// UNTESTED: needs a Mac, a device/simulator with a StoreKit configuration
/// file or sandbox account, and the product created in App Store Connect.
/// See docs/mac-setup-guide.md §内购.
///
/// The unlock is cached in SharedPreferences so the app works offline.
/// TODO before release: verify transactions (StoreKit 2 JWS on device, or a
/// server) instead of trusting the local flag; decide on refund handling.
class StorePurchaseService extends PurchaseService {
  StorePurchaseService(this._prefs, {this.config = const PurchaseConfig(), InAppPurchase? iap})
      : _iap = iap ?? InAppPurchase.instance;

  static const _kPro = 'store.proActive';
  static const restoreTimeout = Duration(seconds: 12);

  final SharedPreferences _prefs;
  final PurchaseConfig config;
  final InAppPurchase _iap;

  StreamSubscription<List<PurchaseDetails>>? _sub;
  ProductDetails? _details;
  ProductInfo? _product;
  bool _available = false;
  Completer<PurchaseResult>? _pendingBuy;
  Completer<PurchaseResult>? _pendingRestore;

  @override
  bool get isPro => _prefs.getBool(_kPro) ?? false;
  @override
  bool get available => _available;
  @override
  ProductInfo? get product => _product;

  @override
  Future<void> init() async {
    _sub ??= _iap.purchaseStream.listen(_onPurchases, onError: (Object e) {
      _finish(PurchaseResult(PurchaseOutcome.failed, '$e'));
    });
    _available = await _iap.isAvailable();
    if (_available) {
      final resp = await _iap.queryProductDetails({config.proProductId});
      _details = resp.productDetails.where((d) => d.id == config.proProductId).firstOrNull;
      final d = _details;
      _product = d == null
          ? null
          : ProductInfo(id: d.id, title: d.title, description: d.description, priceLabel: d.price);
    }
    notifyListeners();
  }

  @override
  Future<PurchaseResult> buyPro() async {
    if (!_available || _details == null) await init();
    final d = _details;
    if (!_available || d == null) return const PurchaseResult(PurchaseOutcome.unavailable);
    _pendingBuy = Completer();
    try {
      final started = await _iap.buyNonConsumable(purchaseParam: PurchaseParam(productDetails: d));
      if (!started) _finish(const PurchaseResult(PurchaseOutcome.failed, 'not started'));
    } catch (e) {
      _finish(PurchaseResult(PurchaseOutcome.failed, '$e'));
    }
    return _pendingBuy!.future;
  }

  @override
  Future<PurchaseResult> restore() async {
    if (!_available) await init();
    if (!_available) return const PurchaseResult(PurchaseOutcome.unavailable);
    final c = _pendingRestore = Completer();
    await _iap.restorePurchases();
    // StoreKit sends nothing when there is nothing to restore.
    return c.future.timeout(restoreTimeout, onTimeout: () {
      _pendingRestore = null;
      return const PurchaseResult(PurchaseOutcome.nothingToRestore);
    });
  }

  Future<void> _onPurchases(List<PurchaseDetails> list) async {
    for (final p in list) {
      if (p.productID != config.proProductId) continue;
      switch (p.status) {
        case PurchaseStatus.pending:
          break; // Ask to Buy etc.; the stream reports the final state later.
        case PurchaseStatus.purchased:
        case PurchaseStatus.restored:
          await _prefs.setBool(_kPro, true);
          notifyListeners();
          _finish(PurchaseResult(
              p.status == PurchaseStatus.restored ? PurchaseOutcome.restored : PurchaseOutcome.success));
        case PurchaseStatus.canceled:
          _finish(const PurchaseResult(PurchaseOutcome.cancelled));
        case PurchaseStatus.error:
          _finish(PurchaseResult(PurchaseOutcome.failed, p.error?.message));
      }
      if (p.pendingCompletePurchase) await _iap.completePurchase(p);
    }
  }

  void _finish(PurchaseResult r) {
    final restoring = r.outcome == PurchaseOutcome.restored;
    final c = restoring ? (_pendingRestore ?? _pendingBuy) : (_pendingBuy ?? _pendingRestore);
    if (c != null && !c.isCompleted) c.complete(r);
    if (identical(c, _pendingBuy)) _pendingBuy = null;
    if (identical(c, _pendingRestore)) _pendingRestore = null;
  }

  @override
  void dispose() {
    _sub?.cancel();
    super.dispose();
  }
}
