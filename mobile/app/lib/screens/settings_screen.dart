import 'package:flutter/material.dart';

import '../core/models.dart';
import '../l10n/strings.dart';
import '../purchase/purchase.dart';
import '../state/app_state.dart';
import '../theme/tokens.dart';
import '../widgets/common.dart';
import 'history_screen.dart';
import 'help_screen.dart';
import 'licenses_screen.dart';
import 'paywall_screen.dart';

const appVersion = '0.1.0';

/// Settings (ui-spec §4.13).
class SettingsScreen extends StatelessWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final s = S.of(context);
    final t = Tokens.of(context);
    final st = AppScope.of(context);
    Widget group(List<Widget> children) => ClipRRect(
          borderRadius: BorderRadius.circular(12),
          child: Column(children: [
            for (final (i, c) in children.indexed) ...[if (i > 0) const Divider(indent: 16), c],
          ]),
        );
    final presetName = st.defaultPreset == 'last' ? (s.isZh ? '上次使用' : 'Last used') : s.t('preset.${st.defaultPreset}');
    final meta = st.defaultMeta;
    final metaSet = [meta.author, meta.organization, meta.product, meta.description].any((x) => x.isNotEmpty);
    return Scaffold(
      appBar: AppBar(title: Text(s.t('settings.title'))),
      body: ListView(padding: const EdgeInsets.fromLTRB(16, 0, 16, 32), children: [
        SectionHeader(s.t('settings.purchase')),
        Container(
          decoration: BoxDecoration(
              border: Border.all(color: t.warning, style: BorderStyle.solid), borderRadius: BorderRadius.circular(12)),
          child: group([
            ListTile(
              key: const Key('purchaseStatus'),
              title: Text(s.t('settings.purchase.status')),
              trailing: Row(mainAxisSize: MainAxisSize.min, children: [
                Text(st.isPro ? s.t('purchase.status.pro') : s.t('settings.purchase.free'),
                    style: TextStyle(color: st.isPro ? t.success : t.label2)),
                Icon(Icons.chevron_right, color: t.label3),
              ]),
              onTap: () => Navigator.push(context, MaterialPageRoute(builder: (_) => const PaywallScreen())),
            ),
            ListTile(
              key: const Key('settingsRestore'),
              title: Text(s.t('settings.purchase.restore'), style: TextStyle(color: t.accent)),
              onTap: () async {
                final r = await st.purchase.restore();
                if (context.mounted) showPurchaseResult(context, r);
              },
            ),
            if (st.purchase is MockPurchaseService)
              ListTile(
                title: Text(s.t('x.reset_purchase'), style: TextStyle(color: t.label2)),
                onTap: () => (st.purchase as MockPurchaseService).reset(forgetOwned: true),
              ),
          ]),
        ),
        SectionHeader(s.t('settings.defaults')),
        group([
          ListTile(
            title: Text(s.t('settings.default_format')),
            trailing: DropdownButton<ExportFormat>(
              value: st.defaultFormat,
              underline: const SizedBox(),
              items: [for (final f in ExportFormat.values) DropdownMenuItem(value: f, child: Text(f.label))],
              onChanged: (f) => st.defaultFormat = f!,
            ),
          ),
          ListTile(
            title: Text(s.t('settings.default_preset')),
            trailing: DropdownButton<String>(
              value: st.defaultPreset,
              underline: const SizedBox(),
              items: [
                for (final p in Preset.values) DropdownMenuItem(value: p.name, child: Text(s.t('preset.${p.name}'))),
                DropdownMenuItem(value: 'last', child: Text(s.isZh ? '上次使用' : 'Last used')),
              ],
              onChanged: (v) => st.defaultPreset = v!,
            ),
            subtitle: Text(presetName, style: TextStyle(fontSize: 12, color: t.label3)),
          ),
          ListTile(
            title: Text(s.t('settings.default_meta')),
            trailing: Row(mainAxisSize: MainAxisSize.min, children: [
              Text(metaSet ? meta.author.isNotEmpty ? meta.author : '…' : s.t('settings.default_meta.value'),
                  style: TextStyle(color: t.label2)),
              Icon(Icons.chevron_right, color: t.label3),
            ]),
            onTap: () => _editMeta(context, st),
          ),
        ]),
        SectionHeader(s.t('settings.appearance')),
        group([
          ListTile(
            title: Text(s.t('settings.appearance')),
            trailing: DropdownButton<ThemeMode>(
              value: st.themeMode,
              underline: const SizedBox(),
              items: [
                DropdownMenuItem(value: ThemeMode.system, child: Text(s.t('settings.appearance.system'))),
                DropdownMenuItem(value: ThemeMode.light, child: Text(s.t('settings.appearance.light'))),
                DropdownMenuItem(value: ThemeMode.dark, child: Text(s.t('settings.appearance.dark'))),
              ],
              onChanged: (m) => st.themeMode = m!,
            ),
          ),
          ListTile(
            title: Text(s.t('settings.language')),
            trailing: DropdownButton<String>(
              key: const Key('languageDropdown'),
              value: st.language,
              underline: const SizedBox(),
              items: [
                DropdownMenuItem(value: 'zh', child: Text(s.t('x.language.zh'))),
                DropdownMenuItem(value: 'en', child: Text(s.t('x.language.en'))),
                DropdownMenuItem(value: 'system', child: Text(s.t('settings.language.value'))),
              ],
              onChanged: (v) => st.language = v!,
            ),
          ),
        ]),
        SectionHeader(s.t('settings.about')),
        group([
          ListTile(
            key: const Key('settingsHelp'),
            title: Text(s.t('help.title')),
            trailing: Icon(Icons.chevron_right, color: t.label3),
            onTap: () => openHelp(context),
          ),
          ListTile(title: Text(s.t('settings.about.version')), trailing: Text(appVersion, style: TextStyle(color: t.label2))),
          ListTile(
              title: Text(s.t('settings.about.privacy')),
              trailing: Text(s.t('settings.about.privacy.value'), style: TextStyle(color: t.label2))),
          ListTile(
              title: Text(s.t('x.engine')),
              trailing: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 200),
                  child: Text(st.engine.name, overflow: TextOverflow.ellipsis, style: TextStyle(color: t.label2)))),
          ListTile(
            title: Text(s.t('settings.about.licenses')),
            trailing: Icon(Icons.chevron_right, color: t.label3),
            onTap: () => Navigator.push(context, MaterialPageRoute(builder: (_) => const LicensesScreen())),
          ),
          ListTile(title: Text(s.t('settings.about.feedback')), trailing: Text('—', style: TextStyle(color: t.label2))),
        ]),
        Padding(
          padding: const EdgeInsets.all(12),
          child: Text(s.t('settings.about.engine'), textAlign: TextAlign.center, style: TextStyle(fontSize: 12, color: t.label2)),
        ),
        group([
          ListTile(
            title: Text(s.t('settings.clear_history'), textAlign: TextAlign.center, style: TextStyle(color: t.danger)),
            onTap: () => confirmClearHistory(context),
          ),
        ]),
      ]),
    );
  }

  Future<void> _editMeta(BuildContext context, AppState st) async {
    final s = S.of(context);
    final m = st.defaultMeta;
    final cs = [m.author, m.organization, m.product, m.description].map((v) => TextEditingController(text: v)).toList();
    final keys = ['author', 'org', 'product', 'description'];
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(s.t('settings.default_meta')),
        content: Column(mainAxisSize: MainAxisSize.min, children: [
          for (var i = 0; i < 4; i++)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 4),
              child: TextField(
                controller: cs[i],
                maxLength: 200,
                decoration: InputDecoration(
                    labelText: s.t('advanced.meta.${keys[i]}'),
                    hintText: s.t('advanced.meta.placeholder'),
                    counterText: '',
                    border: const OutlineInputBorder(),
                    isDense: true),
              ),
            ),
        ]),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(s.t('common.cancel'))),
          TextButton(onPressed: () => Navigator.pop(ctx, true), child: Text(s.t('common.ok'))),
        ],
      ),
    );
    if (ok == true) {
      st.defaultMeta = ConvertSettings(
          author: cs[0].text.trim(), organization: cs[1].text.trim(), product: cs[2].text.trim(), description: cs[3].text.trim());
    }
  }
}
