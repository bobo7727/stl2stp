# 操作手册（Recipes）

[← 返回总入口](../HANDOFF.md)

> 每个手册列出要改的文件（按顺序）、命令、验收标准。源码路径相对 mobile/；Git 分支从当前 master 建立。命令在 WSL 里执行，环境变量见 [build-test.md](build-test.md) 开头。

## 目录

- [R1 新增一个核心库选项（端到端）](#r1-新增一个核心库选项端到端)
- [R2 新增或修改一条界面文案](#r2-新增或修改一条界面文案)
- [R3 修改预设](#r3-修改预设)
- [R4 重新标定估算和内存系数](#r4-重新标定估算和内存系数)
- [R5 新增一种导出格式](#r5-新增一种导出格式)
- [R6 升级 OCCT 或 Flutter](#r6-升级-occt-或-flutter)
- [R7 修改商品 ID、价格档位或免费额度](#r7-修改商品-id价格档位或免费额度)
- [R8 发布新版本](#r8-发布新版本)

---

## R1 新增一个核心库选项（端到端）

以“新增 `int foo`”为例（参考 `weld_vertices` 是怎么加的：`git log --oneline -- core/src/weld.cpp`，以及 app/flutter 上的 `1eb0bf0`）。

1. **分支**：在最新 master 的克隆中运行 `git switch -c core/foo`。不需要原 ZIP 中的 main/app/flutter 分支。
2. **头文件** `core/include/stl2stp/stl2stp.h`：在 `stl2stp_export_options` **末尾**追加字段并写注释（默认值、范围、单位）。永远不要在中间插入或改已有字段的顺序。
3. **默认值** `core/src/mesh.cpp` 的 `stl2stp_default_export_options()`：按字段顺序补上默认值（这是一个按位置初始化的列表，顺序必须一致）。
4. **参数校验** `core/src/exchange.cpp` 导出入口附近（现有 `sewing_tolerance` / `merge_angular_tolerance` 校验处）：非法值返回 `STL2STP_ERROR_ARGUMENT`，并且在写任何文件之前。
5. **实现**：在 `make_original_shape` / `make_sewing_shape` / `merge_shape` 或导出流程里使用它；需要报告修复就用现有的 `Notes` 追加说明并返回相应警告码。
6. **字符串字段**：在获取锁之前复制成 `std::string`（照 author 等字段的做法）。
7. **CLI** `core/cli/main.cpp`：加 `--foo`（照 `--no-weld` / `--weld-tol` 的写法，检查重复和缺值），更新 `usage()`。
8. **测试**：`core/tests/test_review.cpp` 加用例并在 `core/tests/CMakeLists.txt` 的 review 列表登记；`test_c_api.c` 若默认值需要检查也加上；CLI 测试按需加。
9. **构建验收**：`cmake --build build/rel && ctest --test-dir build/rel -j4` → 全部通过、总数 = 122 + 新增数。
10. **文档**：`docs/core-api.md`（字段表、语义）、`docs/CHANGES-from-original.md`（新的一节）、`docs/core-validation.md`（测试数）。
11. **合并**：先完成以下 Dart/UI 适配和验证，再将 core/foo 合并到 master；当前 master 已同时包含 core 与 app。
12. **Dart 绑定** `app/lib/core/ffi/bindings.dart` 的 `Stl2StpExportOptions`：在**同样的位置**追加同类型字段（`int` → `@Int()`，`double` → `@Double()`，指针 → `Pointer<Utf8>`）。顺序或类型错了会静默写错内存。
13. **设置模型** `app/lib/core/models.dart` 的 `ConvertSettings`：加字段、构造默认值、`copyWith`、`toJson`/`fromJson`（旧 JSON 里没有该键时要有默认值）；如果它应该参与预设匹配，加进 `preset` getter 的比较和 `forPreset`。
14. **传参** `app/lib/core/ffi_engine.dart`：`convert()` 的 `args` Map 加一项；`_convertWorker` 里 `opt.ref..foo = ...`。`DemoEngine` 一般不用改。
15. **界面** `app/lib/screens/preview_screen.dart` 的高级设置区加控件（照 `weldSwitch`），加 Key；需要帮助就在 `help_screen.dart` 的 `helpTopics` 加主题。
16. **文案**：按 R2 加 `x.foo`、`x.foo.desc`、（帮助）`help.foo.title/body`。
17. **估算/内存**：如果影响耗时或内存，更新 `estimate.dart` / `memory_guard.dart` 并按 R4 标定。
18. **App 测试**：`test/features_test.dart` 加一组（默认值、JSON 往返、改动后预设变“自定义”、界面开关），需要真实核心库的加到集成测试。
19. **验收**：`flutter analyze`（无问题）、`flutter test`（55 + 新增全部通过）、集成测试 3 个通过；重新编 `build/core-shared` 后再跑集成测试；更新 [ui-reference.md](ui-reference.md) 的选项表并运行 `python3 app/tool/gen_ui_docs.py`。

## R2 新增或修改一条界面文案

文案有两张表，键不能重复：

| 表 | 来源（唯一真源） | 生成到 | 键前缀约定 |
|---|---|---|---|
| 设计稿表（200 键） | `design/ui/tools/build_strings.py`（生成 `design/ui/strings_zh.json`、`strings_en.json` 和原型用的 `prototype/strings.js`） | `app/assets/i18n/zh.json`、`en.json` | 按页面：`home.*`、`preview.*`、`done.*`… |
| App 专用表（100 键） | `app/tool/app_strings.py` 里的 `S = {key: (中文, English)}` | `app/assets/i18n/app_zh.json`、`app_en.json` | `x.*`、`est.*`、`purchase.*`、`licenses` 补充等 |

步骤（新增文案一般放 App 专用表）：

```bash
cd $R/app
# 1) 编辑 tool/app_strings.py，加一行： 'x.foo': ('中文', 'English'),
python3 tool/app_strings.py
# 2) 代码里使用： S.of(context).t('x.foo')，带参数： t('x.foo', {'n': 3})，文案里写 {n}
# 3) 检查
flutter test test/core_test.dart --name strings      # zh/en 键和占位符一致；代码里引用的字面键都存在；两表不冲突
python3 tool/gen_ui_docs.py     # 更新 docs/handoff/strings.md 和 screen-map.md
```

改设计稿表：改 `design/ui/tools/build_strings.py` 并运行它（`python3 design/ui/tools/build_strings.py`，生成两份 JSON），再在 `app/` 下运行 `bash tool/sync_strings.sh`（打印 `synced 200 keys`，键数随之变化），测试 `assets match design/ui string tables` 会检查两边一致。

文案风格（`design/ui/ui-spec.md §10`）：口语化、短句、少用术语（STEP/IGES/STL 保留英文）、不用感叹号、中文引号用“”。注意测试 `zh and en have the same 200 keys` 写死了设计稿表的键数，改了键数要同时改测试。

## R3 修改预设

1. `app/lib/core/models.dart`：`ConvertSettings.forPreset()` 改参数；如果新增预设，`enum Preset` 加一项。`preset` getter 自动按 `forPreset` 匹配。
2. 文案：`preset.<name>`、`preset.<name>.desc`（设计稿表，或放 App 表），帮助主题 `help.presets.body` 同步描述。
3. 界面：`preview_screen.dart` 的预设卡片按 `Preset.values` 自动生成（新增时检查宽度，三张卡片一行）；设置页“默认方式”下拉也自动包含。
4. 估算：新预设的估算会自动按设置计算，无需改。
5. 测试：`core_test.dart` 的 `presets` 组（默认是推荐、往返、编辑后变自定义）要更新期望值；集成测试里用到 `preset_smaller` 的估算检查。
6. 验收：`flutter test` 全部通过；手动在预览页切换三个预设，确认“自定义”标签只在不匹配时出现。

## R4 重新标定估算和内存系数

**PC 上（形状）**：

```bash
cd $R && cmake --build build/rel                     # 确保 CLI 是最新代码
cd $R/app/tool/calib
python3 calib.py  $R/build/rel/stl2stp-cli $R/build/tmp/calib       # ~15 分钟；机器要空闲，一次只跑一个
python3 calib2.py $R/build/rel/stl2stp-cli $R/build/tmp/calib2      # ~2 分钟
python3 memtime.py $R/build/rel/stl2stp-cli $R/build/tmp/mem $R/app/tool/calib/calib.py
```

1. 把新 CSV 复制回 `app/tool/calib/`（替换 `calib.csv`、`calib2.csv`、`memtime.csv`）。
2. 拟合：时间 ≈ `base + exportPerFace·F (+mergePerTriangle·f) + verifyPerMB·MB`；大小 ≈ `header + bytesPerFace·F`；内存 ≈ `baseBytes + perFace·F`（取上包络，保证模型不低于实测）。用任意工具（Python 最小二乘即可）。
3. 改 `app/lib/core/estimate.dart` 的 `Coefficients` 默认值、`memory_guard.dart` 的 `MemoryGuard` 默认值。
4. 把新测量点更新到 `test/core_test.dart` 的 `estimate (calibrated)` 组（数据行）和 `features_test.dart` 的 `memory guard` 组。
5. 验收：`flutter test` 全部通过（每个点落在范围内，中心误差 ≤ 35% / 20%）；更新 [flutter-app.md §7](../flutter-app.md) 的表。

**手机上（只需一个系数）**（需要 Mac + iPhone）：
1. 在真机上用 3–5 个模型（5k、20k、50k 三角形，STEP 推荐预设）转换，记下完成页的用时和文件大小。
2. `deviceFactor` = 实测用时 ÷ PC 估算中心值的平均；在 `main.dart` 的 `createAppState` 里构造 `AppState(estimator: Estimator(deviceFactor: x))`（`AppState` 有 `estimator` 参数）。
3. 内存：用 Xcode Instruments 或 Debug Navigator 记录同样几次转换的峰值内存，若高于 `MemoryGuard` 模型，按比例加大 `perFace`/`baseBytes`。
4. 验收：真机上估算范围包住实测值；50k 三角形模型不再被系统杀掉，或在预检阶段就被警告。

## R5 新增一种导出格式

（例如 BREP 或 STL 再导出。现有两种是 STEP、IGES。）

1. 核心库：`stl2stp.h` 加 `stl2stp_export_xxx(handle, path, options, callbacks, topology)`；在 `exchange.cpp` 里照 `stl2stp_export_iges` 写（共用构面 + 合并逻辑，持全局锁，`OutputFile` 原子写出，取消检查）；`exchange_disabled.cpp` 加桩。
2. CLI：`convert` 的扩展名分派加 `.xxx`。
3. CTest：导出 + 回读（如有）+ 取消 + 错误路径；CMakeLists 登记。
4. Dart：`bindings.dart` 加函数查找；`models.dart` 的 `ExportFormat` 加一项（标签、扩展名）；`ffi_engine.dart` 的 `_convertWorker` 按 `format` 分派；`verifyEffective` 决定是否校验；元数据规则（是否只能 ASCII）。
5. 界面：`preview_screen.dart` 的格式分段控件（三项以上可能要换成下拉）、格式说明文案、完成页横幅规则（`done_screen.dart`）、设置的默认格式下拉自动包含。
6. 估算：`Coefficients` 加该格式的每面字节数并标定（R4）。
7. 免费额度：如需限制，`FreeTierLimits` 加开关（参照 `igesRequiresPro`）。
8. 验收：CTest、`flutter test`、集成测试里加一次真实转换；外部 CAD 打开。

## R6 升级 OCCT 或 Flutter

**OCCT**：
1. 在 `~/stl2stp-work` 按 [environment.md §4](environment.md#4-occt-801从源码编译) 编新版本，**装到新目录**（如 `~/occt-install-8.x`），不要覆盖现有的。
2. 新 build 目录：`cmake -S core -B build/rel-occtX ... -DOCCT_ROOT=$HOME/occt-install-8.x`，跑 CTest。重点关注：`metadata_*`（OCCT 默认 header 值可能变）、`review_*`（修复逻辑）、IGES 全局参数键名、`Interface_Static` 参数是否改名。
3. 跑 [validation.md §3](validation.md#3-怎样重做外部校验) 全套外部校验，对比输出大小和拓扑。
4. 按 R4 重新标定（OCCT 性能可能变化）。
5. 更新：`app/tool/ios/build_ios_core.sh` 的 URL/SHA256、`app/assets/licenses/occt/`（许可文本若有变）、`licenses.dart` 的版本名、environment.md、build-linux.md、CHANGES。
6. 验收：CTest 全过；外部校验全 PASS；集成测试通过。

**Flutter**：
1. 下载新版 SDK 到新目录（如 `/mnt/d/dev/flutter-linux-3.xx`），用 PATH 切换；不要原地 `flutter upgrade`（保持可回退）。
2. `cd app && flutter pub get && flutter analyze && flutter test`，再跑集成测试；`flutter pub outdated` 看依赖，谨慎升级 `in_app_purchase`、`share_plus`、`file_picker`（这几个 iOS 原生部分变化大）。
3. Mac 上同步换成同版本的 macOS SDK。
4. 更新 environment.md、`pubspec.yaml` 的 `environment.sdk`（如需要）。
5. 验收：analyze 无问题、55 + 3 测试通过、截图无明显变化（重新生成对比）。

## R7 修改商品 ID、价格档位或免费额度

- **商品 ID**：`app/lib/purchase/purchase_service.dart` 的 `PurchaseConfig.proProductId`（默认 `com.stl2stp.app.pro`）。必须与 App Store Connect 里的商品 ID 完全一致；一旦上架不要再改（已购用户的恢复购买依赖它）。如果要传不同配置：`createPurchaseService(prefs, config: PurchaseConfig(proProductId: '...'))`（在 `main.dart` 的 `createAppState` 里）。
- **价格档位**：代码里**没有价格**，在 App Store Connect 里设；App 显示 StoreKit 返回的本地化价格（`ProductInfo.priceLabel`）。
- **免费额度**：在 `main.dart` 的 `createAppState` 里给 `AppState` 传 `limits:`，例如：

```dart
return AppState(
    engine: engine ?? createEngine(), prefs: prefs, outputDir: dir, purchase: purchase,
    memoryBudgetBytes: budget,
    limits: const FreeTierLimits(maxTriangles: 50000, dailyConversions: 3, igesRequiresPro: true));
```

  `maxTriangles` 按减面后的三角形数计算；每日次数按本地日历日、存在 prefs 的 `convDay`/`convCount`。
- **文案**：确定后把 `paywall.free_limit`（“【额度待定】”）、`paywall.buy`（“价格待定”）、权益列表后的“待定”标签和黄色占位横幅改掉（`paywall_screen.dart`、文案表），设置页的“免费版（占位）”同理。
- **测试**：`purchase_test.dart` 的 `free tier` 组；集成测试用模拟商店走一遍。

## R8 发布新版本

1. 确认所有测试通过：CTest 122+、`flutter analyze`、`flutter test`、集成测试。
2. 版本号：改 `app/pubspec.yaml` 的 `version: X.Y.Z+N`（N 每次上传递增）；同时改 `settings_screen.dart` 的 `appVersion`（直到按 [operations.md §4](operations.md#4-版本号与发版流程) 改成自动读取）。
3. 写 `CHANGELOG.md`（根目录，新建）：版本、日期、用户可见变化、核心库变化、已知问题。
4. 从仓库根检查 `git status --short`，按实际路径暂存源码/配置/文档，提交 `git commit -m "release: X.Y.Z"`，需要时打标签 `git tag vX.Y.Z`；生成文件和签名私密文件不入库。
5. iOS（Mac 上）：`flutter build ipa --release` → Xcode Organizer 或 Transporter 上传 → TestFlight 内测 → 提交审核（截图、隐私问卷：不收集数据）。详见 [mac-setup-guide.md §9](../mac-setup-guide.md)。
6. 发版后：确认提交已同步到 GitHub；还可以建立离线备份（`git bundle create stl2stp-vX.Y.Z.bundle --all`），把 bundle 存到另一存储位置。
