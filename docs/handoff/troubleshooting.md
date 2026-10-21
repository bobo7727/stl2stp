# 故障排查

[← 返回总入口](../HANDOFF.md)

按“现象 → 原因 → 解决”排列。路径以 `$R` 表示 app/flutter worktree 在 WSL 里的路径（例如 `/mnt/d/stl2stp-flutter`）。

## 目录

1. [编译 OCCT 和核心库](#1-编译-occt-和核心库)
2. [Flutter App](#2-flutter-app)
3. [测试](#3-测试)
4. [Windows / PowerShell / WSL 之间](#4-windows--powershell--wsl-之间)
5. [磁盘与内存](#5-磁盘与内存)

---

## 1. 编译 OCCT 和核心库

| 现象 | 原因 | 解决 |
|---|---|---|
| 编 OCCT 8.0.1 时在 `Bnd_Box.hxx` 报 constexpr / defaulted / noexcept 构造函数错误 | Ubuntu 20.04 默认的 g++ 9.4 不支持 | 装 `g++-10`（[environment.md](environment.md)），configure 时加 `-DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10`。改编译器后要删掉 build 目录重新 configure（CMake 会缓存编译器） |
| `cmake --version` 显示 3.16.3 | 用到了 Ubuntu 20.04 自带的 `/usr/bin/cmake`（核心库够用，但和文档记录的 3.26.4 不一致；Flutter 的 Linux 构建也用 PATH 里的 cmake） | 按 [environment.md](environment.md) 装 3.26.4 到 `/usr/local/bin`（它在 PATH 里排在 `/usr/bin` 前面）；`which -a cmake` 检查顺序 |
| `OCCT headers not found. Set OCCT_ROOT or OCCT_INCLUDE_DIR.` 或 `OCCT libraries not found. Set OCCT_ROOT or OCCT_LIBRARY_DIR.` | 没告诉 CMake OCCT 装在哪（`core/CMakeLists.txt`） | configure 时加 `-DOCCT_ROOT=$HOME/occt-install` |
| 运行 `stl2stp-cli` 报 `error while loading shared libraries: libTKDE.so.8.0`（或别的 `libTK*.so`） | OCCT 的 `.so` 本身不带 RPATH，默认的 `DT_RUNPATH` 只覆盖直接依赖 | CMakeLists 已用 `LINKER:--disable-new-dtags` 写 `DT_RPATH`。检查：`readelf -d build/rel/stl2stp-cli \| grep -i path` 应显示 `~/occt-install/lib` 的绝对路径。如果是自己改过链接参数，恢复它；**不要**改全局 `LD_LIBRARY_PATH` 或运行 `ldconfig` |
| 换了用户名或 OCCT 安装路径后程序找不到 OCCT | RPATH 是绝对路径 | 用新的 `OCCT_ROOT` 重新 configure（`OCCT_LIBRARY_DIR` 会缓存，最好删掉 build 目录）并编译 |
| 同一份源码编出的 `libstl2stp-core.so` sha256 和文档不一致 | RPATH 里含 home 目录路径，用户名不同 sha 就不同 | 正常。比较转换结果（固定 `--timestamp`）而不是比较 `.so` |

## 2. Flutter App

| 现象 | 原因 | 解决 |
|---|---|---|
| 转换几乎瞬间完成，输出文件很小、内容是占位文字 | App 跑在**演示模式**（`DemoEngine`）：找不到 `libstl2stp-core.so` | 看 `flutter build linux` / `flutter run` 的输出里有没有 `stl2stp-core not found at …; the app will run in demo mode`。先编 `build/core-shared`（[build-test.md](build-test.md)），或设 `STL2STP_CORE_LIB=/绝对路径/libstl2stp-core.so`，再重新构建。另外确认没设 `STL2STP_DEMO=1` |
| 点“导入 STL 文件”后弹出“输入文件路径”的对话框 | WSLg 下没有 XDG 桌面门户，系统文件选择器不可用 | 正常的回退。直接粘贴 WSL 路径（Windows 的 `D:\a.stl` 写成 `/mnt/d/a.stl`）；或者命令行 `stl2stp_app /mnt/d/a.stl` 直接打开；首页也有“用示例模型试试” |
| 窗口 800×600 或更宽时出现左右分栏 | 分栏规则：宽 ≥ 900，或最短边 ≥ 600 且宽 ≥ 700（`home_screen.dart` 的 `useSplitLayout`） | 正常（视为平板）。Linux 窗口默认 430×900 是手机布局 |
| Linux 窗口里中文显示成方块 | WSL 没装中文字体 | `fc-list :lang=zh` 检查；没有的话装 `fonts-noto-cjk`（会占约 100 MB，记得记录） |
| 购买页一直转圈或报错 | 桌面上用的是模拟商店，iOS 上 StoreKit 没配置商品 | 桌面正常；iOS 模拟器没有 StoreKit 配置时设 `STL2STP_MOCK_STORE=1` |
| 设置里的版本号和 `pubspec.yaml` 不一致 | 已知问题（写死 `0.1.0`） | 见 [roadmap.md N-3](roadmap.md#3-不需要-mac) |

## 3. 测试

| 现象 | 原因 | 解决 |
|---|---|---|
| Widget 测试在默认尺寸下找不到手机布局的按钮 | `flutter test` 默认画布是 800×600，按规则算平板分栏 | 在测试里 `await tester.binding.setSurfaceSize(const Size(430, 1400));`，结束时 `setSurfaceSize(null)`（参照 `test/widget_flow_test.dart`） |
| 从 PowerShell 启动集成测试时窗口起不来 / 报没有显示 | 非交互 bash 可能没有拿到 WSLg 的显示环境变量 | 用 `wsl -d Ubuntu-20.04 -e bash -ic "<命令>"`（`-i` 交互式） |
| 集成测试里的转换显示演示结果 | 同上“演示模式” | 先编 `build/core-shared` |
| `assets match design/ui string tables` 失败 | 改了 `app/assets/i18n/*.json` 但没改设计稿表（或反过来） | 按 [recipes.md](recipes.md) 的文案流程：改 `design/ui/tools/build_strings.py` → 运行 → `bash tool/sync_strings.sh` |
| `test/core_test.dart` 的估算测试失败 | 改了估算系数或标定数据，实测点落到范围外 | 按 [operations.md §2](operations.md#2-估算与内存模型的系数及重新标定) 重新标定；不要只放宽测试 |
| ASan 测试日志无限增长（曾到 31 GB），终端刷 `AddressSanitizer:DEADLYSIGNAL` | gcc-10 的 ASan 和 WSL2 6.x 内核的地址随机化冲突 | 只用 `core/tools/run-sanitizer-ctest.sh`（内置 `setarch -R` 和日志上限）。如果已失控：`pkill -f ctest`，删掉对应 build 目录下的日志 |
| ASan 报 OCCT 的内存泄漏 | OCCT 8.0.1 每次传输在退出时留下约 7.6 MB | 脚本默认关闭 LeakSanitizer；不是本项目的问题 |
| `ctest` 里个别测试超时 | 机器上同时有别的重任务 | 用 `-j4`，空闲时重跑；正常全部约 48 s |

## 4. Windows / PowerShell / WSL 之间

| 现象 | 原因 | 解决 |
|---|---|---|
| 在 WSL 里对 `/mnt/d/stl2stp-flutter` 等 worktree 运行 `git` 报 `fatal: not a git repository: /mnt/d/stl2stp-flutter/D:/stl2stp-ios/.git/worktrees/stl2stp-flutter` | worktree 是 Windows 的 git 建的，`.git` 文件里写的是 `D:/…` 的 Windows 路径 | 在 PowerShell 里用 `git -C D:\stl2stp-flutter …`；构建、测试在 WSL 里跑 |
| 从 PowerShell 传给 `wsl … bash -c "…"` 的命令里 `$_`、`$HOME`、`$(…)` 不见了或被替换 | PowerShell 先展开了 `$` 变量 | 把命令写进 `.sh` 文件，`wsl -d Ubuntu-20.04 -e bash /mnt/d/…/x.sh` 执行；或者在 PowerShell 里用单引号 |
| `.sh` 脚本在 WSL 里报 `$'\r': command not found` 或第一行乱码 | 文件是 CRLF 或带 UTF-8 BOM（PowerShell 5 的 `Set-Content -Encoding utf8` 会加 BOM，且不支持 `utf8NoBOM`） | 在 WSL 里写脚本，或用编辑器保存为 UTF-8 无 BOM + LF；仓库的 `.gitattributes` 已强制 LF |
| WSL 里 `sudo` 等待输入卡住 | 非交互调用没有终端 | 在交互终端里手动执行 `sudo`；不要把密码写进脚本或文件 |
| 在 Windows 资源管理器里改了 `\\wsl$\…` 下的文件后权限怪异 | 跨文件系统 | 源码和输出都放在 `D:`（`/mnt/d`），只把 OCCT 安装在 WSL 的 `~/occt-install` |

## 5. 磁盘与内存

| 现象 | 原因 | 解决 |
|---|---|---|
| C: 剩余空间快速下降 | WSL 的虚拟磁盘 `ext4.vhdx` 在 C: 上，只增不减 | 构建前 `df -h /mnt/c` 检查（建议 ≥ 25 GB）；大输出写到 `/mnt/d`；WSL home 里不放大文件 |
| 大模型转换时 WSL 被杀（`Killed`）或整机变慢 | 内存不够：10 万三角形 STEP 峰值约 2 GB | 先减面；编译时用 `-j4` 而不是 `-j$(nproc)`，避免 OCCT 这类大项目同时开太多编译进程。参考 [risks-faq-glossary.md §3](risks-faq-glossary.md#3-性能范围面数-vs-时间大小内存) |
| `build/` 目录很大 | 标定、截图、cadcheck 的输出 | 可以删掉 `build/` 下除 `rel`、`core-shared` 以外的目录；标定 CSV 在 `app/tool/calib/` 已入库 |
