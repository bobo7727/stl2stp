# Flutter App（`app/`）说明

> 分支 `app/flutter`。界面按 `design/ui/ui-spec.md` 实现，文案来自 `design/ui/strings_{zh,en}.json`（默认中文，可切换英文）。
> 转换引擎是本仓库的 `core/`（stl2stp-core），通过 `dart:ffi` 调用 C API `core/include/stl2stp/stl2stp.h`。

## 1. 目录结构

```
app/
├── pubspec.yaml                 依赖：ffi、path、path_provider、shared_preferences、file_picker、share_plus
├── assets/i18n/zh.json, en.json 文案（从 design/ui 复制，tool/sync_strings.sh 同步）
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
│   │   ├── stl_reader.dart      Dart 端轻量 STL 解析，只用于 3D 预览和 ASCII/二进制标签
│   │   ├── estimate.dart        预计用时/大小（占位系数，需真机标定）
│   │   ├── sample_mesh.dart     示例模型（L 形支架、球）生成器
│   │   └── screenshot.dart      集成测试里截图用
│   ├── l10n/strings.dart        S.of(context).t('key', {参数})
│   ├── theme/tokens.dart        ui-spec §6 的颜色、字号、圆角
│   ├── state/                   AppState（设置、历史、输出目录）、历史记录 JSON
│   ├── screens/                 引导、首页、预览+设置、转换中、完成、错误、历史、设置、开源许可/购买页占位
│   └── widgets/                 3D 预览（纯 Flutter 软件渲染）、提醒条、统计格、进度环等
├── test/                        单元测试 + 组件测试（用演示引擎，任何机器都能跑）
├── integration_test/app_test.dart  端到端：真机/桌面上用真实核心库走完整 UI 流程并截图
├── tool/sync_strings.sh         同步文案
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
- 输出文件在 `~/.local/share/com.stl2stp.stl2stp_app/Outputs/`（Linux）。

### 3.3 Android（可选，后续）

Windows 上已有 Android SDK、NDK 28.2 和模拟器镜像，Flutter 也能编 Android。但 OCCT 需要用 NDK 交叉编译（arm64-v8a、x86_64 各一份），工作量和 iOS 差不多，还没做。不编核心库时 Android 版会以演示模式运行，可以先看界面：`flutter run -d emulator-5554`（Windows 端 `C:\dev\flutter`）。

## 4. 修改文案

文案的唯一来源是 `design/ui/tools/build_strings.py`。改完重新生成 `design/ui/strings_*.json`，再运行 `app/tool/sync_strings.sh`。`test/core_test.dart` 会检查：中英文 key 一致、占位符一致、代码里用到的 key 都存在、assets 与 design 目录一致。App 专用的少量文案在 `lib/l10n/strings.dart` 的 `_extra` 里（`x.` 开头），以后可以合并进 build_strings.py。

## 5. 和 ui-spec 的差异 / 待办

- 3D 预览用纯 Flutter 软件渲染（CustomPainter + `drawVertices`，最多显示 4 万个三角形），所有平台都能用；大模型以后可换 `flutter_scene` 或原生 SceneKit。
- iPad 分栏、拖放导入、后台任务、内存警告自动取消、触感反馈、购买（in_app_purchase）还没做；宽屏（≥900 宽）时预览页已改为左右两栏。
- 历史记录左滑只有“删除”；点一行即“再次分享”。
- 预计用时/大小沿用 ui-spec 的占位系数，需真机标定。
