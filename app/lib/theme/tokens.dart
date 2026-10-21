import 'package:flutter/material.dart';

/// Design tokens from ui-spec §6 (light / dark).
@immutable
class Tokens extends ThemeExtension<Tokens> {
  const Tokens({
    required this.accent,
    required this.success,
    required this.warning,
    required this.danger,
    required this.bg,
    required this.surface,
    required this.surface2,
    required this.fill,
    required this.label,
    required this.label2,
    required this.label3,
    required this.separator,
    required this.warnBg,
    required this.warnFg,
    required this.infoBg,
    required this.infoFg,
    required this.viewerTop,
    required this.viewerBottom,
    required this.mesh,
  });

  final Color accent, success, warning, danger, bg, surface, surface2, fill, label, label2, label3, separator;
  final Color warnBg, warnFg, infoBg, infoFg, viewerTop, viewerBottom, mesh;

  static const light = Tokens(
    accent: Color(0xFF007AFF),
    success: Color(0xFF34C759),
    warning: Color(0xFFFF9500),
    danger: Color(0xFFFF3B30),
    bg: Color(0xFFF2F2F7),
    surface: Color(0xFFFFFFFF),
    surface2: Color(0xFFF2F2F7),
    fill: Color(0x1F787880),
    label: Color(0xFF000000),
    label2: Color(0x993C3C43),
    label3: Color(0x4D3C3C43),
    separator: Color(0x4A3C3C43),
    warnBg: Color(0xFFFFF4E5),
    warnFg: Color(0xFF8A4B00),
    infoBg: Color(0xFFEAF2FF),
    infoFg: Color(0xFF0A4FB3),
    viewerTop: Color(0xFFEEF2F8),
    viewerBottom: Color(0xFFDCE3EE),
    mesh: Color(0xFF6E91CD),
  );

  static const dark = Tokens(
    accent: Color(0xFF0A84FF),
    success: Color(0xFF30D158),
    warning: Color(0xFFFF9F0A),
    danger: Color(0xFFFF453A),
    bg: Color(0xFF000000),
    surface: Color(0xFF1C1C1E),
    surface2: Color(0xFF2C2C2E),
    fill: Color(0x3D787880),
    label: Color(0xFFFFFFFF),
    label2: Color(0x99EBEBF5),
    label3: Color(0x4DEBEBF5),
    separator: Color(0xA6545458),
    warnBg: Color(0xFF3A2A10),
    warnFg: Color(0xFFFFD08A),
    infoBg: Color(0xFF0F2547),
    infoFg: Color(0xFF9CC4FF),
    viewerTop: Color(0xFF1E2430),
    viewerBottom: Color(0xFF111419),
    mesh: Color(0xFF7896C8),
  );

  static Tokens of(BuildContext context) => Theme.of(context).extension<Tokens>()!;

  @override
  Tokens copyWith() => this;

  @override
  Tokens lerp(ThemeExtension<Tokens>? other, double t) => t < 0.5 ? this : (other as Tokens? ?? this);
}

ThemeData buildTheme(Brightness b) {
  final t = b == Brightness.light ? Tokens.light : Tokens.dark;
  final scheme = ColorScheme.fromSeed(seedColor: t.accent, brightness: b).copyWith(
    primary: t.accent,
    error: t.danger,
    surface: t.surface,
  );
  return ThemeData(
    useMaterial3: true,
    brightness: b,
    colorScheme: scheme,
    scaffoldBackgroundColor: t.bg,
    extensions: [t],
    appBarTheme: AppBarTheme(
      backgroundColor: t.bg,
      surfaceTintColor: Colors.transparent,
      elevation: 0,
      centerTitle: true,
      foregroundColor: t.label,
      titleTextStyle: TextStyle(fontSize: 17, fontWeight: FontWeight.w600, color: t.label),
    ),
    cardTheme: CardThemeData(
      color: t.surface,
      elevation: 0,
      margin: EdgeInsets.zero,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
    ),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        minimumSize: const Size.fromHeight(50),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        textStyle: const TextStyle(fontSize: 17, fontWeight: FontWeight.w600),
      ),
    ),
    dividerTheme: DividerThemeData(color: t.separator, thickness: 0.5, space: 0.5),
    listTileTheme: ListTileThemeData(tileColor: t.surface, minTileHeight: 44),
  );
}

/// Tabular (monospaced) digits per ui-spec §6.2.
const tabular = [FontFeature.tabularFigures()];
