import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'ios_store_service.dart';
import 'mock_purchase_service.dart';
import 'purchase_service.dart';
import 'store_purchase_service.dart';
import 'tip_service.dart';

export 'free_tier.dart';
export 'mock_purchase_service.dart';
export 'purchase_service.dart';
export 'tip_service.dart';

/// iOS: native StoreKit 2 (tips + Pro restore). macOS/Android: `in_app_purchase`.
/// Linux/Windows: mock. `STL2STP_MOCK_STORE=1` forces the mock, but only in
/// non-release builds so a production build can never fall back to fake payments.
PurchaseService createPurchaseService(SharedPreferences prefs, {PurchaseConfig config = const PurchaseConfig()}) {
  final forceMock = !kReleaseMode && Platform.environment['STL2STP_MOCK_STORE'] == '1';
  if (!forceMock) {
    if (Platform.isIOS) return IosStoreService(prefs);
    if (Platform.isMacOS || Platform.isAndroid) return StorePurchaseService(prefs, config: config);
  }
  return MockPurchaseService(prefs, config: config);
}

/// The tip backend for a purchase service: only the iOS service offers tips.
TipService tipServiceFor(PurchaseService purchase) => purchase is TipService ? purchase as TipService : const NoTipService();
