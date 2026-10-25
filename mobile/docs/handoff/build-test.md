# 构建、运行、测试

[← 返回总入口](../HANDOFF.md)

> 以下命令从当前仓库的 mobile/ 执行，`$R` 表示 mobile 的绝对路径。优先使用 [QUICKSTART.md](../../QUICKSTART.md) 中的工具入口；测试数量和产物大小是原环境的历史基线。
> 公共环境（只影响当前 shell，不改 `~/.bashrc`）：

```bash
R="$(git rev-parse --show-toplevel)/mobile"
cd "$R"
export OCCT_ROOT="${OCCT_ROOT:-$R/build/occt/install}"
export CMAKE_BUILD_PARALLEL_LEVEL=4
# 先把自己的 Flutter SDK 加入 PATH；PUB_CACHE 可按需设置。
mkdir -p $R/build/tmp
```

## 目录

1. [核心库（CMake + CTest）](#1-核心库cmake--ctest)
2. [命令行工具 stl2stp-cli](#2-命令行工具-stl2stp-cli)
3. [App 测试](#3-app-测试)
4. [Linux 发布包](#4-linux-发布包)
5. [运行 App、演示模式、环境变量](#5-运行-app演示模式环境变量)
6. [怎样证明两台机器的构建等价](#6-怎样证明两台机器的构建等价)
7. [工具脚本：标定、文案、cadcheck、sanitizer](#7-工具脚本标定文案cadchecksanitizer)
8. [截图的重新生成](#8-截图的重新生成)
9. [测试目录：CTest 122 个](#9-测试目录ctest-122-个)
10. [测试目录：Flutter 55 个 + 集成测试 3 个](#10-测试目录flutter-55-个--集成测试-3-个)
11. [怎样新增测试](#11-怎样新增测试)

---

## 1. 核心库（CMake + CTest）

### 1.1 CMake 选项

| 选项 | 默认 | 说明 |
|---|---|---|
| `OCCT_ROOT` | — | OCCT 安装目录（`$HOME/occt-install`）；也可分别设 `OCCT_INCLUDE_DIR` / `OCCT_LIBRARY_DIR` |
| `STL2STP_WITH_OCCT` | ON | OFF 时编 `exchange_disabled.cpp`（只有网格功能，导出返回 ERROR_OCCT），用于无 OCCT 的环境 |
| `STL2STP_BUILD_CLI` | ON | 编 `stl2stp-cli` |
| `BUILD_TESTING` | ON | 编测试并注册 CTest |
| `BUILD_SHARED_LIBS` | OFF | ON 时产出 `libstl2stp-core.so`（App 用）；iOS 用 OFF（静态） |
| `CMAKE_BUILD_TYPE` | — | 一律用 `Release` |

开发机上 `build/rel`（core/weld worktree）的 `CMakeCache.txt` 实际值：`BUILD_TESTING=ON`、`CMAKE_BUILD_TYPE=Release`、`CMAKE_C_COMPILER=gcc-10`、`CMAKE_CXX_COMPILER=g++-10`、`OCCT_ROOT=~/occt-install`、`STL2STP_BUILD_CLI=ON`、`STL2STP_WITH_OCCT=ON`、生成器 Ninja。

### 1.2 编译并跑 122 个测试

```bash
cd $R
TMPDIR=$R/build/tmp cmake -S core -B build/rel -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="${CC:-cc}" -DCMAKE_CXX_COMPILER="${CXX:-c++}" -DOCCT_ROOT="$OCCT_ROOT"
cmake --build build/rel
TMPDIR=$R/build/tmp ctest --test-dir build/rel -j4 --output-on-failure
```

期望结尾：

```text
100% tests passed, 0 tests failed out of 122
Label Time Summary:
metadata    = ...
mesh        = ...
occt        = ...
review      = ...
```

- 按标签只跑一部分：`ctest --test-dir build/rel -L review`（`mesh` / `occt` / `metadata` / `review`）。
- 按名字：`ctest --test-dir build/rel -R weld`。
- 列出全部：`ctest --test-dir build/rel -N`（122 个，名字都以 `stl2stp.` 开头）。
- 测试数据由 `stl2stp.fixtures` 自动生成到 `build/rel/tests/data/`，不需要任何外部文件。

### 1.3 App 用的共享库

```bash
cd $R
TMPDIR=$R/build/tmp cmake -S core -B build/core-shared -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="${CC:-cc}" -DCMAKE_CXX_COMPILER="${CXX:-c++}" -DOCCT_ROOT="$OCCT_ROOT" \
  -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF
cmake --build build/core-shared            # 新目录从零编译约 15 s（-j4，实测）
ls -l build/core-shared/libstl2stp-core.so # 342584 字节
sha256sum build/core-shared/libstl2stp-core.so
# 3cc9d4866fc6924068c473e6db789f91c741719b616019fe0a95e0bad542aedc（core/weld 合并后的源码，用户名不同时会变，见 §6）
readelf -d build/core-shared/libstl2stp-core.so | grep -E "NEEDED|RPATH"
```

`readelf` 期望：`RPATH` = `<家目录>/occt-install/lib`；`NEEDED` = `libTKDESTEP.so.8.0`、`libTKDEIGES.so.8.0`、`libTKXSBase.so.8.0`、`libTKShHealing.so.8.0`、`libTKTopAlgo.so.8.0`、`libTKBRep.so.8.0`、`libTKMath.so.8.0`、`libTKernel.so.8.0`，以及 `libpthread`、`libstdc++`、`libm`、`libgcc_s`、`libc`。

Linux 版 App 构建时会把这个文件拷进 bundle（`app/linux/CMakeLists.txt` 默认找 `<仓库>/build/core-shared/libstl2stp-core.so`，可用环境变量 `STL2STP_CORE_LIB` 指定别处；找不到时只给警告，App 以演示模式运行）。

## 2. 命令行工具 stl2stp-cli

```text
Usage:
  stl2stp-cli info <in.stl>
  stl2stp-cli simplify <in.stl> <out.stl> --faces N [--no-weld] [--weld-tol UNITS]
  stl2stp-cli convert <in.stl> <out.step|out.igs> [--mode original|sewing] [--merge-coplanar] [--simplify N]
      [--merge-angle RADIANS] [--strict-original] [--no-weld] [--weld-tol UNITS]
      [--author TEXT] [--org TEXT] [--product TEXT] [--description TEXT] [--timestamp ISO8601]
  stl2stp-cli readback <in.step>
```

| 参数 | 适用 | 默认 | 作用 |
|---|---|---|---|
| `--mode original\|sewing` | convert | original | 转换模式，见 [algorithms.md](algorithms.md) |
| `--merge-coplanar` | convert | 关 | 共面合并 |
| `--merge-angle R` | convert | 1e-4 | 共面合并角度容差（弧度，(0, 0.1]） |
| `--simplify N` | convert | 不减面 | 先 QEM 减到 N 个三角形 |
| `--faces N` | simplify | 必填 | 目标三角形数 |
| `--strict-original` | convert | 关 | 原版旧行为：不校验、不修复、不焊接（可能写出无效实体） |
| `--no-weld` | convert, simplify | 焊接开 | 关闭容差焊接，只合并坐标完全相同的点（与原版一致） |
| `--weld-tol U` | convert, simplify | 自动 | 焊接容差（模型单位，必须 > 0）；自动 = max(1e-6×包围盒对角线, 2×FLT_EPSILON×最大坐标) |
| `--author/--org/--product/--description T` | convert | 空 | 文件信息；STEP 无损写中文，IGES 非 ASCII 替换为 `?` |
| `--timestamp ISO8601` | convert | 当前 UTC | 例如 `2026-10-21T10:30:00+08:00`，写入时换算成 UTC |

扩展名决定格式：`.step`/`.stp` → STEP，`.igs`/`.iges` → IGES。退出码：0 成功（含警告），1 失败（`error: ...`），2 用法错误。Ctrl-C 会通过取消回调安全停止。

实测示例（12 个三角形的立方体，2026-10-21）：

```text
$ stl2stp-cli info cube.stl
load_ms=4.87923
mesh vertices=8 edges=18 faces=12
bbox min=0,0,0 max=10,10,10

$ stl2stp-cli convert cube.stl cube.step --author "致知屋" --timestamp 2026-10-21T10:30:00+08:00
load_ms=2.80763
mesh vertices=8 edges=18 faces=12
bbox min=0,0,0 max=10,10,10
mode=original
（OCCT 往 stdout 打印的彩色 "Statistics on Transfer (Write)" 统计，见已知问题）
export_ms=43.9643
export solids=1 shells=1 faces=12 edges=18 vertices=8
readback_ms=14.1615
readback solids=1 shells=1 faces=12 edges=18 vertices=8
output_bytes=24350
# 文件头：FILE_NAME('cube.step','2026-10-21T02:30:00Z',('\X2\81F477E55C4B\X0\'),(...

$ stl2stp-cli convert cube.stl cube.igs --mode sewing --author "致知屋"
warning: IGES metadata must be ASCII: non-ASCII characters were replaced with '?'
...
mode=sewing
export solids=1 shells=1 faces=12 edges=18 vertices=8
output_bytes=20979

$ stl2stp-cli simplify cube.stl s.stl --faces 6
weld_ms=0.005392
welded_vertices=0
simplify_ms=0.04478
mesh vertices=5 edges=9 faces=6
output_bytes=384

$ stl2stp-cli convert nope.stl x.step ; echo $?
error: Cannot open STL input
1
```

`convert` 导出 STEP 后会自动回读一次（`readback_ms`），这就是 App 里的“导出后校验”。

## 3. App 测试

### 3.1 静态检查

```bash
cd $R/app
flutter pub get
flutter analyze          # 期望：No issues found!
```

### 3.2 单元 + 组件测试（55 个）

```bash
cd $R/app
TMPDIR=$R/build/tmp flutter test       # 期望最后一行：All tests passed!（+55）
flutter test test/features_test.dart   # 只跑一个文件
flutter test --name "memory guard"     # 按名字过滤
```

这些测试不需要核心库：转换用 `DemoEngine` 或测试里的假引擎，购买用模拟商店，偏好用 `SharedPreferences.setMockInitialValues`。

### 3.3 集成测试（真实核心库）

前提：已按 §1.3 编好 `build/core-shared/libstl2stp-core.so`，并且有 WSLg 显示。

```bash
cd $R/app
TMPDIR=$R/build/tmp XDG_DATA_HOME=$R/build/xdg-data \
  flutter test integration_test/app_test.dart -d linux --dart-define=SHOT_DIR=$R/build/app-shots
```

- 期望：`All tests passed!`（3 个），`build/app-shots/` 下有 27 张 PNG 和 `results.txt`（`STEP_OK`、`ESTIMATE_CHECK` 等结果行）。
- 从 PowerShell 远程启动时要用交互式 bash 才有显示环境：`wsl -d Ubuntu-20.04 -e bash -ic "<上面的命令>"`。
- 测试会真正调用核心库：支架 → STEP、球 → IGES、焊接开/关对比、内存警告、分栏布局等，见 §10。

## 4. Linux 发布包

```bash
cd $R/app
flutter build linux --release
ls build/linux/x64/release/bundle
#   stl2stp_app  data/  lib/
du -sh build/linux/x64/release/bundle   # 约 43M
ls -l build/linux/x64/release/bundle/lib
#   libapp.so（约 6.98 MB，Dart AOT）  libflutter_linux_gtk.so（约 33.9 MB）
#   libstl2stp-core.so（342,584 B）  libdesktop_drop_plugin.so  liburl_launcher_linux_plugin.so
```

注意：bundle 里的 `libstl2stp-core.so` 通过 RPATH 找 `~/occt-install/lib`，所以这个 bundle **只能在装有同路径 OCCT 的机器上运行**。Linux 版只是开发预览，不是发布目标；要分发 Linux 版需要把 OCCT 的 `.so` 一起放进 `lib/` 并把 RPATH 改成 `$ORIGIN`（未做）。

## 5. 运行 App、演示模式、环境变量

```bash
cd $R/app
build/linux/x64/release/bundle/stl2stp_app                    # 打开窗口（430×900，模拟手机）
build/linux/x64/release/bundle/stl2stp_app /mnt/d/a.stl       # 直接打开一个 STL（跳过引导页）
STL2STP_DEMO=1 build/linux/x64/release/bundle/stl2stp_app     # 演示模式
flutter run -d linux                                          # 调试运行（热重载）
```

| 环境变量 | 作用 |
|---|---|
| `STL2STP_DEMO=1` | 强制演示模式：不加载核心库，转换只生成占位文件，首页显示黄色横幅“演示模式……” |
| `STL2STP_CORE_LIB=/path/libstl2stp-core.so` | 运行时：指定核心库位置（优先于 bundle 里的）；构建时：指定要打包的核心库 |
| `STL2STP_MOCK_STORE=1` | 在 iOS/macOS/Android 上也强制使用模拟商店（如没有 StoreKit 配置的模拟器） |
| `XDG_DATA_HOME` | Linux 上应用数据目录的根（输出在 `$XDG_DATA_HOME/com.stl2stp.stl2stp_app/Outputs/`） |

找不到核心库时自动退回演示模式（不会崩溃）。WSLg 下系统文件选择器可能不可用，这时会弹出输入框让你粘贴路径；首页还有“用示例模型试试”按钮（内置支架和球）。

## 6. 怎样证明两台机器的构建等价

1. **工具链版本**：对照 [environment.md §1](environment.md#1-总览要装什么占多大要多久) 逐项 `--version`。
2. **共享库逐字节**：同一提交、同一工具链、OCCT 装在同一路径时，`libstl2stp-core.so` 的 sha256 应为 `3cc9d486…542aedc`。开发机上实测：在新目录重编，`sha256sum` 与 `cmp` 均相同 → 构建是确定性的。路径不同（用户名不同）时 RPATH 字符串不同，sha256 一定不同，这是正常的。
3. **输出逐字节**（推荐，与路径无关）：用同一个 STL 和**固定的时间戳**转换，两边结果应完全相同：

```bash
CLI=$R/build/rel/stl2stp-cli
D=$R/build/rel/tests/data
$CLI convert $D/cube-binary.stl /tmp/a.step --timestamp 2026-10-21T10:30:00+08:00
$CLI convert $D/cube-binary.stl /tmp/a.igs  --timestamp 2026-10-21T10:30:00+08:00
sha256sum /tmp/a.step /tmp/a.igs     # 在两台机器上比较
```

   注意：STEP 文件头里有文件名（basename），所以两边输出文件名要一样；不加 `--timestamp` 时每次写入当前时间，必然不同。
4. **测试数**：CTest 122/122、`flutter test` 55、集成测试 3。

## 7. 工具脚本：标定、文案、cadcheck、sanitizer

### 7.1 估算与内存标定（`app/tool/calib/`）

```bash
cd $R/app/tool/calib
python3 calib.py  $R/build/rel/stl2stp-cli $R/build/tmp/calib       # 约 15 分钟，逐个转换 5k–100k 的球和方块 → calib.csv
python3 calib2.py $R/build/rel/stl2stp-cli $R/build/tmp/calib2      # 减面组合，约 2 分钟 → calib2.csv
python3 memtime.py  $R/build/rel/stl2stp-cli $R/build/tmp/mem  $R/app/tool/calib/calib.py   # 每种配置的最大 RSS → memtime.csv（内存模型）
python3 weldtime.py $R/build/rel/stl2stp-cli $R/build/tmp/weld $R/app/tool/calib/calib.py   # 焊接开/关耗时 → weldtime.csv
```

第三个参数是 `calib.py` 的路径（memtime/weldtime 复用其中的网格生成函数）；`calib.py` 可加第三个参数只跑部分网格（逗号分隔的网格名）。说明见 [calib/README.md](../../app/tool/calib/README.md)；怎样把结果变成代码里的系数见 [recipes.md R4](recipes.md#r4-重新标定估算和内存系数)。网格和输出在每次运行后自动删除。

### 7.2 文案工具

```bash
cd $R/app
python3 tool/app_strings.py    # 由 tool/app_strings.py 里的表生成 assets/i18n/app_zh.json、app_en.json
bash tool/sync_strings.sh      # 把 design/ui/strings_{zh,en}.json 拷到 assets/i18n/{zh,en}.json，打印 "synced 200 keys"
flutter test test/core_test.dart --name strings   # 5 个文案一致性测试
```

### 7.3 外部 CAD 校验（当前 master 的 `core/tools/cadcheck/`）

见 [validation.md §3](validation.md#3-怎样重做外部校验)。

### 7.4 ASan / UBSan

```bash
cd $R && bash core/tools/run-sanitizer-ctest.sh     # 用法和参数见脚本开头
```

脚本内置 `setarch -R` 和日志大小上限：gcc-10 的 ASan 在 WSL2 6.x 内核上偶尔会无限输出 `AddressSanitizer:DEADLYSIGNAL`（曾经写出 31 GB 日志）。LeakSanitizer 默认关闭（OCCT 8.0.1 每次传输在退出时留下约 7.6 MB，非本项目代码）。详见 [CHANGES-from-original.md §9.6](../CHANGES-from-original.md)。

## 8. 截图的重新生成

参考截图在 [screenshots/](screenshots/)（27 张，约 4.2 MB），由集成测试生成：

```bash
cd $R/app
TMPDIR=$R/build/tmp XDG_DATA_HOME=$R/build/xdg-data \
  flutter test integration_test/app_test.dart -d linux --dart-define=SHOT_DIR=$R/build/app-shots
cp $R/build/app-shots/*.png $R/docs/handoff/screenshots/
```

截图方式：`lib/core/screenshot.dart` 的 `captureWidget` 把根部 `RepaintBoundary`（`screenshotKey`）渲染成 PNG（像素比 2），不依赖操作系统截图工具，所以在没有桌面的环境里也一致。新增截图：在 `app_test.dart` 里需要的位置调用 `await shot(tester, 'NN_name');`。

## 9. 测试目录：CTest 122 个

> 注册在 `core/tests/CMakeLists.txt`；实现分别在 `test_core.cpp`（mesh_*/导出/模式）、`test_metadata.cpp`（metadata_*、cli_*_read）、`test_review.cpp`（review_*）、`test_c_api.c`；CLI 测试直接调用 `stl2stp-cli` 并用正则或 `WILL_FAIL` 判断。下表的“检查内容”是概括，精确断言以源码为准。

| # | 测试名（ctest 里前缀 `stl2stp.`） | 检查内容 |
|---|---|---|
| 1 | `c_header` | C99 编译头文件：默认选项（ORIGINAL、merge 关、元数据全空）、枚举 4 字节、空 handle 取统计返回 EMPTY 且有 last_error |
| 2 | `fixtures` | 测试夹具：生成全部测试用 STL（立方体、两个壳、退化、截断、共线、球、大 ASCII 等）到 build/.../tests/data，其它测试依赖它 |
| 3 | `load_binary` | 读二进制立方体 STL：8 顶点 / 18 边 / 12 面 |
| 4 | `load_ascii` | 读 ASCII 立方体 STL |
| 5 | `solid_header` | 以 "solid" 开头的二进制 STL 不被误判为 ASCII |
| 6 | `truncated` | 截断文件：读出完整的部分，返回 WARNING_PARTIAL |
| 7 | `stats` | 网格统计（顶点、边、面、包围盒） |
| 8 | `errors` | 各种错误：空路径、不存在、垃圾内容、空 handle → 对应错误码 |
| 9 | `simplify` | QEM 减面到目标面数，结果有效 |
| 10 | `save` | 保存 STL（原子写出，失败不留半个文件） |
| 11 | `unicode` | 中文/Unicode 路径读写 |
| 12 | `cancel_mesh` | 读入与减面过程中取消 → ERROR_CANCELLED，原网格不变 |
| 13 | `metadata_default_original_step` | 默认：文本元数据为空、时间戳为当前 UTC（…Z）（original / STEP） |
| 14 | `metadata_default_original_iges` | 默认：文本元数据为空、时间戳为当前 UTC（…Z）（original / IGES） |
| 15 | `metadata_default_sewing_step` | 默认：文本元数据为空、时间戳为当前 UTC（…Z）（sewing / STEP） |
| 16 | `metadata_default_sewing_iges` | 默认：文本元数据为空、时间戳为当前 UTC（…Z）（sewing / IGES） |
| 17 | `metadata_custom_original_step` | 自定义作者/单位/产品/描述写入并能读回（original / STEP） |
| 18 | `metadata_custom_original_iges` | 自定义作者/单位/产品/描述写入并能读回（original / IGES） |
| 19 | `metadata_custom_sewing_step` | 自定义作者/单位/产品/描述写入并能读回（sewing / STEP） |
| 20 | `metadata_custom_sewing_iges` | 自定义作者/单位/产品/描述写入并能读回（sewing / IGES） |
| 21 | `metadata_isolation_original_step` | 两次导出之间设置互不影响（全局参数被恢复）（original / STEP） |
| 22 | `metadata_isolation_original_iges` | 两次导出之间设置互不影响（全局参数被恢复）（original / IGES） |
| 23 | `metadata_isolation_sewing_step` | 两次导出之间设置互不影响（全局参数被恢复）（sewing / STEP） |
| 24 | `metadata_isolation_sewing_iges` | 两次导出之间设置互不影响（全局参数被恢复）（sewing / IGES） |
| 25 | `metadata_copy_original_step` | 库在开始时复制字符串，回调里修改调用方缓冲区不影响输出（original / STEP） |
| 26 | `metadata_copy_original_iges` | 库在开始时复制字符串，回调里修改调用方缓冲区不影响输出（original / IGES） |
| 27 | `metadata_copy_sewing_step` | 库在开始时复制字符串，回调里修改调用方缓冲区不影响输出（sewing / STEP） |
| 28 | `metadata_copy_sewing_iges` | 库在开始时复制字符串，回调里修改调用方缓冲区不影响输出（sewing / IGES） |
| 29 | `metadata_cancel_original_step` | 导出中取消：不产生目标文件，全局参数被恢复（original / STEP） |
| 30 | `metadata_cancel_original_iges` | 导出中取消：不产生目标文件，全局参数被恢复（original / IGES） |
| 31 | `metadata_cancel_sewing_step` | 导出中取消：不产生目标文件，全局参数被恢复（sewing / STEP） |
| 32 | `metadata_cancel_sewing_iges` | 导出中取消：不产生目标文件，全局参数被恢复（sewing / IGES） |
| 33 | `metadata_timestamp_original_step` | 时间戳覆盖值的校验与 UTC 换算（闰年、跨日、小数秒）（original / STEP） |
| 34 | `metadata_timestamp_original_iges` | 时间戳覆盖值的校验与 UTC 换算（闰年、跨日、小数秒）（original / IGES） |
| 35 | `metadata_timestamp_sewing_step` | 时间戳覆盖值的校验与 UTC 换算（闰年、跨日、小数秒）（sewing / STEP） |
| 36 | `metadata_timestamp_sewing_iges` | 时间戳覆盖值的校验与 UTC 换算（闰年、跨日、小数秒）（sewing / IGES） |
| 37 | `metadata_encoding_original_step` | 中文/特殊字符：STEP 用 \X2\ 无损；IGES 替换为 ? 并返回 METADATA_CHANGED（original / STEP） |
| 38 | `metadata_encoding_original_iges` | 中文/特殊字符：STEP 用 \X2\ 无损；IGES 替换为 ? 并返回 METADATA_CHANGED（original / IGES） |
| 39 | `metadata_encoding_sewing_step` | 中文/特殊字符：STEP 用 \X2\ 无损；IGES 替换为 ? 并返回 METADATA_CHANGED（sewing / STEP） |
| 40 | `metadata_encoding_sewing_iges` | 中文/特殊字符：STEP 用 \X2\ 无损；IGES 替换为 ? 并返回 METADATA_CHANGED（sewing / IGES） |
| 41 | `metadata_parallel_step` | 多个 handle 并行导出 STEP：全局 exchange 锁保证元数据不串 |
| 42 | `metadata_parallel_iges` | 多个 handle 并行导出 IGES：全局 exchange 锁保证元数据不串 |
| 43 | `metadata_iges_limits` | IGES 字段 195 字节上限按替换后的 ASCII 计算 |
| 44 | `step` | 立方体导出 STEP：1 solid / 12 面，拓扑正确 |
| 45 | `step_merge` | 开共面合并：立方体合并为 6 面 |
| 46 | `readback` | STEP 回读拓扑与导出一致（新 handle） |
| 47 | `two_shells` | 两个分离的立方体：默认得到 2 个 solid |
| 48 | `iges` | 导出 IGES |
| 49 | `exchange_errors` | 导出参数错误（非法模式、公差、空路径等）→ ERROR_ARGUMENT/IO |
| 50 | `cancel_exchange` | OCCT 传输过程中取消 → ERROR_CANCELLED，不留文件 |
| 51 | `original_cube` | ORIGINAL 模式：立方体，核对导出与回读拓扑、状态码 |
| 52 | `original_two_shells` | ORIGINAL 模式：两个壳，核对导出与回读拓扑、状态码 |
| 53 | `original_degenerate_duplicate` | ORIGINAL 模式：含退化/重复三角形（结果不封闭 → NOT_SOLID），核对导出与回读拓扑、状态码 |
| 54 | `original_duplicate_open` | ORIGINAL 模式：零面积重复面（ORIGINAL → REPAIRED），核对导出与回读拓扑、状态码 |
| 55 | `original_collinear` | ORIGINAL 模式：含共线三角形（ORIGINAL → REPAIRED），核对导出与回读拓扑、状态码 |
| 56 | `original_collinear_only` | ORIGINAL 模式：只有共线三角形（无可用面 → 错误），核对导出与回读拓扑、状态码 |
| 57 | `original_truncated` | ORIGINAL 模式：截断文件（PARTIAL 读入，导出 NOT_SOLID，回读 0 solid），核对导出与回读拓扑、状态码 |
| 58 | `original_iges` | ORIGINAL 模式：IGES 导出，核对导出与回读拓扑、状态码 |
| 59 | `sewing_cube` | SEWING 模式：立方体，核对导出与回读拓扑、状态码 |
| 60 | `sewing_two_shells` | SEWING 模式：两个壳，核对导出与回读拓扑、状态码 |
| 61 | `sewing_degenerate_duplicate` | SEWING 模式：含退化/重复三角形（结果不封闭 → NOT_SOLID），核对导出与回读拓扑、状态码 |
| 62 | `sewing_duplicate_open` | SEWING 模式：零面积重复面（ORIGINAL → REPAIRED），核对导出与回读拓扑、状态码 |
| 63 | `sewing_collinear` | SEWING 模式：含共线三角形（ORIGINAL → REPAIRED），核对导出与回读拓扑、状态码 |
| 64 | `sewing_collinear_only` | SEWING 模式：只有共线三角形（无可用面 → 错误），核对导出与回读拓扑、状态码 |
| 65 | `sewing_truncated` | SEWING 模式：截断文件（PARTIAL 读入，导出 NOT_SOLID，回读 0 solid），核对导出与回读拓扑、状态码 |
| 66 | `sewing_iges` | SEWING 模式：IGES 导出，核对导出与回读拓扑、状态码 |
| 67 | `cli_info` | CLI info：输出须含 "vertices=8 edges=18 faces=12" |
| 68 | `cli_simplify` | CLI simplify 球 → 2000 面，输出含 output_bytes= |
| 69 | `cli_invalid` | CLI simplify --faces -1 必须失败 |
| 70 | `cli_metadata_original_step` | CLI 带中文元数据转换（original/STEP） |
| 71 | `cli_metadata_original_step_read` | 读回上一个文件核对元数据（original/STEP） |
| 72 | `cli_metadata_original_iges` | CLI 带中文元数据转换（original/IGES） |
| 73 | `cli_metadata_original_iges_read` | 读回上一个文件核对元数据（original/IGES） |
| 74 | `cli_metadata_sewing_step` | CLI 带中文元数据转换（sewing/STEP） |
| 75 | `cli_metadata_sewing_step_read` | 读回上一个文件核对元数据（sewing/STEP） |
| 76 | `cli_metadata_sewing_iges` | CLI 带中文元数据转换（sewing/IGES） |
| 77 | `cli_metadata_sewing_iges_read` | 读回上一个文件核对元数据（sewing/IGES） |
| 78 | `cli_timestamp_original_step` | CLI --timestamp 2024-02-29T18:52:00.123+08:00（original/STEP） |
| 79 | `cli_timestamp_original_step_read` | 读回核对时间戳已换算为 UTC（original/STEP） |
| 80 | `cli_timestamp_original_iges` | CLI --timestamp 2024-02-29T18:52:00.123+08:00（original/IGES） |
| 81 | `cli_timestamp_original_iges_read` | 读回核对时间戳已换算为 UTC（original/IGES） |
| 82 | `cli_timestamp_sewing_step` | CLI --timestamp 2024-02-29T18:52:00.123+08:00（sewing/STEP） |
| 83 | `cli_timestamp_sewing_step_read` | 读回核对时间戳已换算为 UTC（sewing/STEP） |
| 84 | `cli_timestamp_sewing_iges` | CLI --timestamp 2024-02-29T18:52:00.123+08:00（sewing/IGES） |
| 85 | `cli_timestamp_sewing_iges_read` | 读回核对时间戳已换算为 UTC（sewing/IGES） |
| 86 | `cli_timestamp_invalid` | 非法日期 2026-02-29 必须失败 |
| 87 | `cli_metadata_author_missing` | --author 缺值必须失败 |
| 88 | `cli_metadata_author_duplicate` | --author 重复必须失败 |
| 89 | `cli_metadata_org_missing` | --org 缺值必须失败 |
| 90 | `cli_metadata_org_duplicate` | --org 重复必须失败 |
| 91 | `cli_metadata_product_missing` | --product 缺值必须失败 |
| 92 | `cli_metadata_product_duplicate` | --product 重复必须失败 |
| 93 | `cli_metadata_description_missing` | --description 缺值必须失败 |
| 94 | `cli_metadata_description_duplicate` | --description 重复必须失败 |
| 95 | `cli_metadata_timestamp_missing` | --timestamp 缺值必须失败 |
| 96 | `cli_metadata_timestamp_duplicate` | --timestamp 重复必须失败 |
| 97 | `cli_convert` | CLI convert --merge-coplanar，输出含 mode=original |
| 98 | `cli_readback` | CLI readback 上一个文件，输出含 faces=6 |
| 99 | `cli_iges` | CLI convert 到 IGES 并 --simplify 8 |
| 100 | `cli_mode_original` | CLI --mode original（两个壳） |
| 101 | `cli_mode_sewing` | CLI --mode sewing（两个壳） |
| 102 | `cli_mode_invalid` | --mode invalid 必须失败 |
| 103 | `cli_mode_missing` | --mode 缺值必须失败 |
| 104 | `cli_mode_duplicate` | --mode 重复必须失败 |
| 105 | `review_original_closed_cube` | 闭合立方体 → 1 个有效 solid，OK |
| 106 | `review_original_open_box` | 开口盒 → 开放 shell，NOT_SOLID |
| 107 | `review_original_flipped_triangle` | 一个三角形朝向反了 → 自动统一朝向，REPAIRED |
| 108 | `review_original_inverted` | 整体朝内 → 翻转，体积为正，REPAIRED |
| 109 | `review_original_non_manifold_edge` | 非流形边 → 自动改用 SEWING，结果有效 |
| 110 | `review_original_collinear_sliver` | 共线细长三角形 → 丢弃，REPAIRED |
| 111 | `review_original_disconnected` | 两个分离的体 → 2 个 solid，体积 2 |
| 112 | `review_original_strict_legacy` | strict_original=1 保持原版旧行为（不校验不修复） |
| 113 | `review_iges_non_ascii` | IGES 非 ASCII 元数据 → ?，METADATA_CHANGED |
| 114 | `review_qem_open_boundary` | QEM 保持开放边界不收缩 |
| 115 | `review_qem_link_condition` | QEM link condition：不产生非流形/翻面 |
| 116 | `review_qem_closed_sphere` | 闭合 UV 球减到 50%/25% 仍闭合、1 个 solid（含合并共面） |
| 117 | `review_qem_near_coincident_pole` | 极点有 1e-15 级缝隙的球：精确模式报告不封闭，SEWING 能缝合 |
| 118 | `review_weld_open_pole` | 开放极点球：焊接开 → 63 个顶点被焊、1 solid、REPAIRED；关 → NOT_SOLID；strict → OK；简化后仍闭合 |
| 119 | `review_weld_guards` | 焊接防护：薄片不被压扁、细长三角形不退化、极小覆盖值不焊、NaN → ERROR_ARGUMENT |
| 120 | `review_qem_scale_invariance` | QEM 结果与模型缩放无关 |
| 121 | `review_merge_rotated` | 斜放（旋转）立方体共面合并：两种模式都得到 6 面有效实体 |
| 122 | `review_malformed_fuzz` | 随机/畸形输入不崩溃，返回错误码 |

## 10. 测试目录：Flutter 55 个 + 集成测试 3 个

### 10.1 单元 + 组件测试（`app/test/`）

| # | 文件 | 分组 | 类型 | 测试名（原文） |
|---|---|---|---|---|
| 1 | core_test.dart | presets | 单元 | defaults are the recommended preset (original mode, verify on) |
| 2 | core_test.dart | presets | 单元 | each preset round-trips and edits become custom |
| 3 | core_test.dart | presets | 单元 | withPreset keeps format and metadata |
| 4 | core_test.dart | presets | 单元 | verify only applies to STEP; IGES strips non-ASCII |
| 5 | core_test.dart | presets | 单元 | json round trip |
| 6 | core_test.dart | presets | 单元 | target faces |
| 7 | core_test.dart | progress | 单元 | stage weights combine to 0..1 |
| 8 | core_test.dart | stl reader | 单元 | binary sample bracket is closed and sized |
| 9 | core_test.dart | stl reader | 单元 | sample sphere is watertight (poles are single vertices, not sin(pi) jitter) |
| 10 | core_test.dart | stl reader | 单元 | ascii parse and binary file starting with "solid" |
| 11 | core_test.dart | stl reader | 单元 | garbage is rejected |
| 12 | core_test.dart | stl reader | 单元 | display decimation keeps total count |
| 13 | core_test.dart | format helpers | 单元 | bytes and counts |
| 14 | core_test.dart | format helpers | 单元 | estimate scales with options |
| 15 | core_test.dart | estimate (calibrated) | 单元 | every measurement lies inside the predicted range (device factor 1) |
| 16 | core_test.dart | estimate (calibrated) | 单元 | busy-machine measurements lie inside the range including device spread |
| 17 | core_test.dart | estimate (calibrated) | 单元 | central values are within 35 % (time) / 20 % (size) for large meshes |
| 18 | core_test.dart | estimate (calibrated) | 单元 | device factor scales time but not size |
| 19 | core_test.dart | estimate (calibrated) | 单元 | unknown planarity widens the merge range |
| 20 | core_test.dart | estimate (calibrated) | 单元 | range wording |
| 21 | core_test.dart | plane counter | 单元 | bracket has 8 planes, sphere ≈ quads + pole triangles |
| 22 | core_test.dart | strings | 单元 | zh and en have the same 200 keys and placeholders |
| 23 | core_test.dart | strings | 单元 | app-only strings: same keys/placeholders, no clash with design keys |
| 24 | core_test.dart | strings | 单元 | assets match design/ui string tables |
| 25 | core_test.dart | strings | 单元 | every literal key used in lib/ exists |
| 26 | core_test.dart | strings | 单元 | placeholder substitution |
| 27 | widget_flow_test.dart | — | 组件 | onboarding then empty home |
| 28 | widget_flow_test.dart | — | 组件 | preview → convert (demo) → done → history |
| 29 | widget_flow_test.dart | — | 组件 | cancel asks for confirmation and shows cancelled screen |
| 30 | widget_flow_test.dart | — | 组件 | engine failure maps to error screen with code |
| 31 | widget_flow_test.dart | — | 组件 | English locale |
| 32 | widget_flow_test.dart | — | 组件 | help page from home, zh and en, all topics present |
| 33 | widget_flow_test.dart | — | 组件 | help opens at a topic from the preview "?" button; estimate shows a range |
| 34 | widget_flow_test.dart | — | 单元 | drop target picks the first .stl |
| 35 | purchase_test.dart | free tier | 单元 | default limits are none and never block |
| 36 | purchase_test.dart | free tier | 单元 | each limit triggers |
| 37 | purchase_test.dart | mock purchase service | 单元 | buy, persist, reset, restore |
| 38 | purchase_test.dart | mock purchase service | 单元 | simulated failures do not unlock |
| 39 | purchase_test.dart | mock purchase service | 单元 | AppState: Pro bypasses limits; daily counter |
| 40 | purchase_test.dart | paywall | 组件 | placeholder price when the store reports none, then buy |
| 41 | purchase_test.dart | paywall | 组件 | price label comes from product info |
| 42 | features_test.dart | weld option | 单元 | default on, json round trip, off makes the preset custom |
| 43 | features_test.dart | weld option | 组件 | advanced settings toggle and help topic |
| 44 | features_test.dart | licences | 组件 | OCCT LGPL + exception text and the package licence page |
| 45 | features_test.dart | split layout (iPad) | 单元 | threshold |
| 46 | features_test.dart | split layout (iPad) | 组件 | wide: history left, preview opens in the right pane |
| 47 | features_test.dart | split layout (iPad) | 组件 | phone: unchanged single column, preview is a pushed page |
| 48 | features_test.dart | history swipe | 组件 | swipe right shares, missing file explains, swipe left deletes |
| 49 | features_test.dart | memory guard | 单元 | peak model and decisions |
| 50 | features_test.dart | memory guard | 单元 | budget detection: override, /proc/meminfo parsing |
| 51 | features_test.dart | memory guard | 组件 | warning dialog offers to simplify before converting |
| 52 | features_test.dart | memory guard | 组件 | refuses when even the conversion alone does not fit |
| 53 | features_test.dart | running conversion | 组件 | memory pressure cancels cleanly with a clear message |
| 54 | features_test.dart | running conversion | 组件 | background task begins/ends around a conversion; expiry stops it |
| 55 | features_test.dart | running conversion | 单元 | background and haptics are no-ops on desktop |

辅助：`test/` 里的测试用 `SharedPreferences.setMockInitialValues`、假引擎（可控制成功/失败/取消/内存紧张）、`MockPurchaseService`；组件测试用 `tester.view.physicalSize` 模拟手机或 iPad 宽度（注意 800×600 会被当成平板，手机测试用窄尺寸）。

### 10.2 集成测试（`app/integration_test/app_test.dart`，真实核心库）

| # | 测试名 | 内容 | 产出截图 |
|---|---|---|---|
| 1 | real FFI: bracket → STEP via UI, sphere → IGES | 首页 → 示例支架 → 预览/高级设置 → 转 STEP（核对 solids、校验通过、文件存在）→ 历史 → 球 → IGES → 转换中 → 完成；英文首页；取消流程 | 01–10 |
| 2 | help, smaller-preset estimate range, settings, paywall (mock store) | “文件更小”预设的估算范围、帮助页（中/英、FAQ）、设置（免费版）、购买页、模拟购买后设置显示完整版 | 11–17 |
| 3 | real FFI weld on/off, help, licences, memory warning, split layout, history swipe | 开放极点球在焊接开/关下的结果对比、焊接帮助、开源许可三页、内存警告对话框、历史右滑分享、宽屏分栏 | 18–27 |

## 11. 怎样新增测试

- **核心库**：
  1. 网格/导出类：在 `test_core.cpp` 的分派里加 `else if (mode == "my_case") {...}`，在 `core/tests/CMakeLists.txt` 的 `_mesh_tests` 或 `_exchange_tests` 列表里加名字。
  2. 审查/回归类（推荐）：在 `test_review.cpp` 里加 `else if (which == "my_case")`，在 CMakeLists 的 review `foreach(_case ...)` 列表里加名字。用文件里现成的 `cube()`、`uv_sphere()`、`plane()` 等网格生成函数和 `require(cond, msg)`。
  3. CLI 类：在 CMakeLists 里 `add_test(NAME stl2stp.cli_xxx COMMAND stl2stp-cli ...)`，用 `PASS_REGULAR_EXPRESSION` 或 `WILL_FAIL`，并加 `FIXTURES_REQUIRED stl2stp_data`。
  4. 重新 `cmake --build build/rel && ctest --test-dir build/rel -j4`，确认总数 +1 且全部通过；更新本页 §9 和 [core-validation.md](../core-validation.md)。
- **App**：
  1. 纯逻辑放 `test/core_test.dart`（或新文件）；界面流程用 `testWidgets`，参考 `widget_flow_test.dart` 里的 `makeState(engine:, prefs:)` 辅助函数。
  2. 需要真实核心库的放 `integration_test/app_test.dart`。
  3. 新增文案键后，`core_test.dart` 的 strings 组会自动检查 zh/en 键与占位符一致、代码里引用的字面键都存在。
  4. 更新本页 §10 的数量。
