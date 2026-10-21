import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../core/estimate.dart';
import '../l10n/strings.dart';
import '../state/app_state.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';

enum ErrorKind { format, empty, tooBig, memory, cancelled, io, generic }

/// Error / cancelled screens (ui-spec §4.11).
class ErrorScreen extends StatelessWidget {
  const ErrorScreen(
      {super.key, required this.kind, this.code, this.details = '', this.faces, this.onPrimary, this.notice});
  final ErrorKind kind;
  final int? code;
  final String details;
  final int? faces;
  final void Function(BuildContext context)? onPrimary;

  /// Extra explanation shown under the body (e.g. why the app stopped the
  /// conversion itself: memory pressure, background time used up).
  final String? notice;

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final (icon, color, key) = switch (kind) {
      ErrorKind.format => (Icons.description_outlined, t.danger, 'format'),
      ErrorKind.empty => (Icons.view_in_ar_outlined, t.warning, 'empty'),
      ErrorKind.tooBig => (Icons.zoom_out_map, t.warning, 'too_big'),
      ErrorKind.memory => (Icons.memory, t.danger, 'memory'),
      ErrorKind.cancelled => (Icons.stop_circle_outlined, t.label2, 'cancelled'),
      ErrorKind.io => (Icons.folder_off_outlined, t.danger, 'io'),
      ErrorKind.generic => (Icons.warning_amber_rounded, t.danger, 'generic'),
    };
    final primary = switch (kind) {
      ErrorKind.format || ErrorKind.empty => s.t('error.choose_other'),
      ErrorKind.tooBig => s.t('error.too_big.action'),
      ErrorKind.memory => s.t('error.memory.action'),
      ErrorKind.cancelled => s.t('error.cancelled.action'),
      ErrorKind.io || ErrorKind.generic => s.t('common.retry'),
    };
    final copyDetails = kind == ErrorKind.memory || kind == ErrorKind.generic;
    final secondary = copyDetails
        ? s.t('error.copy_details')
        : kind == ErrorKind.cancelled
            ? s.t('error.choose_other')
            : s.t('common.back');
    final body = s.t('error.$key.body', {
      'code': code ?? '?',
      'faces': formatCount(faces ?? 0),
      'limit': formatCount(AppState.limitFaces),
    });
    return Scaffold(
      appBar: AppBar(),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(children: [
            const Spacer(),
            StatusIcon(icon: icon, color: color),
            const SizedBox(height: 20),
            Text(s.t('error.$key.title', {'code': code ?? '?'}),
                key: const Key('errorTitle'),
                textAlign: TextAlign.center,
                style: TextStyle(fontSize: 22, fontWeight: FontWeight.w700, color: t.label)),
            const SizedBox(height: 8),
            Text(body, textAlign: TextAlign.center, style: TextStyle(fontSize: 15, color: t.label2)),
            if (notice != null) ...[
              const SizedBox(height: 16),
              InfoBanner(key: const Key('errorNotice'), kind: BannerKind.warn, text: notice!),
            ],
            const Spacer(flex: 2),
            if (onPrimary != null)
              FilledButton(key: const Key('errorPrimary'), onPressed: () => onPrimary!(context), child: Text(primary)),
            TextButton(
              onPressed: () {
                if (copyDetails) {
                  Clipboard.setData(ClipboardData(
                      text: 'code=$code faces=$faces os=${Platform.operatingSystem} ${Platform.operatingSystemVersion}\n$details'));
                  ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(s.t('x.copied'))));
                } else if (kind == ErrorKind.cancelled) {
                  Navigator.of(context).popUntil((r) => r.isFirst);
                } else {
                  Navigator.of(context).maybePop();
                }
              },
              child: Text(secondary),
            ),
          ]),
        ),
      ),
    );
  }
}
