# Flutter App（`app/`）说明

> 分支 `app/flutter`。界面按 `design/ui/ui-spec.md` 实现，文案来自 `design/ui/strings_{zh,en}.json`（默认中文，可切换英文）。
> 转换引擎是本仓库的 `core/`（stl2stp-core），通过 `dart:ffi` 调用 C API `core/include/stl2stp/stl2stp.h`。

## 1. 目录结构

```
app/
├── pubspec.yaml                 依赖：ffi、path、path_provider、shared_preferences、file_picker、share_plus、in_app_purchase、desktop_drop
├── assets/i18n/zh.json, en.json 设计文案（从 design/ui 复制，tool/sync_strings.sh 同步）
├── assets/i18n/app_zh.json, app_en.json  App 专用文案（帮助页、购买、估算等；tool/app_strings.py 生成）
├── assets/licenses/occt/        OCCT 8.0.1 的 LICENSE_LGPL_21.txt 和 OCCT_LGPL_EXCEPTION.txt（原样取自 OCCT 安装目录 share/doc/opencascade/）
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
│   │   ├── sample_mesh.dart     示例模型（L 形支架、球；可生成“极点不精确”的开放球）生成器
│   │   ├── memory_guard.dart    转换前的峰值内存预估和本机内存预算（见 §13）
│   │   ├── licenses.dart        OCCT 许可文本：注册进 LicenseRegistry、读取全文（见 §12）
│   │   └── screenshot.dart      集成测试里截图用
│   ├── l10n/strings.dart        S.of(context).t('key', {参数})
│   ├── theme/tokens.dart        ui-spec §6 的颜色、字号、圆角
│   ├── platform/                background_task.dart（iOS 后台任务桥）、haptics.dart（触感反馈）；其他平台为空操作
│   ├── purchase/                购买框架：PurchaseService 接口、免费额度、模拟商店、StoreKit 实现（见 §8）
│   ├── state/                   AppState（设置、历史、输出目录、购买/额度、估算器）、历史记录 JSON
│   ├── screens/                 引导、首页、预览+设置、转换中、完成、错误、历史、设置、帮助、购买页、开源许可
│   └── widgets/                 history_list.dart（可左右滑的历史行）、3D 预览（纯 Flutter 软件渲染）、桌面拖放、提醒条、统计格、进度环等
├── test/                        单元测试 + 组件测试（用演示引擎，任何机器都能跑）
├── integration_test/app_test.dart  端到端：真机/桌面上用真实核心库走完整 UI 流程并截图
├── tool/sync_strings.sh         同步设计文案
├── tool/app_strings.py          App 专用文案的唯一来源（生成 assets/i18n/app_*.json）
├── tool/calib/                  估算标定脚本和实测数据（calib*.py、calib*.csv；weldtime.*：焊接耗时；memtime.*：峰值内存）
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
- iPad / 宽窗口分栏（§4.16）、历史滑动、后台任务、内存保护、触感反馈已做（§11、§13）；iOS 原生部分（后台任务、可用内存）只写了文档和 Dart 端，**需要在 Mac 上接上并验证**（docs/mac-setup-guide.md §9.3）。
- 预览页本身在宽度 ≥ 760 时用左右两栏（分栏右侧窗格约 940 宽时也会用到）。
- 还没做：iPad 拖放（需要原生 UIDropInteraction）。

## 6. 帮助页

`lib/screens/help_screen.dart`，入口：首页右上角“？”、预览页右上角“？”、预览页“导出格式”和“转换方式”标题旁的小“？”（直接跳到对应段落）、设置 → 关于 → 使用帮助。内容（中英文，`help.*`）：
支持哪些文件、STEP 和 IGES 的区别、三种转换方式、原版模式和缝合模式、合并共面、为什么要减少面数、尺寸和单位、预计用时和大小、文件信息、常见问题（6 条，第一条就是“为什么打开是一堆曲面”）、隐私（全部在本机处理，不上传）。隐私一句也放在页首的绿色提示条里。

## 7. 预计用时和大小（估算与校准）

**模型**（`lib/core/estimate.dart`）。n 是输入三角形数，f 是减面后的三角形数，F 是写出的 CAD 面数：
- F = f；打开“合并共面”时 F ≈ 网格里不同平面的个数。这个数在 Dart 端解析 STL 时统计（`PlaneCounter`：法向量化到约 0.06°，平面偏移量化到 0.001），减面后取 `min(f, 平面数 × √(f/n))`。
- 用时 = 0.07 s + 0.33 ms × F（合并共面时再加 0.34 ms × f），缝合模式 × 1.25；STEP 校验再加 0.18 s/MB × 输出大小。最后乘设备系数 `Estimator.deviceFactor`，默认 1.0，可配置。
- 大小 = 8 KB + 每面字节数 × F：STEP 每面 2400 B，IGES 每面 1870 B；缝合模式 × 1.05。合并后的多边形面每面约 × 1.3；合并后面数不到三角形 5% 时（平板、方块）每面 × 0.6。
- 显示为范围：
  - 用时下限 × 0.75、上限 × 1.35，再乘设备不确定度 × 0.75～1.5。
  - 大小 × 0.9～1.1；合并共面时面数取 0.7～1.4 倍。
  - 平面数未知时（预览解析失败），面数取 2%～100%。
- 文案示例：“预计用时约 4–9 秒，文件约 2.2–3.0 MB”。

**标定数据**：`app/tool/calib/`。
- 条件：2026-10-21 在开发机上测量（下表是机器空闲时的第二轮，所有网格都是闭合的，STEP 读回都是 1 个实体）。环境是 WSL2 Ubuntu 20.04、gcc-10、OCCT 8.0.1，用 `stl2stp-cli convert`，输出写到 `/mnt/d`，一次只跑一个转换。
- 网格：
  - UV 球：曲面多，相邻两个三角形组成一个平面四边形。
  - 细分立方体：全是平面，合并后只剩 6 个面。
  - 规模 5k～100k 三角形。
- `convert` 本身已包含 STEP 读回校验（日志里的 `readback_ms`）。

| 网格 | 三角形 | STEP 原版 | STEP 合并共面 | STEP 缝合 | IGES |
|---|---|---|---|---|---|
| 球 5k | 4 992 | 3.3 s / 11.9 MB | 4.1 s / 7.7 MB | 3.9 s / 12.5 MB | 1.7 s / 10.1 MB |
| 球 20k | 20 224 | 14.1 s / 49.1 MB | 16.9 s / 31.7 MB | 17.0 s / 51.4 MB | 6.5 s / 41.1 MB |
| 方块 20k | 20 172 | 13.8 s / 44.6 MB | 5.4 s / 16 KB | 17.0 s / 47.4 MB | 6.1 s / 34.5 MB |
| 球 50k | 50 000 | 37.1 s / 124 MB | 44.3 s / 79.4 MB | 46.3 s / 130 MB | 18.1 s / 102 MB |
| 球 100k | 101 760 | 82.0 s / 255 MB | 95.4 s / 164 MB | — | 34.7 s / 207 MB |
| 方块 100k | 99 372 | 79.1 s / 227 MB | 28.1 s / 16 KB | 99.3 s / 241 MB | 33.5 s / 170 MB |

减面（STEP 原版，结果都是 1 个闭合实体）：
- 球 50k 减到 50%：18.4 s / 61.0 MB；再合并共面：25.1 s / 48.1 MB。
- 球 50k 减到 25%：8.6 s / 30.4 MB；再合并共面：13.0 s / 27.5 MB。
- 方块 20k 减到 50%：8.7 s / 22.2 MB；再合并共面：4.5 s / 16 KB。

第一轮（同一天早些时候，机器上同时有别的构建在跑）同样的转换慢 1.1～1.9 倍（不合并时差得多，主要是写盘和读回）。那一轮的球还是开放网格（见下文）。第一轮数据保留在 `test/core_test.dart` 的 `busyRows` 里，测试要求它们落在“含设备不确定度”的范围里（上限 × 1.35 × 1.5）。

结论：
- STEP 导出约 0.33 ms/面，读回校验约 0.18 s/MB（约占一半时间）；IGES 不校验，约为 STEP 的一半。
- 缝合比原版慢约 20～25%（第一轮 −15%～+50%），不是原先估计的 2 倍，帮助页已按实测写成“通常会慢一些”。
- 合并共面对平面零件效果极大（方块从 45 MB 变成 16 KB），对曲面模型只省约 35%。
- 每个三角形面约 2.4 KB（STEP），和原占位值吻合。但原占位的用时偏乐观：占位是 0.2 ms/面 × 校验 1.3，实测含校验约 0.65–0.8 ms/面（机器忙时到 1.35 ms/面）。
- 减面后的模型按正常的每面速度转换，不再需要单独放宽下限。

`test/core_test.dart` 把上面每个测量点都放进测试：实测值必须落在预测范围内，中心值误差不超过 35%（用时）/ 20%（大小）。

关于“球减面后变成开放壳”（2026-10-21 已查清，不是核心库的问题）：问题出在测试网格的生成器上。生成器用 sin/cos 算两极，而 sin(π) 在浮点里不是 0（约 1e-16），所以南极成了 64 个相距约 1e-15 的不同点。网格本身就有一个极小的洞（128 条边界边），减面前就不闭合，第一轮的球在不减面时也没有生成实体。核心库按位精确焊接顶点（与桌面版一致），所以如实提示“mesh is not closed”。方块的坐标是精确值，因此没受影响。两极改成精确单点后，球在原始、50%、25% 下都是闭合网格（边界边 0，欧拉数 2），STEP 读回都是 1 个实体。核心库新增回归测试 `qem_closed_sphere` / `qem_near_coincident_pole`。生成器（`sample_mesh.dart`、`calib.py`）已修正。用户自己的 STL 如果有这类微小缝隙，用“更严格”（缝合）模式可以缝成实体。

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

## 10. 合并重合顶点（焊接）

- 核心库（`core/src/weld.cpp`，见 `docs/core-api.md` “core/weld”）在 ORIGINAL 模式里把**开放边界上**、距离小于容差的顶点合并：
  容差 = `max(1e-6 × 包围盒对角线, 2 × FLT_EPSILON × 最大坐标)`，网格哈希 O(n)；不会合并封闭区域的顶点（薄壁不受影响），也不会让三角形退化。
  典型效果：很多导出器写出的球体极点差几个浮点单位，精确合并后留下微小缝隙，原先只能得到开放曲面；焊接后得到 1 个封闭实体（核心测试 `weld_open_pole`）。
- App：预览页“高级设置”里的“合并重合顶点”开关（`weldSwitch`，默认开，属于预设的一部分：关掉后预设显示为“自定义”）；帮助页有 `weld` 段落（`help.weld.*`）。
  - 开：导出选项 `weld_vertices=1`；减面时先调 `stl2stp_weld_vertices` 再 QEM（QEM 需要连通的网格才能保持封闭），有顶点被合并时完成页显示“已修复”。
  - 关：`weld_vertices=0`，保持逐位精确合并的旧行为。
- 实测耗时影响（Release，WSL，STEP ORIGINAL，单次，秒，开 / 关；`tool/calib/weldtime.csv`）：

  | 模型 | 开 | 关 |
  | --- | --- | --- |
  | 球 5k | 3.32 | 3.44 |
  | 球 20k | 14.02 | 13.82 |
  | 方块 20k | 14.20 | 13.93 |
  | 球 50k | 38.68 | 39.82 |
  | 球 50k 减到 50% | 18.76（焊接本身 4.6 ms） | 19.36 |
  | 开放极点球 50k | 36.92（合并 199 个顶点，1 个实体） | 36.68（0 个实体） |
  | 球 100k | 86.96 | 82.02 |
  | 方块 100k | 79.73 | 78.42 |

  差别在单次运行的波动范围内，§7 的估算系数没有改。
- 集成测试用真实核心库验证：开放极点球（3968 面）开焊接 → STEP 里 1 个 `MANIFOLD_SOLID_BREP`；关焊接 → 0 个，只有 `SHELL_BASED_SURFACE_MODEL`。

## 11. iPad 分栏和历史滑动

- `useSplitLayout(size)`：宽度 ≥ 900，或者最短边 ≥ 600 且宽度 ≥ 700（iPad 竖屏 820×1180 也算）时用分栏；iPad 侧拉（Slide Over，约 375 宽）和手机布局不变。
- 分栏：左边固定 340 宽，是首页内容（导入卡片、示例、帮助/设置按钮）加完整历史；右边是一个嵌套 `Navigator`（`detailNavigatorKey`），预览 → 转换中 → 完成/错误都在右栏里走，右栏按自己的宽度布局（单独的 `MediaQuery`），并有自己的 `ScaffoldMessenger`（提示条不会在两栏重复出现）。没选模型时右栏显示占位提示（`x.split.placeholder`）。`openModel` 有右栏时在右栏里打开并替换掉之前的页面。
- 历史行（`lib/widgets/history_list.dart`，首页“最近”和历史页共用）：
  - 向右滑：再次分享（行不消失）；向左滑：删除记录和文件；点一行也是再次分享。
  - 输出文件已不存在：不分享，提示“……已不存在，无法分享”，带“删除记录”按钮（只删记录）。
- 组件测试：`test/features_test.dart` 的 `split layout`（宽窗口两栏、预览在右栏；手机单栏、预览是新页面）和 `history swipe`。

## 12. 开源许可

- 设置 → 关于 → 开源许可：OCCT（LGPL 2.1 + OCCT 例外）、stl2stp-core 说明；“查看 OCCT 许可证全文”显示 App 内置的两个文本（`assets/licenses/occt/`，可选择复制）；“全部开源组件许可”打开 Flutter 的 `showLicensePage`，里面有 Flutter/Dart 各个包的许可（Flutter 自动收集），以及启动时用 `LicenseRegistry.addLicense` 注册的 OCCT 两段文本（包名 `Open CASCADE Technology (OCCT) 8.0.1`）。
- **iOS 静态链接 LGPL 库的要求——这是一个待确认的法律问题，以下不是法律意见**：
  - LGPL 2.1 第 6 条要求用户能用修改过的库重新链接得到可运行的程序。iOS 上 OCCT 是静态链接进 App 的，通常讨论的做法有：
    1. 另外提供 App 的目标文件/静态库（不含 OCCT）和链接说明，让用户能自己重链；或
    2. 把 OCCT 做成动态 framework 随 App 分发。
  - OCCT 例外条款只允许“含有 OCCT 头文件内容（内联函数、模板等）的目标代码”按自选条款分发，条件是在配套文档里显著注明用了 OCCT；它**不**免除对 OCCT 库本身的重链要求。
  - App Store 的分发条款（设备限制、DRM）与 LGPL 的兼容性一直有争议；原 stl2stp 项目本身的许可也要确认。
  - 结论交给法务；技术上两种做法都可行，选 2 需要改 `tool/ios/build_ios_core.sh` 产出 dylib/framework。

## 13. 后台转换、内存保护、触感反馈

- **后台**（`lib/platform/background_task.dart`）：转换开始时 `begin`，结束/离开页面时 `end`；系统通知后台时间快用完（`expired`）时自动安全取消，错误页说明“App 在后台停留太久……转换已停止”。转换页提示“可以切到其他 App，转换会在后台继续一小段时间”。只在 iOS 启用；Linux/桌面/测试里是空操作（桌面进程本来就会继续跑）。iOS 原生代码见 docs/mac-setup-guide.md §9.3，**未在真机验证**。
- **内存预检**（`lib/core/memory_guard.dart`、`lib/screens/memory_dialog.dart`）：
  - 预估峰值 = 50 MB + 20.5 kB × 生成的 CAD 面数（缝合 × 1.06；合并共面 × 1.0；减面时取它和 1.2 kB × 原始三角形数中较大的一项）。
  - 系数来自 `stl2stp-cli` 的 max RSS（`tool/calib/memtime.csv`，WSL，Release）：ORIGINAL STEP 5k 面 140 MB、20k 437 MB、50k 1016 MB、102k 2017 MB；缝合约多 5%；合并共面和 IGES 实测更少（保守地按 ×1.0）；减面 50% 基本按输出面数。模型在这些点上都不低于实测值（测试里有断言）。手机上的实际占用**还没测**（需要 Mac + 真机）。
  - 本机预算：设置项 `memoryBudgetMB`（覆盖）→ iOS `stl2stp/memory` 通道 `available`（`os_proc_available_memory()`）× 0.8 → Linux `/proc/meminfo` MemAvailable × 0.8 → 默认 1.5 GB。
  - 超过预算 70%：提示“可能占用很多内存”，可“仍然转换”或“减到 N% 再转换”（N 为能落到 70% 以下的最大 5% 档）；超过预算：提示“内存不够”，只能减面或取消。
- **内存紧张**：系统内存警告（iOS `didReceiveMemoryWarning`，Flutter 转成 `WidgetsBindingObserver.didHaveMemoryPressure`）时，正在进行的转换安全取消，不留半个文件，错误页说明原因并建议减面。
- **触感**（`lib/platform/haptics.dart`）：开始、成功用轻触，失败用中等力度；只在 iOS/Android 启用，桌面为空操作。
- 测试：`test/features_test.dart` 的 `memory guard`（模型、预算解析、警告对话框选“减面”后按 75% 转换、拒绝）、`running conversion`（内存紧张自动取消、后台任务 begin/end/到期取消、触感调用、桌面空操作）。

