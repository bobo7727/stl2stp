import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:path/path.dart' as p;
import 'package:shared_preferences/shared_preferences.dart';

import '../core/engine.dart';
import '../core/estimate.dart';
import '../core/memory_guard.dart';
import '../core/models.dart';
import '../platform/background_task.dart';
import '../platform/haptics.dart';
import '../purchase/free_tier.dart';
import '../purchase/mock_purchase_service.dart';
import '../purchase/purchase_service.dart';
import 'history.dart';

/// App-wide state: preferences, history, the conversion engine and output dir.
class AppState extends ChangeNotifier {
  AppState({
    required this.engine,
    required this.prefs,
    required this.outputDir,
    PurchaseService? purchase,
    this.limits = FreeTierLimits.none,
    this.estimator = const Estimator(),
    this.memoryGuard = const MemoryGuard(),
    int? memoryBudgetBytes,
    BackgroundTasks? background,
    Haptics? haptics,
  })  : purchase = purchase ?? MockPurchaseService(prefs),
        detectedMemoryBudget = memoryBudgetBytes ?? MemoryBudget.fallbackBytes,
        background = background ?? BackgroundTasks(),
        haptics = haptics ?? Haptics() {
    _history = HistoryStore(File(p.join(outputDir.path, 'history.json')));
    history = _history.load();
    this.purchase.addListener(notifyListeners);
  }

  final ConversionEngine engine;
  final SharedPreferences prefs;
  final Directory outputDir;
  late final HistoryStore _history;
  late List<HistoryEntry> history;

  /// Store backend (mock on Linux/desktop, in_app_purchase on iOS).
  final PurchaseService purchase;

  /// Free-tier limits; [FreeTierLimits.none] (default) = no restrictions.
  final FreeTierLimits limits;

  /// Time/size estimate model (coefficients + device factor, see estimate.dart).
  final Estimator estimator;

  /// Peak-memory model for the pre-conversion check.
  final MemoryGuard memoryGuard;

  /// Device memory budget found at startup ([MemoryBudget.detect]).
  final int detectedMemoryBudget;

  /// iOS background task bridge (no-op elsewhere).
  final BackgroundTasks background;

  /// Haptic feedback (no-op on desktop).
  final Haptics haptics;

  /// Budget used for the memory check: the `memoryBudgetMB` preference when
  /// set (configurable per device), else [detectedMemoryBudget].
  int get memoryBudgetBytes {
    final mb = prefs.getInt('memoryBudgetMB');
    return mb != null && mb > 0 ? mb * 1000 * 1000 : detectedMemoryBudget;
  }

  set memoryBudgetMB(int? mb) {
    if (mb == null || mb <= 0) {
      prefs.remove('memoryBudgetMB');
    } else {
      prefs.setInt('memoryBudgetMB', mb);
    }
    notifyListeners();
  }

  /// Faces above which we suggest simplifying / refuse (placeholders, ui-spec §2.2).
  static const largeFaces = 200000;
  static const limitFaces = 2000000;

  // ---- preferences ----
  ThemeMode get themeMode => ThemeMode.values.byName(prefs.getString('themeMode') ?? 'system');
  set themeMode(ThemeMode m) {
    prefs.setString('themeMode', m.name);
    notifyListeners();
  }

  /// 'zh' (default), 'en' or 'system'.
  String get language => prefs.getString('language') ?? 'zh';
  set language(String v) {
    prefs.setString('language', v);
    notifyListeners();
  }

  Locale? get locale => language == 'system' ? null : Locale(language);

  bool get onboardingSeen => prefs.getBool('onboardingSeen') ?? false;
  set onboardingSeen(bool v) {
    prefs.setBool('onboardingSeen', v);
    notifyListeners();
  }

  ExportFormat get defaultFormat => ExportFormat.values.byName(prefs.getString('defaultFormat') ?? 'step');
  set defaultFormat(ExportFormat f) {
    prefs.setString('defaultFormat', f.name);
    notifyListeners();
  }

  /// A preset name or 'last'.
  String get defaultPreset => prefs.getString('defaultPreset') ?? 'last';
  set defaultPreset(String v) {
    prefs.setString('defaultPreset', v);
    notifyListeners();
  }

  /// Default metadata (empty unless set in Settings).
  ConvertSettings get defaultMeta {
    final raw = prefs.getString('defaultMeta');
    if (raw == null) return const ConvertSettings();
    return ConvertSettings.fromJson((jsonDecode(raw) as Map).cast<String, Object?>());
  }

  set defaultMeta(ConvertSettings s) {
    prefs.setString('defaultMeta', jsonEncode(s.toJson()));
    notifyListeners();
  }

  /// Initial settings for a new file (ui-spec §4.6 "设置是否记住").
  ConvertSettings initialSettings() {
    final meta = defaultMeta;
    ConvertSettings base;
    if (defaultPreset == 'last' && prefs.getString('lastSettings') != null) {
      base = ConvertSettings.fromJson((jsonDecode(prefs.getString('lastSettings')!) as Map).cast<String, Object?>());
    } else {
      final preset = Preset.values.where((p) => p.name == defaultPreset).firstOrNull ?? Preset.recommended;
      base = ConvertSettings.forPreset(preset, format: defaultFormat);
    }
    return base.copyWith(
        author: meta.author, organization: meta.organization, product: meta.product, description: meta.description);
  }

  void rememberSettings(ConvertSettings s) {
    // Metadata is only prefilled from Settings, so do not remember it here.
    prefs.setString(
        'lastSettings', jsonEncode(s.copyWith(author: '', organization: '', product: '', description: '').toJson()));
  }

  // ---- purchase / free tier ----
  bool get isPro => purchase.isPro;

  String _today() {
    final n = DateTime.now();
    return '${n.year}-${n.month}-${n.day}';
  }

  int get conversionsToday => prefs.getString('convDay') == _today() ? (prefs.getInt('convCount') ?? 0) : 0;

  /// Called after each successful conversion (for daily limits).
  void recordConversion() {
    final c = conversionsToday + 1;
    prefs.setString('convDay', _today());
    prefs.setInt('convCount', c);
  }

  /// The free-tier limit this conversion would hit, or null if allowed.
  LimitHit? limitFor(int triangles, ConvertSettings s) =>
      isPro ? null : limits.check(triangles: triangles, settings: s, conversionsToday: conversionsToday);

  // ---- outputs & history ----
  String outputPathFor(String sourcePath, ExportFormat f) {
    final dir = Directory(p.join(outputDir.path, 'Outputs'))..createSync(recursive: true);
    final stem = p.basenameWithoutExtension(sourcePath);
    var candidate = p.join(dir.path, '$stem.${f.extension}');
    var i = 2;
    while (File(candidate).existsSync()) {
      candidate = p.join(dir.path, '$stem ($i).${f.extension}');
      i++;
    }
    return candidate;
  }

  void addHistory(HistoryEntry e) {
    history = [e, ...history].take(HistoryStore.maxEntries).toList();
    _history.save(history);
    notifyListeners();
  }

  void deleteHistory(HistoryEntry e, {bool deleteFile = true}) {
    history = history.where((h) => h.outputPath != e.outputPath).toList();
    if (deleteFile) {
      try {
        File(e.outputPath).deleteSync();
      } catch (_) {}
    }
    _history.save(history);
    notifyListeners();
  }

  void clearHistory() {
    for (final e in history) {
      try {
        File(e.outputPath).deleteSync();
      } catch (_) {}
    }
    history = [];
    _history.save(history);
    notifyListeners();
  }

  @override
  void dispose() {
    purchase.removeListener(notifyListeners);
    super.dispose();
  }
}

/// Provides [AppState] to the widget tree.
class AppScope extends InheritedNotifier<AppState> {
  const AppScope({super.key, required AppState state, required super.child}) : super(notifier: state);

  static AppState of(BuildContext context) => context.dependOnInheritedWidgetOfExactType<AppScope>()!.notifier!;
  static AppState read(BuildContext context) =>
      (context.getElementForInheritedWidgetOfExactType<AppScope>()!.widget as AppScope).notifier!;
}
