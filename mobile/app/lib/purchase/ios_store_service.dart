import 'dart:async';

import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'purchase_service.dart';
import 'tip_service.dart';

/// iOS backend: native StoreKit 2 (ios/Runner/StoreCoordinator.swift) for the
/// voluntary tip and the Pro non-consumable.
///
/// Pro truth lives natively (`Transaction.currentEntitlements`). `store.proActive`
/// is only a cache of the last native answer so a cold start does not flash the
/// paywall before the first native recompute; the native answer then replaces
/// it, true or false (a refund while the app was closed turns it off).
class IosStoreService extends PurchaseService implements TipService {
  IosStoreService(this._prefs, {MethodChannel? method, EventChannel? events})
      : _method = method ?? const MethodChannel('stl2stp/store'),
        _events = events ?? const EventChannel('stl2stp/store/events');

  static const _kPro = 'store.proActive';

  final SharedPreferences _prefs;
  final MethodChannel _method;
  final EventChannel _events;

  StreamSubscription<dynamic>? _sub;
  final _background = StreamController<TipOutcome>.broadcast();
  bool _started = false;
  Future<void> _mirror = Future.value();
  ProductInfo? _pro;
  bool _buying = false;
  TipLoad _load = TipLoad.idle;
  List<TipProduct> _products = const [];

  @override
  bool get isPro => _prefs.getBool(_kPro) ?? false;
  @override
  bool get available => _started;
  @override
  ProductInfo? get product => _pro;
  @override
  bool get proForSale => true;
  @override
  bool get supported => true;
  @override
  TipLoad get loadState => _load;
  @override
  List<TipProduct> get products => _products;
  @override
  Stream<TipOutcome> get background => _background.stream;

  @override
  Future<void> init() async {
    _sub ??= _events.receiveBroadcastStream().listen(_onEvent, onError: (Object _) {});
    try {
      final snap = await _method.invokeMapMethod<String, Object?>('start');
      _started = true;
      if (snap != null) _applyPro(snap);
    } catch (_) {
      // Store trouble must never block conversion.
    }
    notifyListeners();
    await loadProducts();
  }

  @override
  Future<void> loadProducts() async {
    _load = TipLoad.loading;
    notifyListeners();
    try {
      final r = await _method.invokeMapMethod<String, Object?>('loadProducts');
      final canPay = r?['canPay'] != false;
      final list = <TipProduct>[
        for (final p in (r?['tips'] as List? ?? const []))
          if (p is Map && p['id'] is String && p['price'] is String)
            TipProduct(
                id: p['id'] as String,
                title: '${p['title'] ?? ''}',
                description: '${p['description'] ?? ''}',
                priceLabel: p['price'] as String),
      ];
      _products = list;
      final pro = r?['pro'];
      _pro = pro is Map && pro['id'] is String && pro['price'] is String
          ? ProductInfo(
              id: pro['id'] as String,
              title: '${pro['title'] ?? ''}',
              description: '${pro['description'] ?? ''}',
              priceLabel: pro['price'] as String)
          : null;
      _load = canPay && list.isNotEmpty ? TipLoad.ready : TipLoad.unavailable;
    } catch (_) {
      _products = const [];
      _pro = null;
      _load = TipLoad.unavailable;
    }
    notifyListeners();
  }

  @override
  Future<TipResult> buyTip(String productId) async {
    if (_buying) return const TipResult(TipOutcome.busy);
    _buying = true;
    try {
      final r = await _method.invokeMapMethod<String, Object?>('buyTip', {'productId': productId});
      final msg = r?['message'] as String?;
      return switch (r?['outcome']) {
        'success' => const TipResult(TipOutcome.thanks),
        'cancelled' => const TipResult(TipOutcome.cancelled),
        'pending' => const TipResult(TipOutcome.pending),
        'unverified' => const TipResult(TipOutcome.unverified),
        'busy' => const TipResult(TipOutcome.busy),
        'unavailable' => const TipResult(TipOutcome.unavailable),
        _ => TipResult(TipOutcome.failed, msg),
      };
    } catch (e) {
      return TipResult(TipOutcome.failed, '$e');
    } finally {
      _buying = false;
    }
  }

  @override
  Future<PurchaseResult> buyPro() async {
    if (!_started) await init();
    if (_pro == null) return const PurchaseResult(PurchaseOutcome.unavailable);
    if (_buying) return const PurchaseResult(PurchaseOutcome.busy);
    _buying = true;
    try {
      final r = await _method.invokeMapMethod<String, Object?>('buyPro');
      await _mirror;
      return switch (r?['outcome']) {
        'success' => const PurchaseResult(PurchaseOutcome.success),
        'cancelled' => const PurchaseResult(PurchaseOutcome.cancelled),
        'pending' => const PurchaseResult(PurchaseOutcome.pending),
        'unverified' => const PurchaseResult(PurchaseOutcome.unverified),
        'busy' => const PurchaseResult(PurchaseOutcome.busy),
        'unavailable' => const PurchaseResult(PurchaseOutcome.unavailable),
        _ => PurchaseResult(PurchaseOutcome.failed, r?['message'] as String?),
      };
    } catch (e) {
      return PurchaseResult(PurchaseOutcome.failed, '$e');
    } finally {
      _buying = false;
    }
  }

  /// Explicit restore: sync with the App Store, then Apple's entitlements decide.
  /// Success with no entitlement turns the cache off; a failed sync leaves it alone.
  @override
  Future<PurchaseResult> restore() async {
    if (!_started) await init();
    if (!_started) return const PurchaseResult(PurchaseOutcome.unavailable);
    try {
      final r = await _method.invokeMapMethod<String, Object?>('restore');
      if (r?['outcome'] == 'failed') return PurchaseResult(PurchaseOutcome.failed, r?['message'] as String?);
      _applyPro({'pro': r?['pro'] == true});
      await _mirror;
      return r?['outcome'] == 'restored'
          ? const PurchaseResult(PurchaseOutcome.restored)
          : const PurchaseResult(PurchaseOutcome.nothingToRestore);
    } catch (e) {
      return PurchaseResult(PurchaseOutcome.failed, '$e');
    }
  }

  void _onEvent(dynamic e) {
    if (e is! Map) return;
    if (e['type'] == 'pro') _applyPro(Map<String, Object?>.from(e));
    if (e['type'] != 'tip') return;
    if (e['status'] == 'thanks') _background.add(TipOutcome.thanks);
    if (e['status'] == 'unverified') _background.add(TipOutcome.unverified);
  }

  /// Native answers are written in arrival order through one chain, so a stale
  /// `true` can never land after a newer `false`.
  void _applyPro(Map<String, Object?> snap) {
    final pro = snap['pro'] == true;
    _mirror = _mirror.then((_) async {
      if (isPro != pro) await _prefs.setBool(_kPro, pro);
      notifyListeners();
    });
  }

  @override
  void dispose() {
    _sub?.cancel();
    _background.close();
    super.dispose();
  }
}
