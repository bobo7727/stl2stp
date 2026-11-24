import 'package:flutter/services.dart';
import 'package:flutter/material.dart';
import 'package:url_launcher/url_launcher.dart';

/// Public pages App Review needs to reach from inside the app (guideline 5.1.1(i)).
///
/// These must stay identical to the Privacy Policy URL and Support URL entered
/// in App Store Connect. Both pages are bilingual (GitHub Pages, HTTPS).
const privacyPolicyUrl = 'https://bobo7727.github.io/stl2stp-support/privacy.html';
const supportUrl = 'https://bobo7727.github.io/stl2stp-support/';

/// True while a link still points at a reserved `.invalid` placeholder host.
bool get linksArePlaceholders =>
    Uri.parse(privacyPolicyUrl).host.endsWith('.invalid') || Uri.parse(supportUrl).host.endsWith('.invalid');

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
