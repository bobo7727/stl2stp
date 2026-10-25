# 环境复现（从空白机器开始）

[← 返回总入口](../HANDOFF.md)

> 本页所有版本号都是 2026-10-21 在开发机上用 `--version`、`dpkg -l`、`CMakeCache.txt`、`OpenCASCADEConfig.cmake` 核对的实际值。
> 当前命令入口是克隆仓库的 `mobile/`，先读 [QUICKSTART.md](../../QUICKSTART.md)。本页版本表记录原开发环境；新 Ubuntu 可以使用系统的现代 C++17 编译器。需要匹配历史 Ubuntu 20.04 环境时再显式选择 gcc-10/g++-10。

## 目录

1. [总览：要装什么、占多大、要多久](#1-总览要装什么占多大要多久)
2. [WSL 与 Ubuntu](#2-wsl-与-ubuntu)
3. [系统软件包（apt）](#3-系统软件包apt)
4. [OCCT 8.0.1（从源码编译）](#4-occt-801从源码编译)
5. [Flutter 3.38.7](#5-flutter-3387)
6. [Python 工具（标定、cadcheck）](#6-python-工具标定cadcheck)
7. [FreeCAD（外部校验用）](#7-freecad外部校验用)
8. [取得仓库](#8-取得仓库)
9. [磁盘与并行度约定](#9-磁盘与并行度约定)
10. [Mac / iOS 路线概要](#10-mac--ios-路线概要)

---

## 1. 总览：要装什么、占多大、要多久

| 组件 | 版本（实测） | 位置 | 大小 | 首次耗时 |
|---|---|---|---|---|
| WSL2 发行版 | Ubuntu 20.04.6 LTS，内核 6.18.54.1-microsoft-standard-WSL2 | WSL | — | 10 分钟左右 |
| gcc-10 / g++-10 | 10.5.0（`10.5.0-1ubuntu1~20.04`） | apt | 约 100 MB | 1–2 分钟 |
| CMake | 3.26.4 | `/usr/local/bin/cmake` | 约 150 MB | 1 分钟 |
| Ninja | 1.10.0（`ninja-build 1.10.0-1build1`） | apt | 小 | — |
| clang（Flutter Linux 构建用） | 10.0.0-4ubuntu1（`clang-10`） | apt | 约 300 MB | — |
| GTK 开发包 | `libgtk-3-dev 3.24.20-0ubuntu1.2` | apt | 约 100 MB | — |
| OCCT | 8.0.1（Shared、Release） | `~/occt-install` | 安装后 **123 MB**；源码包 45 MB；编译目录数 GB（编完可删） | **约 20 分钟**（12 核 `ninja -j12` 实测 1196 s） |
| Flutter SDK | 3.38.7 stable（Dart 3.10.7） | `/mnt/d/dev/flutter-linux` | **2.5 GB** | 下载 1.47 GB + 首次运行几分钟 |
| pub 缓存 | — | `/mnt/d/dev/pub-cache` | **184 MB** | `flutter pub get` 约 1 分钟 |
| 核心库 + 测试 | — | `<仓库>/build/rel` | 几百 MB（含测试生成的数据） | 编译约 1–2 分钟（-j4），`ctest -j4` 约 48 s（core-validation.md 记录） |

开发机硬件：12 核、23 GB 内存（WSL 内可见）。

## 2. WSL 与 Ubuntu

在 Windows PowerShell（管理员）里：

```powershell
wsl --install -d Ubuntu-20.04
wsl -l -v          # 期望看到 Ubuntu-20.04，VERSION 2
```

进入 WSL 后核对：

```bash
lsb_release -d     # Description: Ubuntu 20.04.6 LTS
uname -r           # 6.18.54.1-microsoft-standard-WSL2（内核版本随 Windows 更新变化，不影响构建）
nproc              # 开发机是 12
```

Linux 桌面版 App 需要 WSLg 显示窗口（Windows 11 自带；`echo $DISPLAY` 应有值）。

## 3. 系统软件包（apt）

### 3.1 需要的包和实测版本

| 包 | 实测版本 | 用途 | 开发机上的来源 |
|---|---|---|---|
| `gcc-10` `g++-10` `cpp-10` `libgcc-10-dev` `libstdc++-10-dev` `libasan6` | 10.5.0-1ubuntu1~20.04 | 编 OCCT 和核心库（**系统默认 gcc 9.4.0 编不了 OCCT 8**） | 本项目 2026-10-20 安装 |
| `build-essential` | 12.8ubuntu1.1 | 基础工具 | 原有 |
| `ninja-build` | 1.10.0-1build1 | 构建 | 原有 |
| `cmake`（apt） | 3.16.3-1ubuntu1.20.04.1 | 也装着，但实际用 `/usr/local/bin/cmake` 3.26.4 | 原有 |
| `git` | 2.25.1 | | 原有 |
| `curl` | 7.68.0 | 下载 OCCT / Flutter | 原有 |
| `clang-10` `clang` | 1:10.0.0-4ubuntu1 / 1:10.0-50~exp1 | Flutter Linux 构建（`/usr/bin/clang++`） | 原有 |
| `libgtk-3-dev` | 3.24.20-0ubuntu1.2 | Flutter Linux 构建 | 原有 |
| `liblzma-dev` | 5.2.4 | Flutter Linux 构建 | 原有 |
| `pkg-config` | 0.29.1 | Flutter Linux 构建 | 原有 |
| `unzip` `xz-utils` `zip` `libglu1-mesa` | — | Flutter SDK 要求 | 原有 |
| `mesa-utils` | — | 可选（`flutter doctor` 用 eglinfo 查驱动） | 原有 |
| `lld-10` | 1:10.0.0-4ubuntu1 | 本项目 2026-10-21 安装；**当前构建没有用到**（检查过 Flutter 生成的 build.ninja，没有 `-fuse-ld=lld`，链接器是 `/usr/bin/ld`）。可以不装 | 本项目安装 |
| `python3` `python3-pip` | 3.8.2-0ubuntu2（Python 3.8.10）/ 20.0.2 | 标定脚本、文案工具、cadcheck | 原有 |

### 3.2 一次装齐（空白 Ubuntu 20.04）

```bash
sudo apt-get update
# 先用 -s 模拟，确认不会升级任何已有包（协作约定：不做 apt upgrade）
sudo apt-get install -s --no-upgrade build-essential gcc-10 g++-10 ninja-build git curl \
  clang libgtk-3-dev liblzma-dev pkg-config unzip xz-utils zip libglu1-mesa python3 python3-pip
sudo apt-get install    --no-upgrade build-essential gcc-10 g++-10 ninja-build git curl \
  clang libgtk-3-dev liblzma-dev pkg-config unzip xz-utils zip libglu1-mesa python3 python3-pip
g++-10 --version | head -1      # g++-10 (Ubuntu 10.5.0-1ubuntu1~20.04) 10.5.0
clang++ --version | head -1     # clang version 10.0.0-4ubuntu1
ninja --version                 # 1.10.0
```

> 开发机上 gcc-10 一组 6 个包是用 `apt-get install --no-upgrade gcc-10 g++-10` 装的（`/var/log/dpkg.log` 2026-10-20 有记录，没有升级或删除别的包）。其它包是机器原有的，空白机器上一条命令装齐即可；Ubuntu 20.04 的源里版本是固定的，装到的就是上表的版本。

### 3.3 CMake

核心库最低要求 CMake 3.16（apt 的 3.16.3 能配置核心库），开发机实际用的是 `/usr/local/bin/cmake` 3.26.4（机器原有，来源未记录）。复现时用官方二进制包（URL 和 sha256 已核对存在）：

```bash
cd /tmp
curl -fLO https://github.com/Kitware/CMake/releases/download/v3.26.4/cmake-3.26.4-linux-x86_64.tar.gz
echo "ba1e0dcc710e2f92be6263f9617510b3660fa9dc409ad2fb8190299563f952a0  cmake-3.26.4-linux-x86_64.tar.gz" | sha256sum -c -
sudo tar xzf cmake-3.26.4-linux-x86_64.tar.gz -C /usr/local --strip-components=1
hash -r; cmake --version | head -1     # cmake version 3.26.4
```

（这段下载命令本次没有在开发机上执行，因为开发机已有 3.26.4；URL 和 sha256 已在线核对。）OCCT 8.0.1 自身要求的最低 CMake 版本没有核对，建议直接用 3.26.4。

## 4. OCCT 8.0.1（从源码编译）

当前自动化入口：在 mobile/ 运行 `bash tools/bootstrap-occt.sh --jobs 4`，会校验固定 8.0.1 源码摘要并在 build/occt 下构建。下文保留原开发环境的手动配方；记录见 [CHANGES-from-original.md §3](../CHANGES-from-original.md) 和 [build-linux.md](../build-linux.md)。

### 4.1 源码

```bash
mkdir -p ~/stl2stp-work/occt-src && cd ~/stl2stp-work/occt-src
curl -sSL -o V8.0.1.tar.gz https://github.com/Open-Cascade-SAS/OCCT/archive/refs/tags/V8.0.1.tar.gz
ls -l V8.0.1.tar.gz        # 45129885 字节（2026-10-21 重新下载核对）
sha256sum V8.0.1.tar.gz    # 6297cc55a1720523a437c54d07dc16b9da8c5f5ab5800da3c6ebded568d53c18
tar xzf V8.0.1.tar.gz      # 得到 OCCT-8.0.1/
```

### 4.2 配置、编译、安装

```bash
SRC=~/stl2stp-work/occt-src/OCCT-8.0.1
B=~/stl2stp-work/occt-src/build-linux-release
mkdir -p "$B" && cd "$B"
cmake -G Ninja "$SRC" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DINSTALL_DIR=$HOME/occt-install -DCMAKE_INSTALL_PREFIX=$HOME/occt-install \
  -DBUILD_LIBRARY_TYPE=Shared \
  -DBUILD_MODULE_FoundationClasses=ON -DBUILD_MODULE_ModelingData=ON \
  -DBUILD_MODULE_ModelingAlgorithms=ON -DBUILD_MODULE_DataExchange=ON \
  -DBUILD_MODULE_ApplicationFramework=OFF -DBUILD_MODULE_Visualization=OFF \
  -DBUILD_MODULE_Draw=OFF -DBUILD_MODULE_DETools=OFF \
  -DBUILD_DOC_Overview=OFF -DBUILD_SAMPLES_QT=OFF \
  -DUSE_FREETYPE=OFF -DUSE_TK=OFF -DUSE_TCL=OFF -DUSE_OPENGL=OFF -DUSE_GLES2=OFF -DUSE_XLIB=OFF \
  -DUSE_FREEIMAGE=OFF -DUSE_RAPIDJSON=OFF -DUSE_DRACO=OFF -DUSE_TBB=OFF -DUSE_VTK=OFF
ninja -j12 > ../build.log 2>&1        # 12 核约 1196 s；内存紧张时用 -j4（约慢 2–3 倍）
ninja install > ../install.log 2>&1
```

### 4.3 核对安装结果（本次实测）

```bash
du -sh ~/occt-install                                   # 123M
ls ~/occt-install/lib/libTK*.so | wc -l                 # 48
grep OCC_VERSION_COMPLETE ~/occt-install/include/opencascade/Standard_Version.hxx   # "8.0.1"
grep -E "BUILD_SHARED_LIBS|BUILD_TYPE|CXX_FLAGS|WITH_" ~/occt-install/lib/cmake/opencascade/OpenCASCADEConfig.cmake
#   OpenCASCADE_BUILD_SHARED_LIBS ON, BUILD_TYPE Release, CXX_FLAGS " -fexceptions -fPIC -Wall -Wextra",
#   WITH_TCL / WITH_FREETYPE / WITH_FREEIMAGE / WITH_TBB / WITH_FFMPEG OFF
```

- 模块列表里会出现 Visualization 和 ApplicationFramework 的几个 toolkit（`TKV3d`、`TKService`、`TKXCAF`、`TKCAF` …），这是 DataExchange 的依赖闭包自动拉进来的，正常。
- 编完可以删掉 `~/stl2stp-work`（开发机上已经删了），只留 `~/occt-install`。
- 【间接核对】编译目录已删，本次没能重读 `CMakeCache.txt`；上面的参数取自当时与 CMakeCache 核对过的记录，并与安装目录的 `OpenCASCADEConfig.cmake` 一致。
- 为什么 OCCT 装在 `~/` 而不是 `/mnt/d`：WSL 访问 `/mnt/d`（Windows 盘）很慢，OCCT 的 `.so` 在每次链接和运行时都要读取。代价是占 WSL 虚拟磁盘（在 C: 盘上）123 MB。

## 5. Flutter 3.38.7

### 5.1 安装 SDK（放 D: 盘，不改 `~/.bashrc`）

```bash
mkdir -p /mnt/d/dev && cd /mnt/d/dev
curl -fLO https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_3.38.7-stable.tar.xz
echo "2d72de31119ccba1421391aa9ab53891a3e4905987a13f8272766ad78e3bbf93  flutter_linux_3.38.7-stable.tar.xz" | sha256sum -c -
tar xf flutter_linux_3.38.7-stable.tar.xz && mv flutter flutter-linux && rm flutter_linux_3.38.7-stable.tar.xz
```

URL、大小（1,466,122,604 字节）和 sha256 取自 Flutter 官方发布清单 `releases_linux.json`（2026-10-21 在线核对）；开发机上的 SDK 已存在，本次没有重新下载。

每次使用前（只影响当前 shell）：

```bash
export PATH=/mnt/d/dev/flutter-linux/bin:$PATH PUB_CACHE=/mnt/d/dev/pub-cache
flutter --version
```

期望输出（实测）：

```text
Flutter 3.38.7 • channel stable • https://github.com/flutter/flutter.git
Framework • revision 3b62efc2a3 • 2026-01-13 13:47:42 -0800
Engine • revision 78fc3012e4
Tools • Dart 3.10.7 • DevTools 2.51.1
```

`flutter doctor -v` 的关键行（实测）：`[✓] Flutter (Channel stable, 3.38.7 ...)`、`[✓] Linux toolchain`（clang 10.0.0-4ubuntu1、cmake 3.26.4、ninja 1.10.0、pkg-config 0.29.1）、`Linux (desktop) • linux`。Android 缺 JDK、找不到 Chrome 的提示可以忽略（不做 Android/Web）。

### 5.2 依赖包

```bash
cd "$(git rev-parse --show-toplevel)/mobile/app" && flutter pub get
```

`pubspec.lock` 已入库，锁定 98 个包。直接依赖（实际解析版本）：

| 包 | 版本 | 用途 |
|---|---|---|
| `ffi` | 2.2.0 | `calloc`、`Utf8` 等 FFI 工具 |
| `path` | 1.9.1 | 路径处理 |
| `path_provider` | 2.1.6 | 应用数据目录（输出、历史） |
| `shared_preferences` | 2.5.5 | 设置、购买状态、每日次数 |
| `file_picker` | 13.1.0 | 系统文件选择器 |
| `share_plus` | 13.3.1 | 系统分享面板 |
| `in_app_purchase` | 3.3.1 | StoreKit（iOS） |
| `desktop_drop` | 0.8.0 | 桌面拖放 |
| `cupertino_icons` | 1.0.9 | 图标 |
| `flutter_localizations` / `flutter_test` / `integration_test` | SDK | |
| `flutter_lints` | 6.0.0 | 静态检查规则 |

`pubspec.yaml` 的 `environment.sdk` 是 `^3.10.7`，即至少 Dart 3.10.7。

## 6. Python 工具（标定、cadcheck）

| 脚本 | 依赖 | 开发机 |
|---|---|---|
| `app/tool/app_strings.py`、`app/tool/sync_strings.sh` | 标准库 | Python 3.8.10 |
| `app/tool/calib/calib.py` `calib2.py` `memtime.py` `weldtime.py` | 标准库（调用 `stl2stp-cli`；memtime 用 `os.wait4` 读子进程的 `ru_maxrss`） | 同上 |
| `core/tools/cadcheck/gen.py` `step_check.py` `pcurve_check.py`（当前 master） | 标准库 | 同上 |
| `core/tools/cadcheck/render.py` | `matplotlib`（及 `numpy`） | 已有 matplotlib 3.7.5、numpy 1.24.4 |
| `core/tools/cadcheck/fc_check.py` | FreeCAD 自带的 Python（`FreeCAD`、`Part` 模块） | Windows 版 FreeCAD 1.0 |

需要画图时：`python3 -m pip install --user matplotlib`（装到 `~/.local`，不影响系统）。

## 7. FreeCAD（外部校验用）

- 开发机用的是 **Windows 版 FreeCAD 1.0.0**（内置 OCC 7.8.1），用它自带的 `FreeCADCmd.exe` 跑 `fc_check.py`，见 [validation.md](validation.md)。
- 用不同版本的 OCCT（7.8.1）读我们用 8.0.1 写的文件，本身就是一种交叉验证。
- 在 Windows 上安装属于用户自己的决定；不装也不影响构建和测试，只是少了一层外部验证。

## 8. 取得仓库

在 Linux、WSL 或 Mac 的目标工作目录运行：

```bash
git clone https://github.com/bobo7727/stl2stp.git
cd stl2stp/mobile
bash tools/bootstrap-occt.sh --jobs 4
bash tools/verify.sh --jobs 4 --cad
```

远端 master 已包含 App、设计稿和 CAD 工具，不需要 checkout 原 ZIP 的 app/flutter 或 validate/cad 分支。构建不依赖 reference/；对照桌面版可直接查看本仓库根目录保留的 MFC 工程。

下载/源码/编译/安装默认位于 mobile/build/occt，参数可覆盖。旧的 home 安装配方仍可用于已有环境，通过 `tools/verify.sh --occt-root /实际安装路径` 指定。

## 9. 磁盘与并行度约定

- WSL 的虚拟磁盘在 C: 盘，开发机 C: 只剩约 44 GB。**所有构建产物放 D:**（仓库内的 `build/`），Flutter SDK 和 pub 缓存也放 D:。WSL 家目录里只放 OCCT 安装（123 MB）。
- 大构建前看一眼 C: 剩余：`df -h /mnt/c`，少于 25 GB 就先清理。
- 并行度用 4：`export CMAKE_BUILD_PARALLEL_LEVEL=4`、`ctest -j4`。12 核机器上全速编译会让 Windows 卡顿；OCCT 一次性编译可以 `-j12`。
- 测试时把临时文件指到 D:：`TMPDIR=$PWD/build/tmp`，App 测试还加 `XDG_DATA_HOME=$PWD/build/xdg-data`。
- 不要对 WSL 做 `wsl --shutdown`（会中断所有正在跑的构建）。

## 10. Mac / iOS 路线概要

详细步骤见 [mac-setup-guide.md](../mac-setup-guide.md)。概要：

1. Mac（Apple 芯片推荐）+ Xcode + CLT + Homebrew（`cmake`、`ninja`、`cocoapods`）+ Flutter 3.38.7 macOS SDK（与 Linux 同版本）。
2. 克隆 GitHub master 并进入 mobile/，见上面的取得仓库步骤。
3. （可选）在 macOS 上编一遍核心库跑 122 个测试。
4. `bash app/tool/ios/build_ios_core.sh`：下载并校验同一个 OCCT 源码包，编 iOS 静态版 OCCT + 核心库（真机 arm64、模拟器 arm64[+x86_64]），生成 `app/ios/Stl2StpCore/*` 和 `app/ios/Flutter/Stl2StpCore.xcconfig`，不用手改 Xcode 工程。**这个脚本写于 Windows 上，还没在 Mac 上运行过。**
5. `flutter run` 到模拟器 → 免费 Apple ID 装到自己的 iPhone → 付费开发者账号（¥688/年）上 TestFlight / App Store。
6. 按 `mac-setup-guide.md §9.3` 验证已集成的后台任务和内存通道。
7. iOS bundle id 是 `com.stl2stp.stl2stpApp`（Linux 的 application id 是 `com.stl2stp.stl2stp_app`），内购商品 ID `com.stl2stp.app.pro`。
