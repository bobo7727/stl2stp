import 'package:flutter/services.dart';
import 'package:flutter/material.dart';
import 'package:url_launcher/url_launcher.dart';

/// Public pages App Review needs to reach from inside the app (guideline 5.1.1(i)).
///
/// PLACEHOLDERS: `.invalid` is a reserved TLD that never resolves. bobo must
/// replace both with the real public HTTPS pages (the same URLs entered in
/// App Store Connect) BEFORE submission; [linksArePlaceholders] tells tests and
/// the release checklist whether that has happened.
const privacyPolicyUrl = 'https://replace-me.invalid/stl2stp/privacy';
const supportUrl = 'https://replace-me.invalid/stl2stp/support';

bool get linksArePlaceholders => Uri.parse(privacyPolicyUrl).host.endsWith('.invalid') || Uri.parse(supportUrl).host.endsWith('.invalid');

/// Opens [url] in the system browser. Returns false when it could not be
/// opened (the caller then offers to copy it).
Future<bool> openExternalUrl(String url, {Future<bool> Function(Uri)? launcher}) async {
  try {
    final uri = Uri.parse(url);
    if (uri.scheme != 'https') return false;
    return await (launcher ?? (u) => launchUrl(u, mode: LaunchMode.externalApplication))(uri);
  } catch (_) {
    return false;
  }
}

/// Link row handler: opens [url], else copies it and tells the user.
Future<void> openLinkOrCopy(BuildContext context, String url, {required String copiedMessage}) async {
  if (await openExternalUrl(url)) return;
  await Clipboard.setData(ClipboardData(text: url));
  if (!context.mounted) return;
  final messenger = ScaffoldMessenger.maybeOf(context);
  messenger?.showSnackBar(SnackBar(content: Text('$copiedMessage\n$url')));
}
