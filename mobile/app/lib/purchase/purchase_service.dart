import 'package:flutter/foundation.dart';

/// Product ids and other store configuration. Prices are never hard-coded:
/// the UI shows [ProductInfo.priceLabel] from the store, or a placeholder.
class PurchaseConfig {
  const PurchaseConfig({this.proProductId = 'com.stl2stp.app.pro'});

  /// One-time (non-consumable) "Pro" unlock. Must match App Store Connect.
  final String proProductId;
}

/// What the store reports about a product.
@immutable
class ProductInfo {
  const ProductInfo({required this.id, required this.title, required this.description, this.priceLabel});
  final String id;
  final String title;
  final String description;

  /// Localized price string from the store (e.g. StoreKit `displayPrice`);
  /// null when unknown, in which case the UI shows a placeholder.
  final String? priceLabel;
}

enum PurchaseOutcome { success, restored, nothingToRestore, cancelled, pending, failed, unavailable }

@immutable
class PurchaseResult {
  const PurchaseResult(this.outcome, [this.message]);
  final PurchaseOutcome outcome;
  final String? message;
  bool get unlocked => outcome == PurchaseOutcome.success || outcome == PurchaseOutcome.restored;
}

/// Abstract purchase backend. One product: a one-time "Pro" unlock.
///
/// Implementations: [MockPurchaseService] (Linux / desktop / tests) and
/// `StorePurchaseService` (in_app_purchase → StoreKit on iOS).
abstract class PurchaseService extends ChangeNotifier {
  /// True once Pro is unlocked (cached locally so it works offline).
  bool get isPro;

  /// Whether the store can be reached right now.
  bool get available;

  /// The Pro product as reported by the store, once loaded.
  ProductInfo? get product;

  /// True for the development mock (shows a "no charge" note).
  bool get isMock => false;

  /// Connects to the store and loads [product]. Safe to call more than once.
  Future<void> init();

  Future<PurchaseResult> buyPro();

  Future<PurchaseResult> restore();
}
