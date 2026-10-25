# 在 Mac 上继续开发 iOS 版：详细操作流程

> 适用：`mobile/` 目录（原独立仓库的 `app/flutter` 分支；Flutter 界面 + `core/` C++ 核心库）。本文在 Windows/WSL 上写成，**还没有在 Mac 上实际跑过**；命令按 Xcode 16/26、Flutter 3.38、CMake 3.26+ 编写，遇到出入请对照第 12 节排错。
> 当前从 GitHub master 克隆，在 mobile/ 内执行核心库与 Flutter 命令。最低 iOS 14；先读 [QUICKSTART.md](../QUICKSTART.md)。iOS 原生通道已纳入工程，仍需 Mac/真机验证。

## 0. 总览

| 步骤 | 内容 | 预计耗时 |
|---|---|---|
| 1 | Mac 硬件和系统要求 | — |
| 2 | 安装 Xcode、命令行工具、Homebrew、CMake/Ninja、CocoaPods、Flutter | 1～2 小时（Xcode 下载最慢） |
| 3 | 从 GitHub 克隆仓库并进入 mobile/ | 10 分钟 |
| 4 | 先在 Mac 上验证核心库（可选，macOS 原生编译） | 30 分钟 |
| 5 | 编译 iOS 版 OCCT + 核心库（一条脚本） | 40～90 分钟 |
| 6 | 在 iOS 模拟器上运行 | 15 分钟 |
| 7 | 用免费 Apple ID 装到自己的 iPhone | 15 分钟 |
| 8 | App 图标、启动页、名称 | 30 分钟 |
| 9 | 付费开发者账号、TestFlight、App Store | 按审核 |
| 10 | LGPL（OCCT 静态链接）合规 | 上架前确认 |
| 11 | 把改动带回 Windows | 5 分钟 |
| 12 | 常见问题 | — |

## 1. Mac 要求

- **Apple 芯片（M1 及以后）优先**；Intel Mac 也行，但编 OCCT 慢一倍，模拟器要 x86_64。
- macOS 版本要能装当前 Xcode（Xcode 16 需要 macOS 14.5+；更新的 Xcode 要求更高，以 App Store 显示为准）。
- 磁盘至少留 **60 GB**：Xcode 约 15～20 GB，iOS 模拟器运行时 8 GB，Flutter 约 3 GB，OCCT 源码 + 两套 iOS 构建约 6～10 GB。
- 内存 16 GB 更舒服（8 GB 可用，编 OCCT 时把 `JOBS` 调小）。

## 2. 安装开发工具

### 2.1 Xcode
1. App Store 搜 “Xcode”，安装（或从 https://developer.apple.com/download/applications/ 下载 xip）。
2. 首次打开 Xcode，同意许可，勾选安装 **iOS** 平台（Settings → Components → iOS 模拟器运行时）。
3. 终端执行：
   ```bash
   sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
   sudo xcodebuild -license accept
   xcodebuild -runFirstLaunch
   xcodebuild -version          # 记下版本
   xcrun simctl list runtimes   # 应看到 iOS xx.x
   ```

### 2.2 命令行工具（CLT）
装了完整 Xcode 一般已带。没有时：`xcode-select --install`。

### 2.3 Homebrew、CMake、Ninja、CocoaPods
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
# 按提示把 brew 加进 PATH（Apple 芯片：eval "$(/opt/homebrew/bin/brew shellenv)"）
brew install cmake ninja cocoapods
cmake --version   # ≥ 3.16，建议 3.26+
ninja --version
pod --version
```
> 国内网络慢时可以给 Homebrew 换清华/中科大镜像（https://mirrors.tuna.tsinghua.edu.cn/help/homebrew/）。

### 2.4 Flutter
与 Windows/WSL 保持同一版本 **3.38.7**，避免 `pubspec.lock` 不一致：
```bash
mkdir -p ~/dev && cd ~/dev
# Apple 芯片：
curl -LO https://storage.googleapis.com/flutter_infra_release/releases/stable/macos/flutter_macos_arm64_3.38.7-stable.zip
unzip -q flutter_macos_arm64_3.38.7-stable.zip
# Intel Mac 用 flutter_macos_3.38.7-stable.zip
echo 'export PATH="$HOME/dev/flutter/bin:$PATH"' >> ~/.zshrc && source ~/.zshrc
flutter --disable-analytics
flutter precache --ios
flutter doctor -v     # Xcode、CocoaPods 两项应为 ✓（Android、Chrome 可以忽略）
```
> 国内可设镜像：`export PUB_HOSTED_URL=https://pub.flutter-io.cn FLUTTER_STORAGE_BASE_URL=https://storage.flutter-io.cn`。

## 3. 在 Mac 上取得仓库

```bash
mkdir -p ~/work && cd ~/work
git clone https://github.com/bobo7727/stl2stp.git
cd stl2stp/mobile
git log --oneline -5
```

后续 core/、app/、build/ 相对路径均以 mobile/ 为根。不需要原 ZIP 的 app/flutter 分支或旧 worktree。iOS 脚本会下载并校验固定的 OCCT 8.0.1 源码；已有官方源码包时可用 OCCT_TGZ 指定其路径。

## 4.（可选）先在 macOS 上编一遍核心库，跑单元测试

目的是确认 C++ 代码在 Apple Clang 下没问题，再上 iOS。
```bash
# 可先运行 iOS 构建脚本下载源码，或下载官方 V8.0.1.tar.gz 并校验摘要。
cd ~/stl2stp-ios-deps && tar xzf V8.0.1.tar.gz
cmake -G Ninja -S OCCT-8.0.1 -B build-occt-macos \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/stl2stp-ios-deps/occt-macos \
  -DINSTALL_DIR=$HOME/stl2stp-ios-deps/occt-macos -DBUILD_LIBRARY_TYPE=Shared \
  -DBUILD_MODULE_FoundationClasses=ON -DBUILD_MODULE_ModelingData=ON \
  -DBUILD_MODULE_ModelingAlgorithms=ON -DBUILD_MODULE_DataExchange=ON \
  -DBUILD_MODULE_ApplicationFramework=OFF -DBUILD_MODULE_Visualization=OFF \
  -DBUILD_MODULE_Draw=OFF -DBUILD_MODULE_DETools=OFF -DBUILD_DOC_Overview=OFF -DBUILD_USE_PCH=OFF \
  -DUSE_FREETYPE=OFF -DUSE_TK=OFF -DUSE_TCL=OFF -DUSE_OPENGL=OFF -DUSE_GLES2=OFF -DUSE_XLIB=OFF \
  -DUSE_FREEIMAGE=OFF -DUSE_RAPIDJSON=OFF -DUSE_DRACO=OFF -DUSE_TBB=OFF -DUSE_VTK=OFF
cmake --build build-occt-macos --parallel && cmake --install build-occt-macos

cd ~/work/stl2stp/mobile
cmake -G Ninja -S core -B build/core-macos -DCMAKE_BUILD_TYPE=Release \
  -DOCCT_ROOT=$HOME/stl2stp-ios-deps/occt-macos -DBUILD_SHARED_LIBS=ON
cmake --build build/core-macos --parallel
ctest --test-dir build/core-macos --output-on-failure     # 应全部通过（Linux 上是 118/118）
```
说明：`core/CMakeLists.txt` 在 Linux 才加 `--disable-new-dtags`；macOS 上 RPATH 由 CMake 处理。若 ctest 报找不到 `libTK*.dylib`，执行 `export DYLD_LIBRARY_PATH=$HOME/stl2stp-ios-deps/occt-macos/lib` 后再跑。

**顺便跑 macOS 桌面版看界面（真实转换）**：
```bash
cd app
flutter config --enable-macos-desktop
STL2STP_CORE_LIB=$PWD/../build/core-macos/libstl2stp-core.dylib flutter run -d macos
```
如果 App 提示“演示模式”，说明沙盒/库校验挡住了外部 dylib：调试期可在 `macos/Runner/DebugProfile.entitlements` 临时加 `com.apple.security.cs.disable-library-validation = true`，或把 dylib 拷进 `.app/Contents/Frameworks/`。macOS 版不是目标，只用于快速看效果。

## 5. 编译 iOS 版 OCCT + 核心库

仓库里已备好脚本 `app/tool/ios/build_ios_core.sh`，它会：
1. 校验并解压 OCCT 8.0.1；
2. 用 CMake（`CMAKE_SYSTEM_NAME=iOS`）编 **静态** OCCT 两份：真机 `iphoneos/arm64`、模拟器 `iphonesimulator/arm64;x86_64`；只开 FoundationClasses、ModelingData、ModelingAlgorithms、DataExchange 四个模块（其依赖的少量可视化/OCAF 工具包会自动带上，与 Linux 构建一致），`-Os`；
3. 编 `core/` 为静态库，并用 `libtool -static` 把核心库和全部 `libTK*.a` 合并成一个 `libStl2StpCore.a`（每个平台一份）；
4. 生成 `app/ios/Stl2StpCore/Stl2StpCore.xcframework`（备用，SwiftUI 也能直接用）；
5. 生成 `app/ios/Flutter/Stl2StpCore.xcconfig`，内容是按 SDK 区分的库搜索路径、`-lStl2StpCore -lc++`、完整 C API 的 `-Wl,-u,_stl2stp_*`（强制把 C API 链进 Runner），以及 `STRIP_STYLE = non-global`（保留全局符号，Dart 用 `DynamicLibrary.process()` 才找得到）。`ios/Flutter/Debug.xcconfig`、`Release.xcconfig` 已经写好 `#include? "Stl2StpCore.xcconfig"`，**不需要在 Xcode 里拖文件、改工程**。

执行：
```bash
cd ~/work/stl2stp/mobile
# Apple 芯片且不需要 Intel 模拟器时可只编 arm64，快一半：
mkdir -p build
set -o pipefail
SIM_ARCHS=arm64 JOBS=4 bash app/tool/ios/build_ios_core.sh 2>&1 | tee build/ios-core.log
ls -lh app/ios/Stl2StpCore/*/libStl2StpCore.a
cat app/ios/Flutter/Stl2StpCore.xcconfig
```
脚本里的关键 CMake 参数（便于手工排错）：
```
-G Ninja -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos|iphonesimulator
-DCMAKE_OSX_ARCHITECTURES=arm64 | "arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_BUILD_TYPE=Release
-DBUILD_LIBRARY_TYPE=Static -DBUILD_USE_PCH=OFF  (+ 与 Linux 相同的模块/USE_* 开关)
核心库：-DOCCT_ROOT=<上一步安装目录> -DBUILD_SHARED_LIBS=OFF -DSTL2STP_BUILD_CLI=OFF -DBUILD_TESTING=OFF
```
最低系统版本为 **iOS 14.0**，与锁定的 file_picker_darwin 插件要求一致。修改最低版本时应同步 Runner、Podfile、AppFrameworkInfo 和构建脚本默认值。

## 6. 在 iOS 模拟器上运行

```bash
cd ~/work/stl2stp/mobile/app
flutter pub get
cd ios && pod install && cd ..        # 首次需要；插件 path_provider/shared_preferences/file_picker/share_plus/in_app_purchase_storekit（desktop_drop 没有 iOS 实现，会被忽略）
open -a Simulator                     # 或 xcrun simctl boot "iPhone 16"
flutter devices
flutter run -d "iPhone 16"            # 名称以 flutter devices 显示为准
```
检查点：
- 首页**没有**黄色“演示模式”提示 → 核心库已链接成功；设置 → 关于 → “转换引擎”显示 `stl2stp-core (FFI)`。
- 往模拟器里放 STL：把文件拖进模拟器窗口会存到“文件”App；或 `xcrun simctl addmedia booted model.stl`。然后在 App 里“导入 STL 文件”。
- 跑测试：`flutter test`（不需要设备）；端到端：`flutter test integration_test/app_test.dart -d "iPhone 16" --dart-define=SHOT_DIR=/tmp/shots`（模拟器里截图写到模拟器沙盒的 /tmp，可以只看日志中的 STEP_OK / IGES_OK）。

## 7. 用免费 Apple ID 装到自己的 iPhone

1. iPhone 用数据线连 Mac，手机上点“信任此电脑”；iOS 16+ 需在 设置 → 隐私与安全性 → **开发者模式** 打开并重启。
2. `open ios/Runner.xcworkspace`（注意是 workspace），左侧选 Runner 工程 → TARGETS Runner → **Signing & Capabilities**：
   - 勾 “Automatically manage signing”；
   - Team 选 “Add an Account…” 登录你的 Apple ID，出现 “你的名字 (Personal Team)”；
   - **Bundle Identifier** 改成全球唯一，例如 `com.zhizhiwu.stl2stp`（`com.stl2stp.stl2stpApp` 可能被占用）。
3. 回终端：`flutter run -d <你的 iPhone 名称> --release`。
4. 首次运行手机会提示“不受信任的开发者”：设置 → 通用 → VPN 与设备管理 → 信任你的 Apple ID。
5. 免费个人团队的限制：描述文件 **7 天过期**（过期后重新 `flutter run` 即可），同时最多 3 个自签 App，不能用 TestFlight / App Store，部分能力（推送、iCloud 等）不可用——本 App 不需要这些能力。

## 8. App 图标、启动页、名称

- **图标**：准备 1024×1024 PNG（不透明、无圆角）。最省事：`flutter pub add --dev flutter_launcher_icons`，在 `pubspec.yaml` 加
  ```yaml
  flutter_launcher_icons:
    ios: true
    android: true
    image_path: assets/icon/icon.png
    remove_alpha_ios: true
  ```
  再 `dart run flutter_launcher_icons`。或在 Xcode 的 `Runner/Assets.xcassets/AppIcon` 里直接拖图。
- **启动页**：`ios/Runner/Base.lproj/LaunchScreen.storyboard`（Xcode 里改背景色、居中放图标）。
- **名称**：`ios/Runner/Info.plist` 的 `CFBundleDisplayName` 已是 `STL2STP`；中文名可加 `zh-Hans.lproj/InfoPlist.strings`：`"CFBundleDisplayName" = "STL转STEP";`。
- 版本号：`pubspec.yaml` 的 `version: 1.0.0+1`（`+` 后是 build 号，每次上传 TestFlight 必须递增）。

## 9. 付费开发者账号、TestFlight、App Store

1. 加入 Apple Developer Program（个人 **99 美元/年**，国内约 688 元/年）：https://developer.apple.com/programs/ 。可用 Apple ID 在 “Apple Developer” App 里注册。
2. Xcode 里 Team 换成付费团队；在 https://appstoreconnect.apple.com 新建 App（Bundle ID 与工程一致，主语言选简体中文）。
3. 打包上传：
   ```bash
   cd app
   flutter build ipa --release        # 产物 build/ios/ipa/*.ipa 与 build/ios/archive/Runner.xcarchive
   open build/ios/archive/Runner.xcarchive   # Xcode Organizer → Distribute App → App Store Connect → Upload
   ```
   （也可用 Apple 的 Transporter App 上传 `.ipa`。）
4. **TestFlight**：上传处理完成后，在 App Store Connect → TestFlight 加内部测试员（最多 100 人，无需审核）；外部测试需要 Beta 审核。
5. **上架**：填写描述、截图（6.7"/6.9" iPhone 必需，iPad 若支持也要）、隐私（本 App 不收集数据，“数据不会被收集”）、出口合规（未用自定义加密，选“否”）。
6. **中国大陆区**：自 2023 年起，在中国大陆上架的新 App 需要先完成 **ICP 备案（App 备案）**，并在 App Store Connect 填备案号；否则只能先上其他地区。请提前办理。
7. 内购（以后的“解锁完整版”）：见下面 9.1。

### 9.1 内购：StoreKit + `in_app_purchase`

代码里已经有完整的框架，见 `docs/flutter-app.md` §8。
- iOS 上自动使用 `StorePurchaseService`（`in_app_purchase` 3.3，底层是 StoreKit）。
- 免费额度默认不限制，所以不配置内购也能正常使用 App。
- 下面的步骤都没在 Mac 上验证过，按顺序做：

1. **协议和商品**：
   - App Store Connect → 协议、税务和银行业务：签“付费 App 协议”，填银行和税务信息。不签的话商品查不到。
   - 你的 App → 营利 → App 内购买项目 → “+” → **非消耗型项目**：
     - 产品 ID 填 `com.stl2stp.app.pro`。如果要用别的 ID，改 `lib/purchase/purchase_service.dart` 里的 `PurchaseConfig.proProductId`。
     - 填参考名称、价格档位、中英文显示名称和描述，以及审核截图（可以用购买页截图）。
   - 价格只在这里设，App 显示的是商店返回的本地化价格。
2. **Xcode 能力**：Runner target → Signing & Capabilities → “+ Capability” → **In-App Purchase**。这一步需要付费开发者账号，免费个人团队测不了内购。
3. **模拟器本地测试**（不需要联网和账号）：
   - Xcode → File → New → File… → **StoreKit Configuration File**，命名 `Products.storekit`，勾选 “Sync with App Store Connect” 或手动添加。
   - 手动添加时：“+” → Add Non-Consumable，Product ID `com.stl2stp.app.pro`，价格随便填（只在本地用）。
   - Product → Scheme → Edit Scheme… → Run → Options → **StoreKit Configuration** 选 `Products.storekit`。
   - 从 Xcode 运行（`open ios/Runner.xcworkspace`，再按 ⌘R）。`flutter run` 不会带上 StoreKit 配置。
   - 测试：
     - 设置 → 购买 → 购买，按钮上显示的是配置文件里的价格。
     - 买完设置页显示“完整版（已解锁）”。
     - 删掉 App 重新装，点“恢复购买”能恢复。
     - Xcode → Debug → StoreKit → Manage Transactions 里可以退款或删除交易，用来测各种情况。
4. **真机沙盒测试**：
   - App Store Connect → 用户和访问 → 沙盒测试员，新建一个测试 Apple ID。
   - iPhone：设置 → App Store → 沙盒账户，登录这个 ID。
   - `flutter run --release` 后购买，会弹出“沙盒环境”提示，不扣费。
5. **TestFlight**：内购同样走沙盒，不扣费。
6. **上架前要补的**：
   - 交易校验：现在只把“已解锁”存在本机。可以换 StoreKit 2 的签名交易（`in_app_purchase_storekit` 的 SK2 接口）在设备上校验，或接服务器校验。
   - 退款处理。
   - 定好免费额度：在 `main.dart` 里给 `AppState` 传 `FreeTierLimits(...)`。
   - 去掉购买页的黄色占位横幅：设计文案 `paywall.placeholder_banner` 和权益后面的“待定”。
   - 审核要求界面上有“恢复购买”按钮，这个已经有了。
7. 不想连商店时（例如没配 StoreKit 文件就跑模拟器），启动参数加环境变量 `STL2STP_MOCK_STORE=1`，会改用模拟商店（Xcode → Scheme → Run → Arguments → Environment Variables）。

### 9.2 分享（share_plus）

- iOS 上“分享…”打开系统分享面板（微信、QQ、隔空投送、存到“文件”等），不需要额外配置。
- iPad 上分享面板以弹出框形式出现，代码已经给了锚点位置；在 iPad 模拟器上点一次“分享…”确认不会崩溃。
- `Info.plist` 已声明 STL 文档类型（UTI `com.stl2stp.stl`）。App 能不能出现在微信、“文件”等 App 的“用其他应用打开”列表里，要在真机上验证。

### 9.3 后台转换与内存警告

后台任务和实时内存查询已经集成在 app/ios/Runner/AppDelegate.swift，通过 stl2stp/background 和 stl2stp/memory 两个通道与 Dart 通信。无需手动复制 Swift 片段。后台时间到期会通知 Dart 安全取消，并结束对应的系统任务；可用内存查询使用 os_proc_available_memory。

这些桥接逻辑仍需在 Mac/iPhone 上验证，不保证系统一定给予固定时长的后台运行时间。Flutter 的内存压力通知继续由引擎传到 Dart。

验证清单：

1. 模拟器启动后确认使用真实 FFI 核心，并完成示例 STL → STEP → 回读。
2. 真机转换时切到后台再回来，验证任务结束或继续；系统触发任务过期时应安全取消。
3. 模拟器触发 Memory Warning，转换应取消并显示内存提示。
4. 用 Xcode 检查可用内存通道返回值和 App 预算，不能把固定降级预算当作原生查询成功。
5. Mac 上构建 Release 后运行符号检查，并验证焊接开关与真实导出，不能只检查界面能否打开。

## 10. OCCT（LGPL 2.1 + 例外）静态链接合规——上架前必须确认

- iOS 上我们把 OCCT **静态**链接进 App。LGPL 2.1 第 6 条要求用户能用修改过的库**重新链接**你的程序。常见做法二选一：
  1. 提供 App 的**目标文件**（`.o`/静态库，不含 OCCT）和链接说明，让用户可以自己重链；或
  2. 把 OCCT 做成**动态 framework**随 App 分发（iOS 允许自带动态 framework），用户理论上可以替换。
- 无论哪种，都要在 App 内“开源许可”页附 LGPL 2.1 全文和 OCCT 例外条款，并说明从哪里获取 OCCT 源码。这部分已经做了：两个文本在 `app/assets/licenses/occt/`，设置 → 关于 → 开源许可里可以看全文，也出现在 Flutter 的许可页里（docs/flutter-app.md §12）。
- 以上是对一个**尚未确认的法律问题**的技术整理，不是法律意见。
- App Store 的使用条款与 (L)GPL 是否兼容存在争议，**以法务意见为准**。原 stl2stp 项目的许可证也需确认。

## 11. 提交 Mac 上的改动

从最新 master 建立功能分支；构建和测试后只暂存源码、配置及文档。不要暂存 app/ios/Stl2StpCore、Pods、生成的 xcconfig、build 缓存或签名私密文件。

```bash
cd ~/work/stl2stp
git switch -c ios/your-change
git status --short
# 按实际改动选择 git add 的路径，然后提交到功能分支并按仓库流程合并。
```

原本地 bundle/worktree 的离线交接流程不再是获取当前源码的前提。

## 12. 常见问题

| 现象 | 原因 / 处理 |
|---|---|
| CMake 报 `The C compiler ... is not able to compile a simple test program`（iOS） | 缺 `-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`，或 Xcode 路径不对：`sudo xcode-select -s /Applications/Xcode.app/Contents/Developer` |
| OCCT 用 Ninja 配置 iOS 失败 | 换 Xcode 生成器：脚本里 `-G Ninja` 改 `-G Xcode`，构建加 `--config Release` |
| `ld: symbol(s) not found ... _stl2stp_*` | 没跑第 5 步，或 `Stl2StpCore.xcconfig` 指向的 `.a` 不存在；看 `ls app/ios/Stl2StpCore/*/` |
| 链接报 `building for iOS Simulator, but linking in object file built for iOS` | 模拟器库用了真机产物；确认 `iphonesimulator/libStl2StpCore.a` 存在且 `lipo -info` 显示 arm64（模拟器） |
| 链接报 `Undefined symbols: std::__1::...` | 缺 `-lc++`（xcconfig 已加；检查 Build Settings → Other Linker Flags 是否同时有 Pods 的 `-ObjC` 和我们的项。若 Pods 的设置把我们的覆盖了，在 Runner target 的 Other Linker Flags 里手动加 `$(inherited)`） |
| App 运行后显示“演示模式” | Dart 没找到符号：确认 `STRIP_STYLE = non-global`、`-Wl,-u,_stl2stp_create` 生效；Release 下可在 Build Settings 搜 “Strip Style”。兜底：在 `AppDelegate.swift` 用桥接头调用一次 `stl2stp_default_export_options()`，确保符号被引用 |
| `pod install` 很慢/失败 | 设 CocoaPods 镜像，或 `pod repo update`；Apple 芯片若报 ffi 错误：`sudo arch -x86_64 gem install ffi` 后重试 |
| 真机安装提示“无法验证 App” | 第 7 步第 4 点，手机里信任开发者；或免费描述文件已过 7 天，重新 `flutter run` |
| 转换大模型时闪退 | 内存不足（手机比 PC 小得多）；先用“减少面数”，`AppState.largeFaces/limitFaces` 需按真机实测调整 |
| 包体太大 | 确认 Release、`-Os`、Dead Code Stripping=YES；看 `flutter build ipa --analyze-size` |
| OCCT 往控制台打印统计 | 正常（ui-spec §8 待办 6），不影响功能，后续在核心库里关闭 |

## 附：不用 Mac 时有哪些免费/低价的 iOS 打包方式（只作参考）

> 下面所有云方案都要把代码上传到第三方（通常还要求 git 远端），**与“只在本地、不上传”的约定冲突，目前不建议使用**。价格/额度会变，用前请到官网核实（这次调研时我这边无法访问这些网站，下列信息来自既有资料，未现场核实）。

| 方案 | 免费额度 / 价格（需核实） | 需要什么 | 能否装到手机 / 上架 |
|---|---|---|---|
| Codemagic（https://codemagic.io/pricing/） | 个人账号每月约 500 分钟 macOS（M2）免费构建 | 连接 GitHub/GitLab/Bitbucket 等 git 仓库；签名要 Apple 证书 | 有付费开发者账号才能出可安装 ipa / 传 TestFlight |
| GitHub Actions macOS runner（https://docs.github.com/en/billing/managing-billing-for-your-products/managing-billing-for-github-actions/about-billing-for-github-actions） | 公开仓库免费；私有仓库免费额度 2000 分钟/月，但 macOS 按 10 倍计，约等于 200 分钟 | 代码推到 GitHub | 同上 |
| Bitrise（https://bitrise.io/pricing） | 免费档近年多次调整/缩减 | 托管仓库 | 同上 |
| Xcode Cloud（https://developer.apple.com/xcode-cloud/） | 含在 99 美元/年的开发者会员里，每月 25 小时 | 付费开发者账号 + 仓库在 GitHub/GitLab/Bitbucket | 可直接发 TestFlight |
| 云 Mac 租用：MacinCloud（https://www.macincloud.com）、AWS EC2 Mac（https://aws.amazon.com/ec2/instance-types/mac/，至少按 24 小时计费）、Scaleway Apple silicon（https://www.scaleway.com/en/hello-m1/，24 小时起） | 按小时/按月付费，没有长期免费 | 远程桌面上装 Xcode；代码要传到对方机器 | 和自己的 Mac 一样 |
| 免费 Apple ID 个人团队 | 免费 | **必须有 Mac + Xcode** | 只能装到自己的设备，7 天过期，不能 TestFlight/上架 |
| TestFlight / App Store | 必须 99 美元/年开发者账号 | Mac 或上面的云方案 | 是 |

结论：要做 iOS，最终都绕不开 **Mac + Xcode**（本地或云端）和（上架时）**99 美元/年**的开发者账号。准备好 Mac 后按本文第 2～7 步走，全程本地，不需要任何云服务。
