# 页面 / 组件 → 文案键与 widget Key（自动生成，请勿手改）

> 由 `python3 app/tool/gen_ui_docs.py` 扫描 `app/lib/screens` 和 `app/lib/widgets` 生成。文案键指 `t('...')` 的字面参数（带 `$` 的是拼接键）；widget Key 指 `Key('...')`，测试用 `find.byKey` 定位。文案内容见 [strings.md](strings.md)。

## `app/lib/screens/converting_screen.dart`

- 文案键（8）：`cancel.confirm.body`、`cancel.confirm.no`、`cancel.confirm.title`、`cancel.confirm.yes`、`converting.cancel`、`converting.elapsed`、`converting.stage.${st.name}`、`converting.title`
- widget Key（2）：`cancelButton`、`cancelYes`

## `app/lib/screens/done_screen.dart`

- 文案键（20）：`common.done`、`done.another`、`done.faces`、`done.iges_surfaces`、`done.partial`、`done.save`、`done.share`、`done.size`、`done.solids`、`done.target_not_reached`、`done.time`、`done.tip_pc`、`done.title`、`done.verify_off`、`done.verify_ok`、`x.copied`、`x.not_solid`、`x.output_path`、`x.repaired`、`x.verify_failed`
- widget Key（3）：`doneButton`、`doneTitle`、`shareButton`

## `app/lib/screens/error_screen.dart`

- 文案键（10）：`common.back`、`common.retry`、`error.$key.body`、`error.$key.title`、`error.cancelled.action`、`error.choose_other`、`error.copy_details`、`error.memory.action`、`error.too_big.action`、`x.copied`
- widget Key（3）：`errorNotice`、`errorPrimary`、`errorTitle`

## `app/lib/screens/flow.dart`

- 文案键（10）：`common.cancel`、`common.ok`、`import.loading`、`x.copied`、`x.open_path.hint`、`x.open_path.picker_failed`、`x.open_path.title`、`x.output_path`、`x.saved_to`、`x.share_unavailable`
- widget Key（1）：`pathField`

## `app/lib/screens/help_screen.dart`

- 文案键（8）：`help.$id.body`、`help.$id.title`、`help.faq.a$i`、`help.faq.q$i`、`help.faq.title`、`help.open`、`help.privacy.body`、`help.title`
- widget Key（1）：`faq$i`

## `app/lib/screens/history_screen.dart`

- 文案键（7）：`common.cancel`、`history.clear`、`history.empty`、`history.note`、`history.title`、`x.clear_confirm`、`x.history.swipe_hint`
- widget Key（0）：无

## `app/lib/screens/home_screen.dart`

- 文案键（22）：`$i`、`app.tagline`、`common.done`、`help.open`、`history.missing`、`history.title`、`home.empty.body`、`home.empty.title`、`home.import`、`home.import_hint`、`home.other_ways`、`home.recent`、`home.see_all`、`home.settings`、`home.title`、`import.help.files`、`import.help.step$i`、`import.help.title`、`x.demo_banner`、`x.history.swipe_hint`、`x.sample`、`x.split.placeholder`
- widget Key（6）：`detailPlaceholder`、`helpButton`、`importCard`、`sampleButton`、`settingsButton`、`splitLeft`

## `app/lib/screens/licenses_screen.dart`

- 文案键（10）：`licenses.core.license`、`licenses.core.name`、`licenses.occt.license`、`licenses.occt.name`、`licenses.occt.notice`、`licenses.title`、`x.licenses.all`、`x.licenses.occt_source`、`x.licenses.occt_text`、`x.licenses.others`
- widget Key（3）：`allLicenses`、`occtLicenseBody`、`occtLicenseText`

## `app/lib/screens/memory_dialog.dart`

- 文案键（3）：`common.cancel`、`x.memory.continue`、`x.memory.simplify`
- widget Key（3）：`memoryContinue`、`memoryDialog`、`memorySimplify`

## `app/lib/screens/onboarding_screen.dart`

- 文案键（8）：`common.next`、`common.skip`、`common.start`、`onboarding.1.body`、`onboarding.1.title`、`onboarding.2.privacy`、`onboarding.2.step${i + 1}`、`onboarding.2.title`
- widget Key（1）：`onboardingNext`

## `app/lib/screens/paywall_screen.dart`

- 文案键（19）：`common.placeholder_tag`、`paywall.buy`、`paywall.feature.$k`、`paywall.free_limit`、`paywall.later`、`paywall.placeholder_banner`、`paywall.restore`、`paywall.title`、`purchase.buy_price`、`purchase.buying`、`purchase.cancelled`、`purchase.failed`、`purchase.mock_note`、`purchase.nothing_to_restore`、`purchase.restored`、`purchase.restoring`、`purchase.status.pro`、`purchase.success`、`purchase.unavailable`
- widget Key（2）：`buyButton`、`restoreButton`

## `app/lib/screens/preview_screen.dart`

- 文案键（48）：`a11y.viewer`、`advanced.merge`、`advanced.merge.desc`、`advanced.meta`、`advanced.meta.$key`、`advanced.meta.desc`、`advanced.meta.iges_warn`、`advanced.meta.placeholder`、`advanced.mode`、`advanced.mode.${_s.mode.name}.desc`、`advanced.mode.original`、`advanced.mode.sewing`、`advanced.reset`、`advanced.simplify`、`advanced.simplify.desc`、`advanced.simplify.keep`、`advanced.title`、`advanced.verify`、`advanced.verify.desc`、`advanced.verify.iges_na`、`convert.button`、`estimate.line`、`estimate.note`、`export.format`、`export.format.iges`、`export.format.step`、`help.open`、`preset.${p.name}`、`preset.${p.name}.desc`、`preset.${preset.name}`、`preset.custom`、`preset.title`、`preview.format.ascii`、`preview.format.binary`、`preview.gesture_hint`、`preview.replace`、`preview.reset_view`、`preview.stats.file_size`、`preview.stats.size`、`preview.stats.triangles`、`preview.stats.unit_note`、`preview.stats.vertices`、`preview.wireframe`、`warn.iges.surfaces`、`warn.large.action`、`warn.large.body`、`x.weld`、`x.weld.desc`
- widget Key（8）：`advancedToggle`、`convertButton`、`estimateLine`、`formatSegment`、`preset_${p.name}`、`previewHelp`、`simplifySwitch`、`weldSwitch`

## `app/lib/screens/settings_screen.dart`

- 文案键（35）：`advanced.meta.placeholder`、`common.cancel`、`common.ok`、`help.title`、`preset.${p.name}`、`preset.${st.defaultPreset}`、`purchase.status.pro`、`settings.about`、`settings.about.engine`、`settings.about.feedback`、`settings.about.licenses`、`settings.about.privacy`、`settings.about.privacy.value`、`settings.about.version`、`settings.appearance`、`settings.appearance.dark`、`settings.appearance.light`、`settings.appearance.system`、`settings.clear_history`、`settings.default_format`、`settings.default_meta`、`settings.default_meta.value`、`settings.default_preset`、`settings.defaults`、`settings.language`、`settings.language.value`、`settings.purchase`、`settings.purchase.free`、`settings.purchase.restore`、`settings.purchase.status`、`settings.title`、`x.engine`、`x.language.en`、`x.language.zh`、`x.reset_purchase`
- widget Key（4）：`languageDropdown`、`purchaseStatus`、`settingsHelp`、`settingsRestore`

## `app/lib/widgets/common.dart`

- 文案键（0）：无
- widget Key（0）：无

## `app/lib/widgets/history_list.dart`

- 文案键（4）：`history.delete`、`x.history.missing_file`、`x.history.remove`、`x.history.share`
- widget Key（2）：`history:${entry.outputPath}`、`historyMissingSnack`

## `app/lib/widgets/mesh_viewer.dart`

- 文案键（0）：无
- widget Key（0）：无

## `app/lib/widgets/stl_drop.dart`

- 文案键（2）：`x.drop_here`、`x.drop_not_stl`
- widget Key（0）：无

