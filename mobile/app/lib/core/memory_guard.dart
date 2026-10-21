import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'models.dart';

/// Peak-memory pre-check before a conversion (ui-spec §2.2 "内存不足").
///
/// Model (bytes): peak = base + perFace · F, where F is the number of faces
/// built by OCCT (after simplification); sewing and coplanar merging hold
/// extra copies of the shape (×[sewingFactor] / ×[mergeFactor]); with
/// simplification the QEM pass on the full mesh can be the peak instead
/// (base + simplifyPerTriangle · n). Calibrated with `max RSS` of
/// stl2stp-cli on the dev PC (tool/calib/memtime.csv, docs/flutter-app.md):
/// ORIGINAL STEP 140 MB @ 5k, 437 MB @ 20k, 1016 MB @ 50k, 2017 MB @ 102k
/// triangles (≈19.4 kB/face + ~45 MB); sewing ≈ +5 %; coplanar merge and
/// IGES measured lower (kept at ×1.0, conservative); simplify 50 % follows
/// the output face count. [simplifyPerTriangle] is not separable in the
/// measurements and stays a conservative guess.
class MemoryGuard {
  const MemoryGuard({
    this.baseBytes = 50e6,
    this.perFace = 20.5e3,
    this.sewingFactor = 1.06,
    this.mergeFactor = 1.0,
    this.simplifyPerTriangle = 1.2e3,
    this.warnFraction = 0.7,
  });

  final double baseBytes;
  final double perFace;
  final double sewingFactor;
  final double mergeFactor;
  final double simplifyPerTriangle;

  /// Warn above this share of the budget, refuse above the full budget.
  final double warnFraction;

  int peakBytes(int triangles, ConvertSettings s) {
    final n = math.max(triangles, 0);
    final f = s.simplify ? s.targetFaces(n) : n;
    var build = perFace * f;
    if (s.mode == ConvertMode.sewing) build *= sewingFactor;
    if (s.mergeCoplanar) build *= mergeFactor;
    final qem = s.simplify ? simplifyPerTriangle * n : 0.0;
    return (baseBytes + math.max(build, qem)).round();
  }

  MemoryCheck check(int triangles, ConvertSettings s, int budgetBytes) {
    final need = peakBytes(triangles, s);
    final level = need > budgetBytes
        ? MemoryLevel.refuse
        : need > warnFraction * budgetBytes
            ? MemoryLevel.warn
            : MemoryLevel.ok;
    int? suggest;
    if (level != MemoryLevel.ok) {
      // Largest keep-percentage (step 5) that fits under the warning line.
      for (var p = 95; p >= 5; p -= 5) {
        if (peakBytes(triangles, s.copyWith(simplify: true, keepPercent: p)) <= warnFraction * budgetBytes) {
          suggest = p;
          break;
        }
      }
    }
    return MemoryCheck(level, need, budgetBytes, suggest);
  }
}

enum MemoryLevel { ok, warn, refuse }

class MemoryCheck {
  const MemoryCheck(this.level, this.needBytes, this.budgetBytes, this.suggestPercent);
  final MemoryLevel level;
  final int needBytes;
  final int budgetBytes;

  /// Keep-percentage that would fit, or null if even 5 % does not.
  final int? suggestPercent;
}

/// How much memory a conversion may use on this device.
///
/// Flutter has no portable API for this. Order: explicit override (Settings /
/// tests) → native `stl2stp/memory.available` (iOS: `os_proc_available_memory()`,
/// see docs/mac-setup-guide.md) → Linux `/proc/meminfo` MemAvailable → a
/// conservative default ([fallbackBytes], sized for a 4 GB iPhone whose
/// per-app limit is roughly half of RAM).
class MemoryBudget {
  static const fallbackBytes = 1500 * 1000 * 1000;
  static const channel = MethodChannel('stl2stp/memory');

  /// Share of the reported available memory a conversion may use.
  static const availableShare = 0.8;

  static Future<int> detect({int? overrideBytes}) async {
    if (overrideBytes != null && overrideBytes > 0) return overrideBytes;
    if (!kIsWeb && Platform.isIOS) {
      try {
        final v = await channel.invokeMethod<int>('available');
        if (v != null && v > 0) return (v * availableShare).round();
      } on MissingPluginException {
        // native side not installed yet
      } on PlatformException {
        // ignore
      }
      return fallbackBytes;
    }
    if (!kIsWeb && Platform.isLinux) {
      final v = linuxAvailable();
      if (v != null) return (v * availableShare).round();
    }
    return fallbackBytes;
  }

  /// MemAvailable from /proc/meminfo in bytes, or null.
  static int? linuxAvailable([String path = '/proc/meminfo']) {
    try {
      for (final line in File(path).readAsLinesSync()) {
        if (line.startsWith('MemAvailable:')) {
          final kb = int.tryParse(line.split(RegExp(r'\s+'))[1]);
          return kb == null ? null : kb * 1024;
        }
      }
    } catch (_) {}
    return null;
  }
}
