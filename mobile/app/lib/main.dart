import 'dart:io';

import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'app.dart';
import 'core/demo_engine.dart';
import 'core/engine.dart';
import 'core/ffi_engine.dart';
import 'core/licenses.dart';
import 'core/memory_guard.dart';
import 'l10n/strings.dart';
import 'purchase/purchase.dart';
import 'state/app_state.dart';

/// Picks the real FFI engine when stl2stp-core can be loaded, else demo mode.
/// `STL2STP_DEMO=1` forces demo mode.
ConversionEngine createEngine() {
  if (Platform.environment['STL2STP_DEMO'] == '1') return DemoEngine();
  return FfiEngine.tryCreate() ?? DemoEngine();
}

Future<AppState> createAppState({ConversionEngine? engine}) async {
  final prefs = await SharedPreferences.getInstance();
  final dir = await getApplicationSupportDirectory();
  final purchase = createPurchaseService(prefs);
  // Store errors must never block the app; the paywall retries init().
  purchase.init().catchError((Object _) {});
  final budget = await MemoryBudget.detect();
  return AppState(
      engine: engine ?? createEngine(), prefs: prefs, outputDir: dir, purchase: purchase, memoryBudgetBytes: budget);
}

Future<void> main(List<String> args) async {
  WidgetsFlutterBinding.ensureInitialized();
  final state = await createAppState();
  await S.preload();
  registerAppLicenses();
  // Desktop: `stl2stp_app model.stl` opens the file directly (like "open with").
  final file = args.where((a) => !a.startsWith('-') && a.toLowerCase().endsWith('.stl')).firstOrNull;
  if (file != null && !state.onboardingSeen) state.onboardingSeen = true;
  runApp(Stl2StpApp(state: state, openOnStart: file));
}
