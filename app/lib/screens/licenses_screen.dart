import 'package:flutter/material.dart';

import '../l10n/strings.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';

/// Open-source licences (ui-spec §4.15).
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
            InfoBanner(kind: BannerKind.warn, text: s.t('licenses.occt.compliance_todo')),
            TextButton(
              onPressed: () => showLicensePage(context: context, applicationName: 'STL2STP'),
              child: Text(s.t('licenses.full_text')),
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
        Surface(child: Text(s.t('licenses.others'), style: b)),
      ]),
    );
  }
}
