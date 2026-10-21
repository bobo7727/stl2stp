import 'package:flutter/material.dart';

import '../l10n/strings.dart';
import '../state/app_state.dart';
import '../theme/tokens.dart';

/// Two onboarding pages (ui-spec §4.1/§4.2).
class OnboardingScreen extends StatefulWidget {
  const OnboardingScreen({super.key});
  @override
  State<OnboardingScreen> createState() => _OnboardingScreenState();
}

class _OnboardingScreenState extends State<OnboardingScreen> {
  final _pc = PageController();
  int _page = 0;

  void _finish() => AppScope.read(context).onboardingSeen = true;

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    return Scaffold(
      appBar: AppBar(actions: [TextButton(onPressed: _finish, child: Text(s.t('common.skip')))]),
      body: SafeArea(
        child: Column(children: [
          Expanded(
            child: PageView(
              controller: _pc,
              onPageChanged: (i) => setState(() => _page = i),
              children: [
                _page1(s, t),
                _page2(s, t),
              ],
            ),
          ),
          Row(mainAxisAlignment: MainAxisAlignment.center, children: [
            for (var i = 0; i < 2; i++)
              AnimatedContainer(
                duration: const Duration(milliseconds: 200),
                margin: const EdgeInsets.all(4),
                width: i == _page ? 22 : 8,
                height: 8,
                decoration: BoxDecoration(
                    color: i == _page ? t.accent : t.label3, borderRadius: BorderRadius.circular(4)),
              ),
          ]),
          Padding(
            padding: const EdgeInsets.all(16),
            child: FilledButton(
              key: const Key('onboardingNext'),
              onPressed: () => _page == 0
                  ? _pc.nextPage(duration: const Duration(milliseconds: 250), curve: Curves.easeOut)
                  : _finish(),
              child: Text(_page == 0 ? s.t('common.next') : s.t('common.start')),
            ),
          ),
        ]),
      ),
    );
  }

  Widget _page1(S s, Tokens t) => Padding(
        padding: const EdgeInsets.all(24),
        child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
          ExcludeSemantics(
            child: Row(mainAxisAlignment: MainAxisAlignment.center, children: [
              _box(t, Icons.change_history, 'STL'),
              Padding(padding: const EdgeInsets.all(12), child: Icon(Icons.arrow_forward, color: t.accent, size: 32)),
              _box(t, Icons.view_in_ar, 'STEP / IGES'),
            ]),
          ),
          const SizedBox(height: 40),
          Text(s.t('onboarding.1.title'),
              textAlign: TextAlign.center, style: TextStyle(fontSize: 28, fontWeight: FontWeight.w700, color: t.label)),
          const SizedBox(height: 12),
          Text(s.t('onboarding.1.body'), textAlign: TextAlign.center, style: TextStyle(fontSize: 17, color: t.label2)),
        ]),
      );

  Widget _box(Tokens t, IconData icon, String label) => Column(children: [
        Container(
          width: 96,
          height: 96,
          decoration: BoxDecoration(color: t.surface, borderRadius: BorderRadius.circular(16)),
          child: Icon(icon, size: 56, color: t.mesh),
        ),
        const SizedBox(height: 8),
        Text(label, style: TextStyle(fontSize: 13, color: t.label2)),
      ]);

  Widget _page2(S s, Tokens t) {
    final colors = [t.accent, t.success, t.warning];
    final icons = [Icons.file_download_outlined, Icons.bolt, Icons.ios_share];
    return Padding(
      padding: const EdgeInsets.all(24),
      child: Column(mainAxisAlignment: MainAxisAlignment.center, crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(s.t('onboarding.2.title'), style: TextStyle(fontSize: 34, fontWeight: FontWeight.w700, color: t.label)),
        const SizedBox(height: 24),
        for (var i = 0; i < 3; i++)
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 10),
            child: Row(children: [
              Container(
                width: 44,
                height: 44,
                decoration: BoxDecoration(color: colors[i], borderRadius: BorderRadius.circular(10)),
                child: Icon(icons[i], color: Colors.white),
              ),
              const SizedBox(width: 14),
              Expanded(child: Text(s.t('onboarding.2.step${i + 1}'), style: TextStyle(fontSize: 17, color: t.label))),
            ]),
          ),
        const SizedBox(height: 24),
        Container(
          padding: const EdgeInsets.all(12),
          decoration: BoxDecoration(color: t.infoBg, borderRadius: BorderRadius.circular(12)),
          child: Row(children: [
            Icon(Icons.lock_outline, color: t.infoFg),
            const SizedBox(width: 8),
            Expanded(child: Text(s.t('onboarding.2.privacy'), style: TextStyle(color: t.infoFg))),
          ]),
        ),
      ]),
    );
  }
}
