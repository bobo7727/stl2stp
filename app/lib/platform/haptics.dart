import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

/// Light haptic feedback for conversion start / success / failure.
/// iOS and Android only; a no-op on desktop (where HapticFeedback has no
/// effect anyway) unless [enabled] is forced (tests).
class Haptics {
  Haptics({bool? enabled}) : enabled = enabled ?? (!kIsWeb && (Platform.isIOS || Platform.isAndroid));
  final bool enabled;

  /// Conversion started.
  Future<void> start() => enabled ? HapticFeedback.lightImpact() : Future.value();

  /// Conversion finished and the file was written.
  Future<void> success() => enabled ? HapticFeedback.lightImpact() : Future.value();

  /// Conversion failed (not used for a user's own cancel).
  Future<void> failure() => enabled ? HapticFeedback.mediumImpact() : Future.value();
}
