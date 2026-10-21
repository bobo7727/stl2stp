import 'package:flutter/material.dart';

import '../core/licenses.dart';
import '../l10n/strings.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';

/// Open-source licences (ui-spec §4.15): OCCT (LGPL-2.1 + OCCT exception, full
/// text bundled), stl2stp-core, and every Flutter/Dart package via
/// [showLicensePage].
class LicensesScreen extends StatelessWidget {
  const LicensesScreen({super.key});
  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    TextStyle h = TextStyle(fontSize: 17, fontWeight: FontWeight.w600, color: t.label);
    TextStyle b = TextStyle(fontSize: 13, color: t.label2);
    return Scaffold(
      appBar: AppBar(title: Text(s.t('licenses.title'))),
      body: ListView(padding: const EdgeInsets.all(16), children: [
        Surface(
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Text(s.t('licenses.occt.name'), style: h),
            Text(s.t('licenses.occt.license'), style: b),
            const SizedBox(height: 8),
            Text(s.t('licenses.occt.notice'), style: b),
            const SizedBox(height: 8),
            Text(s.t('x.licenses.occt_source'), style: b),
            TextButton(
              key: const Key('occtLicenseText'),
              onPressed: () => Navigator.push(context, MaterialPageRoute(builder: (_) => const OcctLicenseScreen())),
              child: Text(s.t('x.licenses.occt_text')),
            ),
          ]),
        ),
        const SizedBox(height: 12),
        Surface(
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Text(s.t('licenses.core.name'), style: h),
            Text(s.t('licenses.core.license'), style: b),
          ]),
        ),
        const SizedBox(height: 12),
        Surface(
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Text(s.t('x.licenses.others'), style: b),
            TextButton(
              key: const Key('allLicenses'),
              onPressed: () {
                registerAppLicenses();
                showLicensePage(context: context, applicationName: 'STL2STP');
              },
              child: Text(s.t('x.licenses.all')),
            ),
          ]),
        ),
      ]),
    );
  }
}

/// Full OCCT licence text (LGPL-2.1 and the OCCT LGPL exception).
class OcctLicenseScreen extends StatefulWidget {
  const OcctLicenseScreen({super.key});
  @override
  State<OcctLicenseScreen> createState() => _OcctLicenseScreenState();
}

class _OcctLicenseScreenState extends State<OcctLicenseScreen> {
  late final Future<String> _text = loadOcctLicenseText();
  @override
  Widget build(BuildContext context) {
    final t = Tokens.of(context);
    return Scaffold(
      appBar: AppBar(title: const Text(occtPackageName, overflow: TextOverflow.ellipsis)),
      body: FutureBuilder<String>(
        future: _text,
        builder: (context, snap) => snap.hasData
            ? SingleChildScrollView(
                padding: const EdgeInsets.all(16),
                child: SelectableText(snap.data!,
                    key: const Key('occtLicenseBody'),
                    style: TextStyle(fontSize: 12, fontFamily: 'monospace', color: t.label)))
            : const Center(child: CircularProgressIndicator()),
      ),
    );
  }
}
