import 'dart:io';

import 'package:flutter/foundation.dart';
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

/// Compile-time opt-in for [DemoEngine] (`--dart-define=STL2STP_DEMO=true` or `=1`).
/// This is the only way a release build may use the demo engine.
const bool kDemoEngineDefine =
    bool.fromEnvironment('STL2STP_DEMO') || String.fromEnvironment('STL2STP_DEMO') == '1';

/// Picks the real FFI engine when stl2stp-core can be loaded and initialized.
///
/// [DemoEngine] is used only in debug/profile, or when [kDemoEngineDefine] is
/// set. Debug/profile also honor the `STL2STP_DEMO=1` environment variable,
/// which forces demo mode without loading the core. A release build never
/// falls back to [DemoEngine] when the native core fails: it returns
/// [UnavailableEngine] so the UI can show an error.
ConversionEngine createEngine({
  bool? releaseMode,
  bool? allowDemoDefine,
  Map<String, String>? environment,
  ConversionEngine? Function()? loadNative,
  ConversionEngine Function()? createDemo,
}) {
  final release = releaseMode ?? kReleaseMode;
  final env = environment ?? Platform.environment;
  final allowDemo = allowDemoDefine ?? kDemoEngineDefine;
  final explicitDemo = allowDemo || (!release && env['STL2STP_DEMO'] == '1');
  ConversionEngine demo() {
    final factory = createDemo;
    if (factory != null) return factory();
    return DemoEngine();
  }
  if (explicitDemo) return demo();

  Object? failure;
  try {
    final native = (loadNative ?? _loadNative)();
    if (native != null) return native;
    failure = 'stl2stp-core failed to load';
  } catch (e) {
    failure = e is StateError ? e.message : '$e';
  }
  if (!release) return demo();
  return UnavailableEngine(detail: failure.toString());
}

ConversionEngine _loadNative() {
  final engine = FfiEngine.tryCreate();
  if (engine != null) return engine;
  throw StateError(FfiEngine.lastLoadError ?? 'stl2stp-core failed to load');
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
