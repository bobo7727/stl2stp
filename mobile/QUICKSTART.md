# 从全新克隆复现移动版

仓库默认分支是 master。Windows/MFC 原工程在仓库根目录，本文所有构建命令都在 **mobile/** 执行。无需原开发机的目录、旧 build 缓存、reference 副本或 app/flutter 分支。

## 1. 取得源码和工具

```bash
git clone https://github.com/bobo7727/stl2stp.git
cd stl2stp/mobile
```

核心库需要 Bash、CMake、Ninja、支持 C++17 的 C/C++ 编译器、Python 3、curl、tar 和 SHA256 校验工具。Ubuntu 24.04 可用系统工具链；复现原 Ubuntu 20.04 基线时可显式设置 `CC=gcc-10 CXX=g++-10`。工具不会安装系统包或修改 shell 配置。Ubuntu 系统依赖可按需安装：

```bash
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build curl python3
```

Flutter App 另需 **Flutter 3.38.7 / Dart 3.10.7**，将自己的 SDK 加入 PATH，执行 `flutter --version` 和 `flutter doctor -v` 核对。Linux 桌面还需 clang、pkg-config、libgtk-3-dev、liblzma-dev、unzip、xz-utils、libglu1-mesa；WSL 显示窗口需要 WSLg。安装细节和历史版本见 [environment.md](docs/handoff/environment.md)。

## 2. 没有 OCCT 时先检查网格核心

```bash
bash tools/verify.sh --mesh-only --jobs 4
```

这会从源码构建并运行网格测试，不下载 OCCT。**该模式没有 STEP/IGES 导出能力，不能当作完整转换验证通过。** 默认产物在 build/mesh。

## 3. 完整核心、CLI 与 CAD 检查

```bash
# 首次下载固定的 OCCT 8.0.1 源码、校验 SHA256、构建和安装。
bash tools/bootstrap-occt.sh --jobs 4
# 构建共享核心、CLI、测试，生成模型并执行真实导出和独立 CAD 检查。
bash tools/verify.sh --jobs 4 --cad
```

OCCT 默认写入 build/occt 下的下载、源码、编译和安装目录；验证构建默认位于 build/core-shared。所有内容均可重新生成，不提交 Git。首次 OCCT 构建可能需要较长时间和数 GB 空间。脚本校验固定摘要，不使用原 Windows 工程的 OCCT 二进制。

已有安装或需要自定路径时：

```bash
bash tools/bootstrap-occt.sh --work-dir "$PWD/build/occt" --prefix "$PWD/build/occt/install" --jobs 4
bash tools/verify.sh --occt-root "$PWD/build/occt/install" --build-dir "$PWD/build/core-shared" --jobs 4 --cad
```

CAD 入口也能单独执行：

```bash
bash core/tools/cadcheck/run.sh --build-dir "$PWD/build/core-shared" --check
```

它会准备基础与扩展模型，为每次执行创建新的输出子目录，并打印实际结果位置。失败应返回非零退出码。FreeCAD/SolidWorks 是额外的外部验证层，不由此命令安装或代跑；可选渲染需要 matplotlib。详见 [validation.md](docs/handoff/validation.md)。

## 4. Flutter Linux App

先完成上面的完整共享核心构建，并把自己的 Flutter SDK 加入 PATH：

```bash
R="$PWD"
cd app
flutter pub get
flutter analyze
flutter test
STL2STP_CORE_LIB="$R/build/core-shared/libstl2stp-core.so" flutter run -d linux
# 有图形会话时执行真实核心端到端验证：
STL2STP_CORE_LIB="$R/build/core-shared/libstl2stp-core.so" \
  flutter test integration_test/app_test.dart -d linux \
  --dart-define=SHOT_DIR="$R/build/app-shots"
```

pubspec.lock 已入库。测试和 App 运行会下载/生成 pub 缓存及 Flutter 文件，不依赖作者机器的缓存。App 若提示演示模式，应先排查共享核心加载；看到界面不代表已能真实转换。Linux bundle 目前仍依赖本机安装的 OCCT，直接复制到另一台机器不是本项目的发布流程。

## 5. Mac / iOS

需要 Mac、Xcode、CMake、Ninja、Python 3、Flutter 3.38.7 和 CocoaPods。当前项目最低 **iOS 15.0**。锁定的 file_picker_darwin 插件要求 iOS 14 或更高，低于本工程下限。进入同一克隆的 mobile/：

```bash
mkdir -p build
# Apple 芯片 Mac 可只构建 arm64 模拟器；Intel Mac 保留脚本默认模拟器架构。
SIM_ARCHS=arm64 JOBS=4 bash app/tool/ios/build_ios_core.sh
cd app
flutter pub get
cd ios && pod install && cd ..
flutter devices
flutter run -d <实际模拟器或设备ID>
```

真机签名使用自己的 Apple 开发团队。静态库、XCFramework、生成的 xcconfig、Pods 和签名私密文件不入库。后台任务及实时内存通道已纳入 AppDelegate，无需粘贴文档代码。静态链接的 C API 保留清单与头文件/Dart 绑定会在原生构建前校验。

**Mac 构建和真机功能必须在 Mac 上另行验收，Linux 静态检查不能代替它们。** 验收至少包括：Release 包加载真实 FFI 核心、焊接开关、STL 导入→STEP 导出→回读、分享、后台过期取消和内存压力取消。完整步骤见 [mac-setup-guide.md](docs/mac-setup-guide.md)。

## 6. 提交与复现证据

从 master 建功能分支，提交源码、配置和文档；构建目录不入库。记录 Git 提交号、OS、编译器/CMake/Flutter/OCCT 版本、执行命令及测试报告。历史文档中的 122 项 CTest、55 项 Flutter 测试等数字是参考基线，以本次工具输出为准。

复现以功能和测试结果为验收依据；不同工具链、安装路径和 RPATH 可能改变二进制摘要。固定 STL 与时间戳可用于进一步比对导出内容。

云端 Mac 构建和 TestFlight 发布见 [Codemagic 指南](docs/codemagic.md)。
