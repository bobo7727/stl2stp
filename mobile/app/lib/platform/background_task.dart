import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

/// Lets a running conversion continue for a while after the app goes to the
/// background.
///
/// Flutter keeps the Dart isolates alive only as long as the OS keeps the
/// process running. On iOS the process is suspended a few seconds after
/// leaving the foreground unless the app holds a background task
/// (`UIApplication.beginBackgroundTask`, typically ~30 s, sometimes more).
/// The native side lives in the iOS runner (docs/mac-setup-guide.md §后台转换):
/// channel `stl2stp/background`, methods `begin` → task id, `end(id)`, and a
/// call back to Dart `expired(id)` from the expiration handler.
///
/// On every other platform this is a no-op: desktop apps keep running in the
/// background, and Android is not targeted yet.
class BackgroundTasks {
  BackgroundTasks({MethodChannel? channel, bool? enabled})
      : _channel = channel ?? const MethodChannel('stl2stp/background'),
        enabled = enabled ?? (!kIsWeb && Platform.isIOS) {
    if (this.enabled) _channel.setMethodCallHandler(_onCall);
  }

  final MethodChannel _channel;

  /// False → [begin]/[end] do nothing (Linux, desktop, widget tests).
  final bool enabled;
  final _onExpire = <int, VoidCallback>{};

  /// Starts a background task; returns its id, or null when unsupported or the
  /// native side is missing. [onExpire] runs if the OS is about to suspend the
  /// app before [end] is called (the conversion should be cancelled then).
  Future<int?> begin(String name, {VoidCallback? onExpire}) async {
    if (!enabled) return null;
    try {
      final id = await _channel.invokeMethod<int>('begin', {'name': name});
      if (id != null && onExpire != null) _onExpire[id] = onExpire;
      return id;
    } on MissingPluginException {
      return null;
    } on PlatformException {
      return null;
    }
  }

  /// Ends a task started with [begin] (null is ignored).
  Future<void> end(int? id) async {
    if (!enabled || id == null) return;
    _onExpire.remove(id);
    try {
      await _channel.invokeMethod<void>('end', {'id': id});
    } on MissingPluginException {
      // native side not installed: nothing to end
    } on PlatformException {
      // already ended by the OS
    }
  }

  Future<Object?> _onCall(MethodCall call) async {
    if (call.method == 'expired') handleExpired(call.arguments as int);
    return null;
  }

  /// The OS is about to end background task [id] (also used by tests).
  @visibleForTesting
  void handleExpired(int id) => _onExpire.remove(id)?.call();
}
