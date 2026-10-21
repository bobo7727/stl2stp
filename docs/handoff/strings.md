# 界面文案全表（自动生成，请勿手改）

> 由 `python3 app/tool/gen_ui_docs.py` 从 `app/assets/i18n/{zh,en,app_zh,app_en}.json` 生成，共 300 个键（设计稿表 200 + App 专用 100）。
> “使用位置”是 `app/lib` 里以字符串字面量出现该键的文件；`preset.${p.name}`、`help.$id.title`、`converting.stage.${st.name}` 这类拼接键标为“动态键”。“—”表示没有直接引用：多数是设计稿为 iOS 原生分享面板等预留的文案，或通过 `${i + 1}` 之类表达式拼出的键（如 `onboarding.2.step1..3`）。

改文案：设计稿表改 `design/ui/strings_*.json` 后运行 `app/tool/sync_strings.sh`；App 专用表改 `app/tool/app_strings.py` 后运行 `python3 tool/app_strings.py`；最后重新运行本脚本。详见 [recipes.md R2](recipes.md#r2-新增或修改一条界面文案)。

## 设计稿文案表（zh.json / en.json，来源 design/ui）

| 键 | 中文 | English | 使用位置 |
|---|---|---|---|
| `a11y.progress` | 转换进度 {pct}% | Conversion progress {pct}% | — |
| `a11y.viewer` | 3D 预览，{name}，可旋转缩放 | 3D preview of {name}, rotatable and zoomable | screens/preview_screen.dart |
| `advanced.merge` | 合并共面 | Merge Coplanar Faces | screens/preview_screen.dart |
| `advanced.merge.desc` | 把同一平面上的三角形合成一个面，平面多的零件文件明显变小 | Merges triangles on the same plane; much smaller files for flat parts | screens/preview_screen.dart |
| `advanced.meta` | 文件信息（可选） | File Info (Optional) | screens/preview_screen.dart |
| `advanced.meta.author` | 作者 | Author | screens/preview_screen.dart（动态键） |
| `advanced.meta.desc` | 写入文件头，在 CAD 软件的文件属性里可以看到。默认留空。 | Written to the file header and shown in CAD file properties. Empty by default. | screens/preview_screen.dart |
| `advanced.meta.description` | 描述 | Description | screens/preview_screen.dart（动态键） |
| `advanced.meta.iges_warn` | IGES 只支持英文字符，中文等字符会被去掉 | IGES supports English (ASCII) characters only; others will be removed | screens/preview_screen.dart |
| `advanced.meta.org` | 单位 / 公司 | Organization | screens/preview_screen.dart（动态键） |
| `advanced.meta.placeholder` | 留空 | Empty | screens/preview_screen.dart, screens/settings_screen.dart |
| `advanced.meta.product` | 产品名称 | Product | screens/preview_screen.dart（动态键） |
| `advanced.mode` | 转换模式 | Mode | screens/preview_screen.dart |
| `advanced.mode.original` | 原版 | Original | screens/preview_screen.dart |
| `advanced.mode.original.desc` | 与电脑版相同的算法，速度快 | Same algorithm as the desktop version, fast | screens/preview_screen.dart（动态键） |
| `advanced.mode.sewing` | 缝合 | Sewing | screens/preview_screen.dart |
| `advanced.mode.sewing.desc` | 把相邻面缝合成实体，更严谨，约慢一倍；模型含多个零件时推荐 | Sews faces into a solid; stricter, ~2× slower; recommended for multiple parts | screens/preview_screen.dart（动态键） |
| `advanced.reset` | 恢复默认 | Restore Defaults | screens/preview_screen.dart |
| `advanced.simplify` | 减少面数 | Reduce Faces | screens/preview_screen.dart |
| `advanced.simplify.desc` | 面数越少，文件越小、转换越快，细节会略有损失 | Fewer faces = smaller file and faster conversion, with slight loss of detail | screens/preview_screen.dart |
| `advanced.simplify.keep` | 保留 {pct}%（约 {faces} 面） | Keep {pct}% (about {faces} faces) | screens/preview_screen.dart |
| `advanced.title` | 高级设置 | Advanced | screens/preview_screen.dart |
| `advanced.verify` | 导出后校验 | Verify After Export | screens/preview_screen.dart |
| `advanced.verify.desc` | 重新读取导出的文件，确认能正常打开 | Re-reads the exported file to make sure it opens | screens/preview_screen.dart |
| `advanced.verify.iges_na` | IGES 暂不支持校验 | Verification is not available for IGES | screens/preview_screen.dart |
| `app.name` | STL2STP | STL2STP | — |
| `app.tagline` | 让网格模型重新可编辑 | Make mesh models editable again | screens/home_screen.dart |
| `cancel.confirm.body` | 当前进度不会保留。 | Progress will be lost. | screens/converting_screen.dart |
| `cancel.confirm.no` | 继续转换 | Keep Converting | screens/converting_screen.dart |
| `cancel.confirm.title` | 确定要取消吗？ | Cancel conversion? | screens/converting_screen.dart |
| `cancel.confirm.yes` | 取消转换 | Cancel Conversion | screens/converting_screen.dart |
| `common.back` | 返回 | Back | screens/error_screen.dart |
| `common.cancel` | 取消 | Cancel | screens/flow.dart, screens/history_screen.dart, screens/memory_dialog.dart, screens/settings_screen.dart |
| `common.close` | 关闭 | Close | — |
| `common.done` | 完成 | Done | screens/done_screen.dart, screens/home_screen.dart |
| `common.example` | 示例 | Example | — |
| `common.learn_more` | 了解更多 | Learn More | — |
| `common.next` | 下一步 | Next | screens/onboarding_screen.dart |
| `common.ok` | 好 | OK | screens/flow.dart, screens/settings_screen.dart |
| `common.placeholder_tag` | 占位 | Placeholder | screens/paywall_screen.dart |
| `common.retry` | 重试 | Try Again | screens/error_screen.dart |
| `common.skip` | 跳过 | Skip | screens/onboarding_screen.dart |
| `common.start` | 开始使用 | Get Started | screens/onboarding_screen.dart |
| `convert.button` | 转换为 {fmt} | Convert to {fmt} | screens/preview_screen.dart |
| `converting.cancel` | 取消转换 | Cancel Conversion | screens/converting_screen.dart |
| `converting.elapsed` | 已用 {t} | Elapsed {t} | screens/converting_screen.dart |
| `converting.keep_open` | 请保持 App 在前台，切到后台可能会中断 | Keep the app open; switching away may interrupt conversion | screens/converting_screen.dart |
| `converting.stage.build` | 生成 CAD 面 | Building CAD faces | screens/converting_screen.dart（动态键） |
| `converting.stage.read` | 读取模型 | Reading model | screens/converting_screen.dart（动态键） |
| `converting.stage.simplify` | 减少面数 | Reducing faces | screens/converting_screen.dart（动态键） |
| `converting.stage.verify` | 校验文件 | Verifying file | screens/converting_screen.dart（动态键） |
| `converting.stage.write` | 写入文件 | Writing file | screens/converting_screen.dart（动态键） |
| `converting.title` | 正在转换 | Converting | screens/converting_screen.dart |
| `done.another` | 再转一个 | Convert Another | screens/done_screen.dart |
| `done.faces` | CAD 面 | CAD Faces | screens/done_screen.dart |
| `done.iges_surfaces` | IGES 打开后是曲面，需要在 CAD 中缝合成实体 | IGES opens as surfaces; sew them into a solid in your CAD app | screens/done_screen.dart |
| `done.partial` | 有 {n} 个三角形无法转换，已跳过 | {n} triangles could not be converted and were skipped | screens/done_screen.dart |
| `done.save` | 存到“文件” | Save to Files | screens/done_screen.dart |
| `done.share` | 分享… | Share… | screens/done_screen.dart |
| `done.size` | 文件大小 | File Size | screens/done_screen.dart |
| `done.solids` | 实体 | Solids | screens/done_screen.dart |
| `done.target_not_reached` | 无法再减少，已减到 {faces} 面 | Could not reduce further; reduced to {faces} faces | screens/done_screen.dart |
| `done.time` | 用时 | Time | screens/done_screen.dart |
| `done.tip_pc` | 发到电脑：用微信“文件传输助手”或隔空投送 | To your computer: use WeChat File Transfer or AirDrop | screens/done_screen.dart |
| `done.title` | 转换完成 | Conversion Complete | screens/done_screen.dart |
| `done.verify_off` | 未校验 | Not verified | screens/done_screen.dart |
| `done.verify_ok` | 校验通过，文件可以正常打开 | Verified: the file opens correctly | screens/done_screen.dart |
| `error.cancelled.action` | 回到预览 | Back to Preview | screens/error_screen.dart |
| `error.cancelled.body` | 转换已取消，没有生成文件。 | Conversion was cancelled. No file was created. | screens/error_screen.dart（动态键） |
| `error.cancelled.title` | 已取消 | Cancelled | screens/error_screen.dart（动态键） |
| `error.choose_other` | 换一个文件 | Choose Another File | screens/error_screen.dart |
| `error.copy_details` | 复制错误详情 | Copy Error Details | screens/error_screen.dart |
| `error.empty.body` | 文件能打开，但里面没有可以转换的三角形。 | The file opened but contains no triangles that can be converted. | screens/error_screen.dart（动态键） |
| `error.empty.title` | 模型里没有可用的面 | No usable faces | screens/error_screen.dart（动态键） |
| `error.format.body` | 它不是有效的 STL 文件，或者已经损坏。请确认导出时选的是 STL 格式。 | It isn’t a valid STL file or it’s damaged. Make sure it was exported as STL. | screens/error_screen.dart（动态键） |
| `error.format.title` | 无法读取这个文件 | Can’t read this file | screens/error_screen.dart（动态键） |
| `error.generic.body` | 出现了意外错误（代码 {code}）。 | An unexpected error occurred (code {code}). | screens/error_screen.dart（动态键） |
| `error.generic.title` | 转换失败 | Conversion failed | screens/error_screen.dart（动态键） |
| `error.io.body` | 存储空间可能不足，请清理后重试。 | Storage may be full. Free up space and try again. | screens/error_screen.dart（动态键） |
| `error.io.title` | 无法保存文件 | Can’t save the file | screens/error_screen.dart（动态键） |
| `error.memory.action` | 减到 50% 再试 | Reduce to 50% and Retry | screens/error_screen.dart |
| `error.memory.body` | 转换时设备内存不够用了。可以减少面数，或关闭其他 App 后重试。 | The device ran out of memory. Reduce faces or close other apps and try again. | screens/error_screen.dart（动态键） |
| `error.memory.title` | 内存不足 | Not enough memory | screens/error_screen.dart（动态键） |
| `error.too_big.action` | 减少面数后再转换 | Reduce Faces and Convert | screens/error_screen.dart |
| `error.too_big.body` | 这个模型有 {faces} 个面，超过了这台设备能处理的上限（约 {limit} 面）。请先减少面数，或用电脑版处理。 | This model has {faces} faces, above this device’s limit (about {limit}). Reduce faces first or use the desktop version. | screens/error_screen.dart（动态键） |
| `error.too_big.title` | 模型太大 | Model too large | screens/error_screen.dart（动态键） |
| `estimate.line` | 预计用时{time}，文件约 {size} | Est. {time}, about {size} | screens/preview_screen.dart |
| `estimate.minutes` | 约 {n} 分钟 | about {n} min | — |
| `estimate.note` | 估算值，实际以设备为准 | Estimate only; varies by device | screens/preview_screen.dart |
| `estimate.seconds` | 约 {n} 秒 | about {n} s | — |
| `estimate.under_1s` | 不到 1 秒 | under 1 s | core/estimate.dart |
| `export.format` | 导出格式 | Export Format | screens/preview_screen.dart |
| `export.format.iges` | IGES | IGES | screens/preview_screen.dart |
| `export.format.iges.desc` | 较老的格式，打开后是一组曲面 | Older format; opens as a set of surfaces | screens/preview_screen.dart |
| `export.format.step` | STEP | STEP | screens/preview_screen.dart |
| `export.format.step.desc` | 通用 CAD 格式，打开后是实体，推荐 | Universal CAD format; opens as a solid. Recommended | screens/preview_screen.dart |
| `history.clear` | 清空记录 | Clear History | screens/history_screen.dart |
| `history.delete` | 删除 | Delete | widgets/history_list.dart |
| `history.empty` | 还没有转换记录 | No conversions yet | screens/history_screen.dart |
| `history.missing` | 文件已不存在 | File no longer exists | screens/home_screen.dart |
| `history.note` | 记录和文件只保存在本机，删除 App 后会一起删除 | History and files stay on this device and are removed with the app | screens/history_screen.dart |
| `history.reshare` | 再次分享 | Share Again | — |
| `history.title` | 转换记录 | History | screens/history_screen.dart, screens/home_screen.dart |
| `home.drop_hint` | 或把 STL 文件拖到这里 | or drop an STL file here | — |
| `home.empty.body` | 转换过的文件会显示在这里，方便再次分享。 | Converted files appear here so you can share them again. | screens/home_screen.dart |
| `home.empty.title` | 还没有转换记录 | No conversions yet | screens/home_screen.dart |
| `home.import` | 导入 STL 文件 | Import STL File | screens/home_screen.dart |
| `home.import_hint` | 支持 ASCII 和二进制 STL | ASCII and binary STL supported | screens/home_screen.dart |
| `home.other_ways` | 从微信 / QQ 导入？ | Importing from WeChat / QQ? | screens/home_screen.dart |
| `home.recent` | 最近转换 | Recent | screens/home_screen.dart |
| `home.see_all` | 全部 | See All | screens/home_screen.dart |
| `home.settings` | 设置 | Settings | screens/home_screen.dart |
| `home.title` | STL2STP | STL2STP | screens/home_screen.dart |
| `import.help.files` | 也可以先存到“文件”，再点“导入 STL 文件”选择。 | Or save it to Files first, then tap “Import STL File”. | screens/home_screen.dart |
| `import.help.step1` | 在聊天里点开 STL 文件 | Open the STL file in the chat | screens/home_screen.dart（动态键） |
| `import.help.step2` | 点右上角“…”，选择“用其他应用打开” | Tap “…” at the top right, then “Open in Another App” | screens/home_screen.dart（动态键） |
| `import.help.step3` | 在列表中选择 STL2STP | Choose STL2STP from the list | screens/home_screen.dart（动态键） |
| `import.help.title` | 从微信 / QQ 导入 | Import from WeChat / QQ | screens/home_screen.dart |
| `import.loading` | 正在读取模型… | Reading model… | screens/flow.dart |
| `licenses.core.license` | 基于原 stl2stp 项目改写，许可证【待确认】 | Derived from the original stl2stp project; license [TBD] | screens/licenses_screen.dart |
| `licenses.core.name` | stl2stp-core | stl2stp-core | screens/licenses_screen.dart |
| `licenses.full_text` | 查看许可证全文 | View Full License Text | — |
| `licenses.occt.compliance_todo` | 【待确认】iOS 静态链接 LGPL 库的合规方式（如提供目标文件以便重新链接）需在上架前确认，最终文本以法务意见为准。 | [TBD] How to comply with LGPL when statically linking on iOS (e.g. offering object files for relinking) must be confirmed before release; final text subject to legal review. | — |
| `licenses.occt.license` | GNU LGPL 2.1，附 Open CASCADE 例外条款 | GNU LGPL 2.1 with Open CASCADE Exception | screens/licenses_screen.dart |
| `licenses.occt.name` | Open CASCADE Technology (OCCT) 8.0.1 | Open CASCADE Technology (OCCT) 8.0.1 | screens/licenses_screen.dart |
| `licenses.occt.notice` | 本 App 使用了 Open CASCADE Technology 软件库（© Open CASCADE SAS），按 GNU 宽通用公共许可证 2.1 版发布。你可以从 https://dev.opencascade.org 获取 OCCT 源代码，许可证全文见下方。 | This app uses the Open CASCADE Technology library (© Open CASCADE SAS), distributed under the GNU Lesser General Public License v2.1. OCCT source code is available at https://dev.opencascade.org. Full license text below. | screens/licenses_screen.dart |
| `licenses.others` | 其他第三方库（视最终框架而定）【待补充】 | Other third-party libraries (depends on final framework) [TBD] | — |
| `licenses.title` | 开源许可 | Open Source Licenses | screens/licenses_screen.dart |
| `onboarding.1.body` | 导入 3D 打印常用的 STL 网格模型，一键转成 STEP 或 IGES，在 SolidWorks、FreeCAD 等软件里继续编辑。 | Import an STL mesh and convert it to STEP or IGES in one tap, then keep editing in SolidWorks, FreeCAD and more. | screens/onboarding_screen.dart |
| `onboarding.1.title` | 把 STL 变成可编辑的 CAD 文件 | Turn STL into editable CAD files | screens/onboarding_screen.dart |
| `onboarding.2.privacy` | 所有文件都在手机本地处理，不会上传。 | Everything is processed on your device. Nothing is uploaded. | screens/onboarding_screen.dart |
| `onboarding.2.step1` | 导入 STL：从“文件”、微信或 QQ 打开 | Import: from Files, WeChat or QQ | screens/onboarding_screen.dart（动态键） |
| `onboarding.2.step2` | 预览并转换：默认设置就能用 | Preview & convert: defaults just work | screens/onboarding_screen.dart（动态键） |
| `onboarding.2.step3` | 分享或存储：发到电脑或存到“文件” | Share or save: send to your computer or Files | screens/onboarding_screen.dart（动态键） |
| `onboarding.2.title` | 三步完成 | Done in three steps | screens/onboarding_screen.dart |
| `paywall.buy` | 购买（价格待定） | Buy (price TBD) | screens/paywall_screen.dart |
| `paywall.feature.iges` | IGES 导出 | IGES export | screens/paywall_screen.dart（动态键） |
| `paywall.feature.large` | 支持大模型 | Large models | screens/paywall_screen.dart（动态键） |
| `paywall.feature.meta` | 自定义文件信息 | Custom file info | screens/paywall_screen.dart（动态键） |
| `paywall.feature.unlimited` | 不限次数转换 | Unlimited conversions | screens/paywall_screen.dart（动态键） |
| `paywall.free_limit` | 免费版：【额度待定】 | Free: [limits TBD] | screens/paywall_screen.dart |
| `paywall.later` | 以后再说 | Not Now | screens/paywall_screen.dart |
| `paywall.placeholder_banner` | 【占位】价格、购买方式和免费额度都还没有确定，这里只是版式示意 | [Placeholder] Price, purchase model and free limits are not decided; layout only | screens/paywall_screen.dart |
| `paywall.restore` | 恢复购买 | Restore Purchases | screens/paywall_screen.dart |
| `paywall.title` | 解锁完整版 | Unlock Full Version | screens/paywall_screen.dart |
| `paywall.trigger_note` | 超出免费额度时出现 | Shown when the free limit is exceeded | — |
| `preset.custom` | 自定义 | Custom | screens/preview_screen.dart, screens/settings_screen.dart（动态键） |
| `preset.recommended` | 推荐 | Recommended | screens/preview_screen.dart, screens/settings_screen.dart（动态键） |
| `preset.recommended.desc` | 与电脑版一致，速度快 | Same as desktop, fast | screens/preview_screen.dart（动态键） |
| `preset.smaller` | 文件更小 | Smaller File | screens/preview_screen.dart, screens/settings_screen.dart（动态键） |
| `preset.smaller.desc` | 减面 50%，合并共面 | 50% fewer faces, merge flat areas | screens/preview_screen.dart（动态键） |
| `preset.strict` | 更严格 | Stricter | screens/preview_screen.dart, screens/settings_screen.dart（动态键） |
| `preset.strict.desc` | 缝合成实体，多个零件时选它，约慢一倍 | Sewn solid, best for multiple parts, ~2× slower | screens/preview_screen.dart（动态键） |
| `preset.title` | 转换方式 | Conversion | screens/preview_screen.dart, screens/settings_screen.dart（动态键） |
| `preview.format.ascii` | ASCII STL | ASCII STL | screens/preview_screen.dart |
| `preview.format.binary` | 二进制 STL | Binary STL | screens/preview_screen.dart |
| `preview.gesture_hint` | 单指旋转 · 双指缩放 · 双击复位 | Drag to rotate · Pinch to zoom · Double-tap to reset | screens/preview_screen.dart |
| `preview.replace` | 换一个文件 | Choose Another File | screens/preview_screen.dart |
| `preview.reset_view` | 复位视角 | Reset View | screens/preview_screen.dart |
| `preview.stats.file_size` | 文件大小 | File Size | screens/preview_screen.dart |
| `preview.stats.size` | 尺寸 | Size | screens/preview_screen.dart |
| `preview.stats.triangles` | 三角面 | Triangles | screens/preview_screen.dart |
| `preview.stats.unit_note` | STL 不带单位，尺寸按文件中的数值显示（通常是 mm） | STL has no units; sizes are shown as stored (usually mm) | screens/preview_screen.dart |
| `preview.stats.vertices` | 顶点 | Vertices | screens/preview_screen.dart |
| `preview.wireframe` | 显示网格线 | Show Wireframe | screens/preview_screen.dart |
| `settings.about` | 关于 | About | screens/settings_screen.dart |
| `settings.about.engine` | 转换引擎：stl2stp-core，基于 Open CASCADE Technology | Engine: stl2stp-core, built on Open CASCADE Technology | screens/settings_screen.dart |
| `settings.about.feedback` | 意见反馈 | Feedback | screens/settings_screen.dart |
| `settings.about.licenses` | 开源许可 | Open Source Licenses | screens/settings_screen.dart |
| `settings.about.privacy` | 隐私 | Privacy | screens/settings_screen.dart |
| `settings.about.privacy.value` | 全部本机处理，不上传 | On-device only, nothing uploaded | screens/settings_screen.dart |
| `settings.about.version` | 版本 | Version | screens/settings_screen.dart |
| `settings.appearance` | 外观 | Appearance | screens/settings_screen.dart |
| `settings.appearance.dark` | 深色 | Dark | screens/settings_screen.dart |
| `settings.appearance.light` | 浅色 | Light | screens/settings_screen.dart |
| `settings.appearance.system` | 跟随系统 | System | screens/settings_screen.dart |
| `settings.clear_history` | 清空转换记录 | Clear Conversion History | screens/settings_screen.dart |
| `settings.default_format` | 默认格式 | Default Format | screens/settings_screen.dart |
| `settings.default_meta` | 默认文件信息 | Default File Info | screens/settings_screen.dart |
| `settings.default_meta.value` | 未设置 | Not set | screens/settings_screen.dart |
| `settings.default_preset` | 默认方式 | Default Preset | screens/settings_screen.dart |
| `settings.defaults` | 默认转换设置 | Default Conversion | screens/settings_screen.dart |
| `settings.language` | 语言 | Language | screens/settings_screen.dart |
| `settings.language.value` | 跟随系统 | System | screens/settings_screen.dart |
| `settings.purchase` | 购买 | Purchase | screens/settings_screen.dart |
| `settings.purchase.free` | 免费版（占位） | Free (placeholder) | screens/settings_screen.dart |
| `settings.purchase.restore` | 恢复购买 | Restore Purchases | screens/settings_screen.dart |
| `settings.purchase.status` | 当前版本 | Current Plan | screens/settings_screen.dart |
| `settings.purchase.upgrade` | 解锁完整版 | Unlock Full Version | — |
| `settings.title` | 设置 | Settings | screens/settings_screen.dart |
| `share.airdrop` | 隔空投送 | AirDrop | — |
| `share.mail` | 邮件 | Mail | — |
| `share.more` | 更多 | More | — |
| `share.qq` | QQ | QQ | — |
| `share.save_to_files` | 存储到“文件” | Save to Files | — |
| `share.wechat` | 微信 | WeChat | — |
| `warn.iges.surfaces` | IGES 在 CAD 中打开是一组曲面，需要缝合后才是实体。需要实体请选 STEP。 | IGES opens as surfaces in CAD and must be sewn to become a solid. Choose STEP if you need a solid. | screens/preview_screen.dart |
| `warn.large.action` | 减到 50% | Reduce to 50% | screens/preview_screen.dart |
| `warn.large.body` | 有 {faces} 个面，转成 STEP 预计约 {size}、需要{time}。建议先减少面数。 | {faces} faces. STEP output is estimated at {size} and {time}. Consider reducing faces first. | screens/preview_screen.dart |
| `warn.large.title` | 模型较大 | Large model | — |

## App 专用文案表（app_zh.json / app_en.json，来源 tool/app_strings.py）

| 键 | 中文 | English | 使用位置 |
|---|---|---|---|
| `est.range_minutes` | 约 {lo}–{hi} 分钟 | {lo}–{hi} min | core/estimate.dart |
| `est.range_seconds` | 约 {lo}–{hi} 秒 | {lo}–{hi} s | core/estimate.dart |
| `est.range_seconds_minutes` | 约 {lo} 秒–{hi} 分钟 | {lo} s–{hi} min | core/estimate.dart |
| `help.estimate.body` | 按面数和所选设置估算，给出一个范围。实际用时取决于设备性能和模型形状；“合并共面”能让平面多的零件变得更小，所以范围会更宽。 | Estimated from the face count and settings, shown as a range. Actual time depends on the device and the shape. Merging coplanar faces shrinks flat-faced parts a lot, so the range is wider. | screens/help_screen.dart（动态键） |
| `help.estimate.title` | 预计用时和大小 | Time and size estimates | screens/help_screen.dart（动态键） |
| `help.faq.a1` | 如果导出的是 IGES，这是正常的，IGES 本来就是曲面格式，需要在 CAD 里缝合；要实体请选 STEP。如果是 STEP，说明 STL 本身不封闭（有破洞），完成页会提示“模型不封闭”，可以先在建模软件里修补，或试试“更严格”。 | For IGES this is normal: IGES is a surface format, sew it in CAD or export STEP instead. For STEP it means the STL is not closed (it has holes). The done screen says so. Repair the mesh first, or try Strict. | screens/help_screen.dart（动态键） |
| `help.faq.a2` | 原版模式会把所有零件放进同一个实体里。选“更严格”（缝合模式），每个零件会成为独立的实体。 | Original mode puts all parts into one solid. Choose Strict (sewing) to get one solid per part. | screens/help_screen.dart（动态键） |
| `help.faq.a3` | STEP 是文本格式，每个三角形面要写好几十行。打开“合并共面”或减少面数能明显变小。 | STEP is text and each triangle face takes dozens of lines. Merge coplanar faces or reduce faces. | screens/help_screen.dart（动态键） |
| `help.faq.a4` | 面数很多时手机内存可能不够。App 会在开始前按面数估算内存，不够时提醒或建议先减少面数；转换中系统提示内存紧张时，会安全停止，不会留下损坏的文件。切到其他 App 时转换还能继续一小段时间（一般约半分钟），大模型请保持 App 在前台。 | Very large meshes can run out of memory on a phone. Before starting, the app estimates the memory from the face count and warns or suggests reducing faces. If the system reports memory pressure during a conversion, it stops cleanly without leaving a broken file. After switching to another app the conversion keeps running for a short while (usually about half a minute); keep the app open for large models. | screens/help_screen.dart（动态键） |
| `help.faq.a5` | IGES 只支持英文字符，中文等字符会被去掉；需要中文请导出 STEP。 | IGES supports English characters only. Other characters are removed. Use STEP to keep them. | screens/help_screen.dart（动态键） |
| `help.faq.a6` | 完成页点“分享…”，发到微信“文件传输助手”、QQ 我的电脑，或用隔空投送、邮件；也可以“存到‘文件’”后用 iCloud 云盘同步。 | On the done screen tap Share… and send it via WeChat/QQ, AirDrop or mail, or save it to Files and sync with iCloud Drive. | screens/help_screen.dart（动态键） |
| `help.faq.q1` | 为什么 CAD 里打开是一堆曲面，不是实体？ | Why does it open as surfaces, not a solid? | screens/help_screen.dart（动态键） |
| `help.faq.q2` | 一个文件里有好几个零件怎么办？ | The file contains several parts | screens/help_screen.dart（动态键） |
| `help.faq.q3` | 文件为什么这么大？ | Why is the file so big? | screens/help_screen.dart（动态键） |
| `help.faq.q4` | 转换很慢或中途退出？ | Conversion is slow or the app quits | screens/help_screen.dart（动态键） |
| `help.faq.q5` | IGES 里写的中文作者名不见了？ | Chinese text disappeared from IGES file info | screens/help_screen.dart（动态键） |
| `help.faq.q6` | 怎么把文件传到电脑？ | How do I get the file onto my computer? | screens/help_screen.dart（动态键） |
| `help.faq.title` | 常见问题 | FAQ | screens/help_screen.dart |
| `help.formats.body` | 导入：STL 网格模型，ASCII 和二进制两种都支持（3D 打印、扫描、建模软件常用的导出格式）。<br>导出：STEP（.step）或 IGES（.igs），两者都能被 SolidWorks、Creo、NX、Fusion 360、FreeCAD 等 CAD 软件打开。 | Import: STL mesh files, both ASCII and binary (the usual export of 3D-printing, scanning and modelling tools).<br>Export: STEP (.step) or IGES (.igs). Both open in SolidWorks, Creo, NX, Fusion 360, FreeCAD and other CAD software. | screens/help_screen.dart（动态键） |
| `help.formats.title` | 支持哪些文件 | Supported files | screens/help_screen.dart（动态键） |
| `help.merge.body` | STL 里一个平面往往由很多三角形组成。打开“合并共面”后，同一平面上的三角形会合成一个 CAD 面：平面多的零件（机加工件、钣金、外壳）文件能小一半以上，在 CAD 里选面、测量也更方便；曲面多的模型（雕塑、扫描件）几乎没有变化，只会多花一点时间。 | In STL one flat face is usually many triangles. With merging on, triangles on the same plane become one CAD face. Parts with many flat faces (machined parts, sheet metal, housings) often shrink by half or more and are easier to select and measure in CAD. Curvy models (sculptures, scans) barely change and just take a little longer. | screens/help_screen.dart（动态键） |
| `help.merge.title` | 合并共面 | Merge coplanar faces | screens/help_screen.dart（动态键） |
| `help.meta.body` | 作者、单位、产品名称、描述会写进文件头，在 CAD 的文件属性里能看到，默认都留空。时间戳自动写当前的 UTC 时间。 | Author, organization, product and description are written into the file header and show up in CAD file properties. All empty by default. The timestamp is the current UTC time. | screens/help_screen.dart（动态键） |
| `help.meta.title` | 文件信息 | File info | screens/help_screen.dart（动态键） |
| `help.mode.body` | 原版模式：按三角形共享的边直接拼成一个壳和一个实体，和电脑版结果一致，速度快。<br>缝合模式：先把每个三角形变成面，再按容差缝合，能处理稍有缝隙的模型，也能把多个零件分别变成实体，但更慢、更耗内存。 | Original: builds one shell and one solid directly from shared triangle edges. Same result as the desktop version, fast.<br>Sewing: turns every triangle into a face and stitches them within a tolerance. Handles small gaps and gives each part its own solid, but is slower and uses more memory. | screens/help_screen.dart（动态键） |
| `help.mode.title` | 原版模式和缝合模式 | Original vs sewing mode | screens/help_screen.dart（动态键） |
| `help.open` | 帮助 | Help | screens/help_screen.dart, screens/home_screen.dart, screens/preview_screen.dart |
| `help.presets.body` | 推荐：与电脑版相同的算法（原版模式），速度快，适合大多数零件。<br>文件更小：先把面数减到 50%，再把同一平面上的三角形合并成一个面，文件明显变小、CAD 里更流畅，细节略有损失。<br>更严格：用“缝合”模式把相邻面缝成实体，更严谨，通常会慢一些；一个文件里有多个零件、或“推荐”方式转出来不是实体时选它。<br>手动改了高级设置后显示为“自定义”。 | Recommended: the same algorithm as the desktop version (original mode). Fast, fine for most parts.<br>Smaller file: reduce faces to 50 %, then merge triangles lying on the same plane into one face. Much smaller files that are smoother in CAD, with slightly less detail.<br>Strict: “sewing” mode stitches neighbouring faces into a solid. More rigorous, usually somewhat slower. Use it for files with several parts, or when the recommended result is not a solid.<br>Any manual change in Advanced settings shows as “Custom”. | screens/help_screen.dart（动态键） |
| `help.presets.title` | 三种转换方式 | The three presets | screens/help_screen.dart（动态键） |
| `help.privacy.body` | 所有转换都在本机完成，文件不会上传到任何服务器，App 也不收集使用数据。转换结果只保存在本机，删除 App 时一起删除。 | All conversion happens on this device. Files are never uploaded and the app collects no usage data. Results are stored only on this device and are removed with the app. | screens/help_screen.dart |
| `help.privacy.title` | 隐私 | Privacy | screens/help_screen.dart（动态键） |
| `help.simplify.body` | 每个三角形都会变成一个 CAD 面，面数越多，文件越大、转换越慢，CAD 打开也越卡。减面会在尽量保持形状的前提下合并相邻三角形，比例越低文件越小，但圆角、文字等细节会变粗糙。建议先用 50% 试试；模型超过 20 万面时 App 会主动提醒。 | Every triangle becomes a CAD face. More faces mean bigger files, slower conversion and a sluggish CAD session. Simplifying merges neighbouring triangles while keeping the shape. A lower ratio gives a smaller file but coarser fillets and lettering. Try 50 % first. The app suggests it for models above 200,000 faces. | screens/help_screen.dart（动态键） |
| `help.simplify.title` | 为什么要减少面数 | Why reduce faces? | screens/help_screen.dart（动态键） |
| `help.step_iges.body` | STEP 是现在通用的 CAD 交换格式，转出来是一个“实体”，可以直接测量、开孔、倒角，推荐优先用它。<br>IGES 是较老的格式，转出来是一组“曲面”，打开后需要在 CAD 里“缝合 / 加厚”才能变成实体；只有对方软件只认 IGES 时再选它。IGES 文件信息只支持英文字符。 | STEP is the modern exchange format. The result is a solid you can measure, cut and fillet right away, so it is recommended.<br>IGES is older. The result is a set of surfaces that must be knitted/sewn into a solid in CAD. Choose it only if the other side needs IGES. IGES file info supports English characters only. | screens/help_screen.dart（动态键） |
| `help.step_iges.title` | STEP 和 IGES 有什么区别 | STEP or IGES? | screens/help_screen.dart（动态键） |
| `help.title` | 使用帮助 | Help | screens/help_screen.dart, screens/settings_screen.dart |
| `help.units.body` | STL 文件不记录单位，App 按文件里的数值原样转换，不做缩放。大多数软件导出 STL 时用毫米，所以 CAD 里打开后通常也是毫米。如果打开后大了或小了 25.4 倍，说明原文件是英寸，在 CAD 里导入时选对单位即可。 | STL files carry no units. The app converts the numbers as they are, without scaling. Most tools export STL in millimetres, so the CAD file is usually in millimetres too. If the part is 25.4 times too big or small, the STL was in inches: pick the right unit when importing into CAD. | screens/help_screen.dart（动态键） |
| `help.units.title` | 尺寸和单位 | Size and units | screens/help_screen.dart（动态键） |
| `help.weld.body` | 有些软件导出 STL 时，本该是同一个点的顶点坐标差了极小的一点（例如球的极点），模型看起来是封闭的，实际上有肉眼看不见的缝，转出来就不是实体。<br>默认打开“合并重合顶点”：距离小于模型对角线百万分之一的顶点会被合成一个点，只处理缝隙边上的点，不会改动正常的薄壁和细小特征，也不会产生退化的三角形。完成页会提示“已自动修复”。<br>关掉后只合并坐标完全相同的点，结果与电脑版完全一致。 | Some programs export STL files where points that should be the same differ by a tiny amount (a sphere pole, for example). The model looks closed but has invisible gaps, so it does not become a solid.<br>Weld is on by default: vertices closer than one millionth of the model diagonal become one point. Only points on such gaps are touched; thin walls and small features are left alone and no degenerate triangles are created. The done screen then says the input was repaired.<br>Turn it off to join only identical coordinates, exactly like the desktop version. | screens/help_screen.dart（动态键） |
| `help.weld.title` | 合并重合顶点 | Weld near-coincident vertices | screens/help_screen.dart（动态键） |
| `purchase.buy_price` | 购买 {price} | Buy for {price} | screens/paywall_screen.dart |
| `purchase.buying` | 正在购买… | Purchasing… | screens/paywall_screen.dart |
| `purchase.cancelled` | 已取消购买 | Purchase cancelled | screens/paywall_screen.dart |
| `purchase.failed` | 购买失败：{msg} | Purchase failed: {msg} | screens/paywall_screen.dart |
| `purchase.free` | 免费版 | Free | — |
| `purchase.limit.daily` | 免费版每天可转换 {limit} 次，今天已用完。 | The free version allows {limit} conversions a day. Today’s are used up. | purchase/free_tier.dart（动态键） |
| `purchase.limit.iges` | IGES 导出属于完整版功能。 | IGES export is a Pro feature. | purchase/free_tier.dart（动态键） |
| `purchase.limit.meta` | 自定义文件信息属于完整版功能。 | Custom file info is a Pro feature. | purchase/free_tier.dart（动态键） |
| `purchase.limit.title` | 需要完整版 | Pro required | purchase/free_tier.dart（动态键） |
| `purchase.limit.triangles` | 免费版最多转换 {limit} 个面的模型，这个模型有 {faces} 个面。可以先减少面数，或解锁完整版。 | The free version converts up to {limit} faces. This model has {faces}. Reduce faces or unlock Pro. | purchase/free_tier.dart（动态键） |
| `purchase.mock_note` | 开发用模拟商店：不会扣费 | Development mock store: no charge | screens/paywall_screen.dart |
| `purchase.nothing_to_restore` | 没有找到可恢复的购买 | No purchases to restore | screens/paywall_screen.dart |
| `purchase.price_tbd` | 价格待定 | Price TBD | — |
| `purchase.pro` | 完整版 | Pro | — |
| `purchase.restored` | 已恢复购买 | Purchase restored | screens/paywall_screen.dart |
| `purchase.restoring` | 正在恢复… | Restoring… | screens/paywall_screen.dart |
| `purchase.status.pro` | 完整版（已解锁） | Pro (unlocked) | screens/paywall_screen.dart, screens/settings_screen.dart |
| `purchase.success` | 已解锁完整版，感谢支持 | Pro unlocked. Thank you. | screens/paywall_screen.dart |
| `purchase.unavailable` | 当前设备无法连接 App Store，稍后再试 | The store is not available right now | screens/paywall_screen.dart |
| `purchase.unlock` | 解锁完整版 | Unlock Pro | — |
| `x.background.expired` | App 在后台停留太久，系统收回了后台时间，转换已停止。请回到 App 后重试。 | The app stayed in the background too long and the system ended its background time, so the conversion stopped. Try again with the app open. | screens/converting_screen.dart |
| `x.clear_confirm` | 确定清空所有转换记录吗？ | Clear all conversion history? | screens/history_screen.dart |
| `x.converting.background` | 可以切到其他 App，转换会在后台继续一小段时间 | You can switch apps; the conversion keeps running for a short while | screens/converting_screen.dart |
| `x.copied` | 已复制 | Copied | screens/done_screen.dart, screens/error_screen.dart, screens/flow.dart |
| `x.copy_path` | 复制路径 | Copy path | — |
| `x.demo_banner` | 演示模式：未找到 stl2stp-core 转换引擎，转换结果只是占位文件。 | Demo mode: stl2stp-core was not found, outputs are placeholders. | screens/home_screen.dart |
| `x.drop_here` | 松开即可导入 | Release to import | widgets/stl_drop.dart |
| `x.drop_not_stl` | 只支持 .stl 文件 | Only .stl files are supported | widgets/stl_drop.dart |
| `x.engine` | 转换引擎 | Engine | screens/settings_screen.dart |
| `x.history.missing_file` | “{name}”已不存在，无法分享。可以把这条记录删掉。 | “{name}” no longer exists and cannot be shared. You can remove this record. | widgets/history_list.dart |
| `x.history.remove` | 删除记录 | Remove | widgets/history_list.dart |
| `x.history.share` | 分享 | Share | widgets/history_list.dart |
| `x.history.swipe_hint` | 向右滑动再次分享，向左滑动删除 | Swipe right to share again, left to delete | screens/history_screen.dart, screens/home_screen.dart |
| `x.language.en` | English | English | screens/settings_screen.dart |
| `x.language.zh` | 中文 | 中文 | screens/settings_screen.dart |
| `x.licenses.all` | 全部开源组件许可 | All open-source licences | screens/licenses_screen.dart |
| `x.licenses.occt_source` | 许可全文随 App 附带：GNU LGPL 2.1 和 Open CASCADE 例外条款（1.0 版），取自 OCCT 8.0.1。 | Full text included with the app: GNU LGPL 2.1 and the Open CASCADE exception (version 1.0), taken from OCCT 8.0.1. | screens/licenses_screen.dart |
| `x.licenses.occt_text` | 查看 OCCT 许可全文 | Read the OCCT licence | screens/licenses_screen.dart |
| `x.licenses.others` | Flutter 框架和 App 用到的 Dart 包（file_picker、share_plus、shared_preferences 等）各自的许可证，以及上面的 OCCT 文本，都在这里。 | Licences of the Flutter framework and the Dart packages the app uses (file_picker, share_plus, shared_preferences and others), plus the OCCT texts above. | screens/licenses_screen.dart |
| `x.memory.continue` | 仍然转换 | Convert anyway | screens/memory_dialog.dart |
| `x.memory.pressure` | 系统提示内存紧张，转换已安全停止，没有生成文件。减少面数后再试。 | The system ran low on memory, so the conversion was stopped safely. No file was written. Reduce faces and try again. | screens/converting_screen.dart |
| `x.memory.refuse.body` | 预计需要约 {need}，超过本机可用的约 {budget}。请先把面数减到 {percent}% 左右再转换。 | About {need} is needed, more than the {budget} available. Reduce faces to about {percent} % first. | screens/memory_dialog.dart |
| `x.memory.refuse.title` | 内存不够 | Not enough memory | screens/memory_dialog.dart |
| `x.memory.simplify` | 减到 {percent}% 再转换 | Reduce to {percent} % | screens/memory_dialog.dart |
| `x.memory.warn.body` | 预计需要约 {need}，本机可用约 {budget}。转换可能变慢，或被系统中止。建议先减少面数。 | About {need} is needed and about {budget} is available. The conversion may be slow or stopped by the system. Reducing faces is recommended. | screens/memory_dialog.dart |
| `x.memory.warn.title` | 这个模型可能占用很多内存 | This model may need a lot of memory | screens/memory_dialog.dart |
| `x.not_solid` | 模型不封闭，已按曲面导出（没有实体） | The model is open, exported as surfaces (no solid) | screens/done_screen.dart |
| `x.open_path.hint` | /path/to/model.stl | /path/to/model.stl | screens/flow.dart |
| `x.open_path.picker_failed` | 系统文件选择器不可用，请直接输入路径。 | The system file picker is unavailable. Type a path instead. | screens/flow.dart |
| `x.open_path.title` | 输入 STL 文件路径 | Enter STL file path | screens/flow.dart |
| `x.output_path` | 输出位置 | Saved at | screens/done_screen.dart, screens/flow.dart |
| `x.repaired` | 输入模型有瑕疵，已自动修复 | The input had defects and was repaired | screens/done_screen.dart |
| `x.reset_purchase` | 重置购买状态（开发用） | Reset purchase (dev only) | screens/settings_screen.dart |
| `x.sample` | 用示例模型试试 | Try a sample model | screens/home_screen.dart |
| `x.saved_to` | 已保存到 {path} | Saved to {path} | screens/flow.dart |
| `x.share_unavailable` | 此平台不支持系统分享，文件路径已复制到剪贴板。 | Sharing is not supported here. The file path was copied. | screens/flow.dart |
| `x.split.placeholder` | 在左边导入 STL，或从记录里分享 | Import an STL on the left, or share from the history | screens/home_screen.dart |
| `x.verify_failed` | 校验未通过，建议换“更严格”方式再转一次 | Verification failed. Try the Strict preset. | screens/done_screen.dart |
| `x.weld` | 合并重合顶点 | Weld near-coincident vertices | screens/preview_screen.dart |
| `x.weld.desc` | 把几乎重合（小于模型尺寸百万分之一）的顶点当成同一个点，修补导出软件留下的微小缝隙。关掉后与电脑版完全一致，只合并坐标完全相同的点。 | Treats vertices closer than one millionth of the model size as one point, closing microscopic gaps left by exporters. Off = exactly like the desktop version (only identical coordinates are joined). | screens/preview_screen.dart |

