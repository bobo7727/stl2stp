# Flutter App（`app/`）说明

> 分支 `app/flutter`。界面按 `design/ui/ui-spec.md` 实现，文案来自 `design/ui/strings_{zh,en}.json`（默认中文，可切换英文）。
> 转换引擎是本仓库的 `core/`（stl2stp-core），通过 `dart:ffi` 调用 C API `core/include/stl2stp/stl2stp.h`。

## 1. 目录结构

```
app/
├── pubspec.yaml                 依赖：ffi、path、path_provider、shared_preferences、file_picker、share_plus、in_app_purchase、desktop_drop
├── assets/i18n/zh.json, en.json 设计文案（从 design/ui 复制，tool/sync_strings.sh 同步）
├── assets/i18n/app_zh.json, app_en.json  App 专用文案（帮助页、购买、估算等；tool/app_strings.py 生成）
├── lib/
│   ├── main.dart                启动：选引擎（FFI 优先，找不到核心库就进演示模式）、读设置、命令行参数打开 STL
│   ├── app.dart                 MaterialApp、主题、语言、首次引导
│   ├── core/
│   │   ├── ffi/bindings.dart    手写的 FFI 绑定（结构体顺序与 stl2stp.h 一致）
│   │   ├── ffi/native_library.dart  各平台加载核心库的位置
│   │   ├── ffi_engine.dart      真实引擎：后台 isolate 里跑 load → simplify → export → read_step
│   │   ├── demo_engine.dart     演示引擎：不调原生代码，只模拟进度（测试和未编核心库的平台用）
│   │   ├── engine.dart          引擎接口 + 取消标志
│   │   ├── models.dart          设置、预设、阶段、结果、状态码（与 stl2stp_status 对应）
│   │   ├── stl_reader.dart      Dart 端轻量 STL 解析：3D 预览、ASCII/二进制标签、共面分组计数（估算用）
│   │   ├── estimate.dart        预计用时/大小（用核心库实测标定的系数，输出范围；见 §7）
│   │   ├── sample_mesh.dart     示例模型（L 形支架、球）生成器
│   │   └── screenshot.dart      集成测试里截图用
│   ├── l10n/strings.dart        S.of(context).t('key', {参数})
│   ├── theme/tokens.dart        ui-spec §6 的颜色、字号、圆角
│   ├── purchase/                购买框架：PurchaseService 接口、免费额度、模拟商店、StoreKit 实现（见 §8）
│   ├── state/                   AppState（设置、历史、输出目录、购买/额度、估算器）、历史记录 JSON
│   ├── screens/                 引导、首页、预览+设置、转换中、完成、错误、历史、设置、帮助、购买页、开源许可
│   └── widgets/                 3D 预览（纯 Flutter 软件渲染）、桌面拖放、提醒条、统计格、进度环等
├── test/                        单元测试 + 组件测试（用演示引擎，任何机器都能跑）
├── integration_test/app_test.dart  端到端：真机/桌面上用真实核心库走完整 UI 流程并截图
├── tool/sync_strings.sh         同步设计文案
├── tool/app_strings.py          App 专用文案的唯一来源（生成 assets/i18n/app_*.json）
├── tool/calib/                  估算标定脚本和实测数据（calib*.py、calib*.csv）
├── tool/ios/build_ios_core.sh   在 Mac 上编 iOS 版 OCCT + 核心库（见 docs/mac-setup-guide.md）
└── android/ ios/ linux/ macos/ windows/   各平台工程（flutter create 生成，少量改动）
```

各平台的改动：
- `linux/CMakeLists.txt`：把预编译的 `libstl2stp-core.so` 一起装进 bundle 的 `lib/`。默认找 `<仓库>/build/core-shared/libstl2stp-core.so`，可用环境变量 `STL2STP_CORE_LIB` 指定；找不到时 App 仍能运行（演示模式）。
- `linux/runner/my_application.cc`：窗口默认 430×900，标题 STL2STP，像手机一样预览。
- `ios/Runner/Info.plist`：显示名 STL2STP，声明可打开 `.stl`（“用其他应用打开”），中文为开发语言。
- `ios/Flutter/Debug|Release.xcconfig`：可选包含 `Stl2StpCore.xcconfig`（由 `build_ios_core.sh` 生成，负责链接静态库）。没有这个文件时 iOS 工程照常编译，运行时进演示模式。
- `macos/Runner/*.entitlements`：允许读写用户选择的文件。

## 2. 线程和 FFI 设计

- 所有 C 调用都在后台 isolate 里执行，一个 handle 只在一个 isolate 里用，UI 不会卡。
- **进度**：C 回调在同一个工作线程上同步调用 Dart 静态函数（`Pointer.fromFunction`），函数里节流后通过 `SendPort` 发回 UI；UI 再按 ui-spec §4.7 的阶段权重（读取 10、减面 10、生成面 50、写入 20、校验 10）合成总进度，最多每秒刷新 20 次。`export_step/iges` 只有一个回调，前 70% 记为“生成 CAD 面”，后 30% 记为“写入文件”。
- **取消**：UI 在原生内存里分配一个 `int32` 标志，把地址传给工作 isolate；C 的取消回调直接读这个地址。工作 isolate 阻塞在 C 里也能及时取消（不依赖消息循环）。
- **加载库**：iOS 静态链接进 Runner，用 `DynamicLibrary.process()`；Android 用 `libstl2stp-core.so`；Linux 用 bundle 里的 `lib/libstl2stp-core.so`；Windows 用 `stl2stp-core.dll`；macOS 先查进程内符号，再查 `Frameworks/libstl2stp-core.dylib`。环境变量 `STL2STP_CORE_LIB` 可以覆盖。`STL2STP_DEMO=1` 强制演示模式。
- **文件信息**：选 IGES 时 App 先去掉非 ASCII 字符（ui-spec §4.6，界面会提醒）；时间戳传 NULL，由核心库写当前 UTC。
- **校验**：只对 STEP，导出后用 `stl2stp_read_step` 读回，面数 > 0 且实体 ≥ 1（或核心库报告“非实体”）算通过。
- **减面后再改比例**：每次转换都会重新 `load_stl`，所以不需要“恢复原网格”接口（ui-spec §8 待办 3 在 App 层已绕开）。

## 3. 在本机（Windows + WSL）运行

当前 Windows 上没有 Visual Studio C++ 工具链，也没有 Windows 版 OCCT，所以 **Windows 桌面版暂时编不了**。预览用 **WSL Ubuntu 20.04 的 Linux 桌面版**（WSLg 显示窗口），它直接复用 WSL 里已编好的 OCCT（`~/occt-install`）和核心库，转换是真实的。

已安装：
- Linux 版 Flutter 3.38.7：`/mnt/d/dev/flutter-linux`（D 盘，不改 `~/.bashrc`，用时临时加 PATH）
- pub 缓存：`/mnt/d/dev/pub-cache`（每条命令前设 `PUB_CACHE`）
- WSL 里已有 clang 10、cmake 3.26、ninja、pkg-config、libgtk-3-dev

### 3.1 编核心库（共享库版）

```bash
cd /mnt/d/stl2stp-flutter           # app/flutter 分支的 worktree
TMPDIR=$PWD/build/tmp cmake -S core -B build/core-shared -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DOCCT_ROOT=$HOME/occt-install -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF
cmake --build build/core-shared --parallel 8
# 产物：build/core-shared/libstl2stp-core.so（RPATH 指向 ~/occt-install/lib）
```

### 3.2 测试、编译、运行

```bash
export PATH=/mnt/d/dev/flutter-linux/bin:$PATH PUB_CACHE=/mnt/d/dev/pub-cache
cd /mnt/d/stl2stp-flutter/app
flutter pub get
flutter analyze
flutter test                                  # 单元 + 组件测试（演示引擎）
flutter build linux --release                 # 产物 build/linux/x64/release/bundle/
build/linux/x64/release/bundle/stl2stp_app    # 打开窗口（WSLg）
build/linux/x64/release/bundle/stl2stp_app /path/model.stl   # 直接打开一个 STL
# 端到端（真实核心库，自动点完整流程并截图）：
flutter test integration_test/app_test.dart -d linux --dart-define=SHOT_DIR=$PWD/../build/app-shots
```

说明：
- WSLg 下系统文件选择器（XDG 门户）可能不可用，点“导入 STL 文件”失败时会弹出输入框，直接粘贴路径即可（Windows 的 `D:\a.stl` 在 WSL 里是 `/mnt/d/a.stl`）。桌面版首页还有“用示例模型试试”按钮。
- 桌面版没有系统分享面板，“分享…”会把文件路径复制到剪贴板；“存到‘文件’”用系统保存对话框，不可用时同样复制路径。
- 输出文件在 `$XDG_DATA_HOME/com.stl2stp.stl2stp_app/Outputs/`（默认 `~/.local/share/...`）。本机跑测试时把 `XDG_DATA_HOME` 和 `TMPDIR` 指到 D 盘（`build/xdg-data`、`build/tmp`），避免往 WSL 的 C 盘虚拟磁盘写东西。
- 桌面版可以把 `.stl` 文件直接拖进窗口（`desktop_drop`），非 STL 会提示“只支持 .stl 文件”。
- 构建时限制并行：`CMAKE_BUILD_PARALLEL_LEVEL=4`。

### 3.3 Android（可选，后续）

Windows 上已有 Android SDK、NDK 28.2 和模拟器镜像，Flutter 也能编 Android。但 OCCT 需要用 NDK 交叉编译（arm64-v8a、x86_64 各一份），工作量和 iOS 差不多，还没做。不编核心库时 Android 版会以演示模式运行，可以先看界面：`flutter run -d emulator-5554`（Windows 端 `C:\dev\flutter`）。

## 4. 修改文案

- 设计文案的唯一来源是 `design/ui/tools/build_strings.py`。改完重新生成 `design/ui/strings_*.json`，再运行 `app/tool/sync_strings.sh`。
- App 专用文案（`x.*`、`help.*`、`purchase.*`、`est.*`）的唯一来源是 `app/tool/app_strings.py`，改完运行 `python3 tool/app_strings.py` 生成 `assets/i18n/app_{zh,en}.json`。运行时两份合并（同名 key 以设计文案为准，测试保证不重名）。
- `test/core_test.dart` 会检查：两套文案各自中英文 key 一致、占位符一致、App 文案不和设计 key 重名且不用感叹号、代码里用到的 key 都存在、assets 与 design 目录一致。

## 5. 和 ui-spec 的差异 / 待办

- 3D 预览用纯 Flutter 软件渲染（CustomPainter + `drawVertices`，最多显示 4 万个三角形），所有平台都能用；大模型以后可换 `flutter_scene` 或原生 SceneKit。
- 宽屏（≥900 宽）时预览页已改为左右两栏；完整的 iPad 分栏（§4.16）还没做。
- 历史记录左滑只有“删除”；点一行即“再次分享”。
- 还没做：后台任务、内存警告自动取消、触感反馈、iPad 拖放（需要原生 UIDropInteraction）。

## 6. 帮助页

`lib/screens/help_screen.dart`，入口：首页右上角“？”、预览页右上角“？”、预览页“导出格式”和“转换方式”标题旁的小“？”（直接跳到对应段落）、设置 → 关于 → 使用帮助。内容（中英文，`help.*`）：
支持哪些文件、STEP 和 IGES 的区别、三种转换方式、原版模式和缝合模式、合并共面、为什么要减少面数、尺寸和单位、预计用时和大小、文件信息、常见问题（6 条，第一条就是“为什么打开是一堆曲面”）、隐私（全部在本机处理，不上传）。隐私一句也放在页首的绿色提示条里。

## 7. 预计用时和大小（估算与校准）

**模型**（`lib/core/estimate.dart`）。n 是输入三角形数，f 是减面后的三角形数，F 是写出的 CAD 面数：
- F = f；打开“合并共面”时 F ≈ 网格里不同平面的个数。这个数在 Dart 端解析 STL 时统计（`PlaneCounter`：法向量化到约 0.06°，平面偏移量化到 0.001），减面后取 `min(f, 平面数 × √(f/n))`。
- 用时 = 0.15 s + 0.53 ms × F（合并共面时再加 0.37 ms × f），缝合模式 × 1.25；STEP 校验再加 0.25 s/MB × 输出大小。最后乘设备系数 `Estimator.deviceFactor`，默认 1.0，可配置。
- 大小 = 8 KB + 每面字节数 × F：STEP 每面 2400 B，IGES 每面 1870 B；缝合模式 × 1.05。合并后的多边形面每面约 × 1.3；合并后面数不到三角形 5% 时（平板、方块）每面 × 0.6。
- 显示为范围：
  - 用时下限 × 0.75、上限 × 1.3，再乘设备不确定度 × 0.75～1.5；减面时下限再 × 0.7。
  - 大小 × 0.9～1.1；合并共面时面数取 0.7～1.4 倍。
  - 平面数未知时（预览解析失败），面数取 2%～100%。
- 文案示例：“预计用时约 4–9 秒，文件约 2.2–3.0 MB”。

**标定数据**：`app/tool/calib/`。
- 条件：2026-10-21 在开发机上测量。环境是 WSL2 Ubuntu 20.04、gcc-10、OCCT 8.0.1，用 `stl2stp-cli convert`，输出写到 `/mnt/d`，一次只跑一个转换。
- 网格：
  - UV 球：曲面多，相邻两个三角形组成一个平面四边形。
  - 细分立方体：全是平面，合并后只剩 6 个面。
  - 规模 5k～100k 三角形。
- `convert` 本身已包含 STEP 读回校验（日志里的 `readback_ms`）。

| 网格 | 三角形 | STEP 原版 | STEP 合并共面 | STEP 缝合 | IGES |
|---|---|---|---|---|---|
| 球 5k | 4 992 | 6.0 s / 11.9 MB | 6.8 s / 7.7 MB | 7.0 s / 12.5 MB | 3.2 s / 10.1 MB |
| 球 20k | 20 224 | 21.9 s / 49.1 MB | 19.9 s / 31.8 MB | 22.9 s / 51.4 MB | 11.2 s / 41.1 MB |
| 方块 20k | 20 172 | 19.6 s / 44.6 MB | 7.2 s / 16 KB | 28.8 s / 47.4 MB | 10.9 s / 34.5 MB |
| 球 50k | 50 000 | 66.8 s / 124 MB | 63.6 s / 79.5 MB | 55.6 s / 130 MB | 25.2 s / 102 MB |
| 球 100k | 101 760 | 117 s / 255 MB | 108 s / 164 MB | — | 56.0 s / 207 MB |
| 方块 100k | 99 372 | 134 s / 227 MB | 31.1 s / 16 KB | 131 s / 241 MB | 50.2 s / 170 MB |

减面（STEP 原版）：
- 球 50k 减到 50%：19.7 s / 61.1 MB；再合并共面：25.5 s / 48.1 MB。
- 球 50k 减到 25%：9.4 s / 30.5 MB；再合并共面：15.1 s / 28.1 MB。
- 方块 20k 减到 50%：9.9 s / 22.2 MB；再合并共面：5.2 s / 16 KB。

结论：
- STEP 导出约 0.5 ms/面，读回校验约占一半时间；IGES 不校验，约为 STEP 的一半。
- 缝合只慢 0～60%，不是原先估计的 2 倍，帮助页已按实测写成“通常会慢一些”。
- 合并共面对平面零件效果极大（方块从 45 MB 变成 16 KB），对曲面模型只省约 35%。
- 每个三角形面约 2.4 KB（STEP），和原占位值吻合。但原占位的用时偏乐观约 4～5 倍：占位是 0.2 ms/面 × 校验 1.3，实测含校验约 1.0–1.35 ms/面。

`test/core_test.dart` 把上面每个测量点都放进测试：实测值必须落在预测范围内，中心值误差不超过 35%（用时）/ 20%（大小）。

发现的核心库问题（已记录，未改核心库）：球 50k 减面后导出成了**开放壳**，日志提示“mesh is not closed”，约有 200 条边界边；方块减面后仍是实体。所以减面后的用时下限放宽了。

**手机标定**（需要 Mac + iPhone）：在真机上转几组模型，用“实测 ÷ 本表”的比值设置 `Estimator(deviceFactor: …)`（在 `main.dart` 的 `createAppState` 里传给 `AppState`）。手机上输出写在本机闪存，比这里写 `/mnt/d` 快，所以系数可能小于 1。

## 8. 购买框架

- `lib/purchase/purchase_service.dart` 定义了以下接口：
  - `PurchaseService` 是抽象类，有 `isPro`、`available`、`product`、`init()`、`buyPro()`、`restore()`。
  - 只有一个商品：一次性买断的“完整版”（Non-Consumable），商品 ID 在 `PurchaseConfig.proProductId` 里，默认 `com.stl2stp.app.pro`，需要和 App Store Connect 一致。
  - `ProductInfo.priceLabel` 来自商店（StoreKit 的本地化价格）。代码里**没有任何价格**：商店还没返回价格时，按钮显示设计稿的占位“购买（价格待定）”，并带橙色框。
- `free_tier.dart` 定义 `FreeTierLimits`，可以设最大面数（按减面后的面数算）、每天次数、IGES 需完整版、自定义文件信息需完整版。
  - **默认 `FreeTierLimits.none`，不限制任何东西，行为和以前完全一样。**
  - 设了限制后，点“转换”前会检查，超限就弹购买页，并在页面上说明原因。买完回来继续转换。
  - 每天的次数存在 prefs 里。
- `mock_purchase_service.dart` 是 Linux、桌面和测试用的模拟商店：
  - 不扣费，购买页会显示“开发用模拟商店：不会扣费”。
  - 购买状态存在 prefs 里。
  - 可以模拟取消、失败、商店不可用。
  - 设置页有“重置购买状态（开发用）”。
- `store_purchase_service.dart` 是真实商店实现，用 `in_app_purchase`（iOS 上走 StoreKit）：
  - 监听 `purchaseStream`；收到 purchased/restored 时解锁，并调用 `completePurchase`。
  - 恢复购买 12 秒没有回应，就判定为“没有可恢复的购买”。
  - **未测试**（需要 Mac）。上架前要补交易校验，现在只信任本地标记。
- `purchase.dart` 里的 `createPurchaseService()`：iOS、macOS、Android 用真实商店，其他平台用模拟商店。`STL2STP_MOCK_STORE=1` 强制用模拟商店。
- 界面：
  - 购买页按 ui-spec §4.14 做：保留黄色占位横幅，4 项权益后面标“待定”，有“购买 / 恢复购买 / 以后再说”。
  - 设置页显示“免费版 / 完整版（已解锁）”，有恢复购买入口。
- Linux 构建：`in_app_purchase` 没有 Linux 实现，但依赖它不影响 Linux 编译（已验证）。Linux 上不会构造 StoreKit 实现。

## 9. 分享

- iOS、Android、macOS 用 `share_plus` 的系统分享面板。iPad 必须给弹出框一个锚点：完成页传按钮位置，其他地方默认用触发控件自己的位置。
- Linux、Windows 没有分享面板，会把文件路径复制到剪贴板并提示。
- “存到‘文件’”用 `file_picker` 的保存对话框；不可用时同样复制路径。
