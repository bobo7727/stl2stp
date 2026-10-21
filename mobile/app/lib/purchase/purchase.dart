import 'dart:io';

import 'package:shared_preferences/shared_preferences.dart';

import 'mock_purchase_service.dart';
import 'purchase_service.dart';
import 'store_purchase_service.dart';

export 'free_tier.dart';
export 'mock_purchase_service.dart';
export 'purchase_service.dart';

/// Real store on iOS/macOS/Android, mock elsewhere (Linux/Windows dev).
/// `STL2STP_MOCK_STORE=1` forces the mock (e.g. simulator without StoreKit config).
PurchaseService createPurchaseService(SharedPreferences prefs, {PurchaseConfig config = const PurchaseConfig()}) {
  final storePlatform = Platform.isIOS || Platform.isMacOS || Platform.isAndroid;
  if (storePlatform && Platform.environment['STL2STP_MOCK_STORE'] != '1') {
    return StorePurchaseService(prefs, config: config);
  }
  return MockPurchaseService(prefs, config: config);
}
