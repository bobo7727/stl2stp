import 'package:flutter/foundation.dart';

/// First-release tip product (consumable). Must match App Store Connect.
/// A tip is purely voluntary: it never unlocks anything and never touches
/// [PurchaseService.isPro] or the free-tier limits.
const tipSmallProductId = 'com.stl2stp.app.tip.small';

@immutable
class TipProduct {
  const TipProduct({required this.id, required this.title, required this.description, required this.priceLabel});
  final String id;
  final String title;
  final String description;

  /// Store-localised price (StoreKit `displayPrice`); never computed here.
  final String priceLabel;
}

enum TipLoad { idle, loading, ready, unavailable }

enum TipOutcome { thanks, cancelled, pending, unverified, busy, failed, unavailable }

@immutable
class TipResult {
  const TipResult(this.outcome, [this.message]);
  final TipOutcome outcome;
  final String? message;
}

/// Voluntary-tip backend, separate from the legacy Pro [PurchaseService].
abstract interface class TipService implements Listenable {
  /// False where tips are not offered (everything except iOS).
  bool get supported;
  TipLoad get loadState;
  List<TipProduct> get products;

  /// Verified tip completed or rejected outside a user-initiated buy
  /// (Ask to Buy approval, replay after a crash). Only thanks / unverified.
  Stream<TipOutcome> get background;

  Future<void> loadProducts();
  Future<TipResult> buyTip(String productId);
}

/// Tips are not offered on this platform.
class NoTipService implements TipService {
  const NoTipService();
  @override
  bool get supported => false;
  @override
  TipLoad get loadState => TipLoad.unavailable;
  @override
  List<TipProduct> get products => const [];
  @override
  Stream<TipOutcome> get background => const Stream.empty();
  @override
  Future<void> loadProducts() async {}
  @override
  Future<TipResult> buyTip(String productId) async => const TipResult(TipOutcome.unavailable);
  @override
  void addListener(VoidCallback listener) {}
  @override
  void removeListener(VoidCallback listener) {}
}
