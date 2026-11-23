import 'dart:async';

import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'purchase_service.dart';
import 'tip_service.dart';

/// iOS backend: native StoreKit 2 (ios/Runner/StoreCoordinator.swift) for the
/// voluntary tip only.
///
/// The legacy Pro unlock was never sold and unlocks nothing, so it is neither
/// sold nor restored here ([proForSale] is false, [restore] finds nothing).
/// [isPro] only reads the old `store.proActive` cache and never writes it.
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
  bool _buying = false;
  TipLoad _load = TipLoad.idle;
  List<TipProduct> _products = const [];

  @override
  bool get isPro => _prefs.getBool(_kPro) ?? false;
  @override
  bool get available => _started;
  @override
  ProductInfo? get product => null;
  @override
  bool get proForSale => false;
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
      await _method.invokeMethod<Object?>('start');
      _started = true;
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
      _load = canPay && list.isNotEmpty ? TipLoad.ready : TipLoad.unavailable;
    } catch (_) {
      _products = const [];
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
  Future<PurchaseResult> buyPro() async => const PurchaseResult(PurchaseOutcome.unavailable);

  /// Nothing is restorable: tips are consumable and Pro was never sold.
  @override
  Future<PurchaseResult> restore() async => const PurchaseResult(PurchaseOutcome.nothingToRestore);

  void _onEvent(dynamic e) {
    if (e is! Map || e['type'] != 'tip') return;
    if (e['status'] == 'thanks') _background.add(TipOutcome.thanks);
    if (e['status'] == 'unverified') _background.add(TipOutcome.unverified);
  }

  @override
  void dispose() {
    _sub?.cancel();
    _background.close();
    super.dispose();
  }
}
