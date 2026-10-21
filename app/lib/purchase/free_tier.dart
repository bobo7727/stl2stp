import 'package:flutter/foundation.dart';

import '../core/models.dart';

/// Free-tier limits. Every limit defaults to "none", so with the default
/// [FreeTierLimits.none] the app behaves exactly as without a purchase system.
/// Real values are a product decision (ui-spec §4.14: "额度待定").
@immutable
class FreeTierLimits {
  const FreeTierLimits({this.maxTriangles, this.dailyConversions, this.igesRequiresPro = false, this.metaRequiresPro = false});

  static const none = FreeTierLimits();

  /// Largest mesh (triangles after simplification) a free user may convert; null = no limit.
  final int? maxTriangles;

  /// Conversions per local calendar day; null = unlimited.
  final int? dailyConversions;
  final bool igesRequiresPro;
  final bool metaRequiresPro;

  bool get isNone => maxTriangles == null && dailyConversions == null && !igesRequiresPro && !metaRequiresPro;

  /// First limit a free user would hit, or null if allowed.
  LimitHit? check({required int triangles, required ConvertSettings settings, required int conversionsToday}) {
    final maxT = maxTriangles;
    // Triangles after simplification are what the core actually processes.
    final effective = settings.simplify ? (triangles * settings.keepPercent / 100).round() : triangles;
    if (maxT != null && effective > maxT) return LimitHit(LimitKind.triangles, limit: maxT, value: effective);
    final daily = dailyConversions;
    if (daily != null && conversionsToday >= daily) return LimitHit(LimitKind.daily, limit: daily, value: conversionsToday);
    if (igesRequiresPro && settings.format == ExportFormat.iges) return const LimitHit(LimitKind.iges);
    final hasMeta = [settings.author, settings.organization, settings.product, settings.description].any((s) => s.isNotEmpty);
    if (metaRequiresPro && hasMeta) return const LimitHit(LimitKind.meta);
    return null;
  }
}

enum LimitKind { triangles, daily, iges, meta }

@immutable
class LimitHit {
  const LimitHit(this.kind, {this.limit = 0, this.value = 0});
  final LimitKind kind;
  final int limit;
  final int value;

  /// String key in assets/i18n/app_*.json.
  String get messageKey => 'purchase.limit.${kind.name}';
}
