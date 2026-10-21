# 相对原版 stl2stp 的全部改动与复现指南

> 读者：要从零完整复现 `core/extract` 分支成果的同事。
> 编写日期：2026-10-20（UTC+8）。依据：`git log`/`git diff main..core/extract`、仓库文档
> （`docs/core-api.md`、`docs/build-linux.md`、`docs/core-validation.md`、`docs/original-conversion.md`）、
> `core/` 源码，以及只读原版源码 `reference/stl2stp-master/`。
> 第 5 节的测试结果是 2026-10-20 21:13–21:14（UTC+8）在 HEAD `65e30ad` 上**实际重跑**得到的。
> 文中凡标注 **【未验证】** 的内容，表示本次没有办法或没有做实测，请勿当作结论。

---

## 1. 概览

### 1.1 原版是什么

- 来源：gitee `zigzagwag/stl2stp`。这是一个 Windows MFC + OpenCASCADE（OCCT）桌面程序，
  把 STL 网格转换为可编辑的 STEP / IGES（B-Rep，每个三角形一个平面 face）。
- 关键源文件（只读，放在 `reference/stl2stp-master/stl2stp-code/stl2stp-code/`，**不入库**）：
  - `stl2stp-codeDoc.cpp`（2303 行，UTF-8 with BOM / CRLF）：STL 读取（`LoadStlAndDisplay`、
    `ReadStlTolerant`）、B-Rep 重建（`RebuildShapeFromMesh`，第 1611–1783 行）、
    `OnExportStep`（第 1924 行起）、`OnExportIges`（第 1796 行起）、大于 100000 面的确认框。
  - `MeshSimplify.cpp` / `MeshSimplify.h`（401 行）：QEM 边折叠简化，参数是保留比例 `theKeepRatio`。
  - 其余是 MFC 文档/视图、渲染、注册表、对话框等 UI 代码。
- reference 自带 OCCT 开发头文件（`occt-build/inc`，8.1.0-dev）和 Windows `vc14` 的 `.lib`，
  我们**没有使用**，改为在 Linux 上从官方 8.0.1 源码编译。

### 1.2 我们抽取了什么，为什么

iOS App 第一阶段的目标，是做出一个**不依赖 MFC / Windows 的跨平台 C++ 核心库**
`stl2stp-core`。以后 iOS 端只需要通过 Objective-C++ 桥接调用一个纯 C 头文件。
目前没有 Mac，所以先在 WSL Ubuntu 20.04 上开发和验证核心库。

抽取或重写的能力：

| 能力 | 原版位置 | 新位置 |
| --- | --- | --- |
| STL 读取（binary/ASCII、截断容错） | `Doc.cpp` `LoadStlAndDisplay` / `ReadStlTolerant` | `core/src/mesh.cpp` |
| 网格统计 | UI 显示 | `stl2stp_get_mesh_stats` |
| QEM 简化 | `MeshSimplify.cpp` | `core/src/simplify.cpp` |
| 保存 STL | 原版菜单 | `stl2stp_save_stl`（binary） |
| B-Rep 重建（原版算法） | `RebuildShapeFromMesh` | `exchange.cpp` `make_original_shape`（ORIGINAL 模式） |
| B-Rep 重建（缝合） | 原版**没有** | `exchange.cpp` `make_sewing_shape`（SEWING 模式，新增） |
| 共面合并 | 原版**没有** | `merge_shape`（`ShapeUpgrade_UnifySameDomain`，新增可选项） |
| STEP / IGES 写出 | `OnExportStep` / `OnExportIges` | `stl2stp_export_step` / `stl2stp_export_iges` |
| STEP 回读统计 | 原版无独立接口 | `stl2stp_read_step` |
| 命令行 | 无 | `core/cli/main.cpp` → `stl2stp-cli` |

**没有迁移**：MFC UI、渲染与显示降采样、注册表、撤销、交互式确认框（包括 100000 面提示）、
原版硬编码的作者/产品等文件头文案。

### 1.3 仓库结构（`D:\stl2stp-ios`，在 WSL 中是 `/mnt/d/stl2stp-ios`）

```text
.gitattributes / .gitignore / README.md / AGENTS.md
core/
  CMakeLists.txt                 # 库 + CLI + 测试；OCCT_ROOT 指定 OCCT 前缀
  include/stl2stp/stl2stp.h      # 唯一公共头文件（纯 C，C99 可编译）
  src/internal.hpp               # Mesh / Progress / OutputFile / Error 等内部类型
  src/mesh.cpp                   # STL 读写、统计、默认选项、句柄生命周期
  src/simplify.cpp               # QEM 简化
  src/exchange.cpp               # ORIGINAL/SEWING 构形、共面合并、STEP/IGES 写出、STEP 回读、metadata
  src/exchange_disabled.cpp      # STL2STP_WITH_OCCT=OFF 时的桩（导出返回 STL2STP_ERROR_OCCT）
  cli/main.cpp                   # stl2stp-cli
  tests/CMakeLists.txt           # 104 个 CTest
  tests/fixtures.cpp             # 在构建目录生成测试 STL（cube、two-shells、sphere 等）
  tests/test_core.cpp            # 网格/导出/取消/模式测试
  tests/test_metadata.cpp        # metadata/时间戳/编码测试
  tests/test_c_api.c             # C99 头文件与 ABI 检查
  tests/data/small_zero-area-duplicate.stl   # 唯一入库的手写小 fixture
  tests/data/README.md
docs/
  core-api.md  build-linux.md  core-validation.md  original-conversion.md
  CHANGES-from-original.md       # 本文
ios/.gitkeep                     # iOS 阶段占位
reference/                       # 原版源码，只读，被 .gitignore 忽略
build/                           # 所有构建输出与日志，被 .gitignore 忽略
```

### 1.4 分支与提交

- 只用本地 git：**没有 remote，从未 push**。`main` 只含 `d1a8084`，`core/extract` 在其上依次提交。
- 作者统一为 `致知屋 <zhizhiwu@local>`。时间均为 UTC+8。

| 提交 | 时间 | 内容 |
| --- | --- | --- |
| `d1a8084` | 16:51 | init：项目骨架与协作约定（README、AGENTS、.gitignore、.gitattributes、空目录 `.gitkeep`）。`main` 停在这里 |
| `224958d` | 17:57 | chore：去掉 4 个文本文件的 UTF-8 BOM，统一 LF；`.gitattributes` 增加 `*.png`/`*.jpg binary` |
| `9ec394a` | 17:57 | core：抽取跨平台 stl2stp-core（C API、STL 读写、QEM、STEP/IGES 导出、共面合并、STEP 回读）+ CLI + 25 项 CTest；新增 `docs/core-api.md`、`build-linux.md`、`core-validation.md`。当时唯一的导出算法是缝合（即后来的 SEWING） |
| `7d961fe` | 18:36 | core：新增转换模式开关。`ORIGINAL`（默认，复刻桌面版“共享边 + 单 shell + 单 solid”）/ `SEWING`；CLI `--mode`；新增 `docs/original-conversion.md`；测试 25 → 46 |
| `c0a553a` | 18:52 | core：文件头 metadata 可配置，默认为空（去掉原版硬编码 VINCENT/STL2STP 以及 OCCT 默认的 “Open CASCADE …”）；`Interface_Static` 用 RAII 保存/恢复；CLI `--author/--org/--product/--description`；测试 46 → 85 |
| `6344109` | 19:05 | core：默认写当前 UTC 时间戳，可覆盖；STEP 非 ASCII 按 ISO 10303-21 编码为 `\X2\`/`\X4\`；IGES 日期写参数 18/25；CLI `--timestamp`；测试 85 → 104 |
| `65e30ad` | 19:10 | core：IGES 日期字段（18/25）一律换算为 UTC（处理跨日/跨年/闰年，小数秒截断）；STEP 时间戳原样写入 |
| `4c71bcb` | 10-20 | docs：新增本文件 |
| `bead9b8` | 10-20 | （`core/fixes`）test(review)：把代码审查发现写成会失败的 CTest |
| `7d69df3` | 10-21 | （`core/fixes`）API：追加 NOT_SOLID/REPAIRED/METADATA_CHANGED 与 merge/strict 选项 |
| `333839f` | 10-21 | （`core/fixes`）QEM：边界 quadric、link condition、面积加权、尺度无关检查 |
| `3b7b63d` | 10-21 | （`core/fixes`）导出：ORIGINAL 校验修复、安全的共面合并、流式 STEP 过滤、IGES ASCII |
| `4548f33` | 10-21 | （`core/fixes`）测试：review 用例、关闭 healing 读回、期望更新；测试 104 → 118 |
| `e167caa` | 10-21 | （`core/fixes`）docs：core-api.md |
| （本节提交） | 10-21 | （`core/fixes`）docs + 有界 sanitizer 脚本 `core/tools/run-sanitizer-ctest.sh`；第 9 节 |

`git diff --stat main core/extract`（本文提交之前）：23 个文件，+4519 / −5。

---

## 2. 环境

实测于 Bo 的 Windows 机器上的 WSL `Ubuntu-20.04`（Ubuntu 20.04.6 LTS），用户 `grab`，12 核。

| 工具 | 实测版本（`--version`） | 来源 |
| --- | --- | --- |
| gcc-10 / g++-10 | 10.5.0（`10.5.0-1ubuntu1~20.04`） | 本项目用 `apt-get install --no-upgrade` 新装 |
| 系统默认 g++ | 9.4.0 | 原有，**不能编译 OCCT 8** |
| cmake | 3.26.4（`/usr/local/bin/cmake`） | 机器原有（不是 apt；apt 的 cmake 3.16.3 也装着，但版本较低） |
| ninja | 1.10.0（apt `ninja-build`） | 机器原有 |
| git | 2.25.1 | 机器原有 |
| curl | `/usr/bin/curl` | 机器原有，用来下载 OCCT 源码包 |
| build-essential | 12.8ubuntu1.1 | 机器原有 |

为 g++-10 实际新装的包（据 `/var/log/dpkg.log`，2026-10-20 17:18，共 6 个，没有升级或删除任何包）：
`cpp-10`、`libasan6`、`libgcc-10-dev`、`gcc-10`、`libstdc++-10-dev`、`g++-10`。

在一台干净的 Ubuntu 20.04 上复现时：

```sh
sudo apt-get update
sudo apt-get install -s --no-upgrade gcc-10 g++-10 ninja-build build-essential curl   # 先 -s 模拟，确认不会升级其它包
sudo apt-get install    --no-upgrade gcc-10 g++-10 ninja-build build-essential curl
# CMake：core 最低要求 3.16，apt 的 3.16.3 就能配置 core。
# OCCT 8.0.1 自身要求的最低 CMake 版本本次没有核对【未验证】；我们实际用的是 3.26.4。
# 如需新版 CMake，可从 cmake.org 下载官方二进制，解压到自己的目录，不要覆盖系统包。
```

**为什么必须 g++-10**：用 g++ 9.4 编译 OCCT 8.0.1 时，会在 `Bnd_Box.hxx` 的 constexpr defaulted
noexcept 构造函数处报错（记录于 `docs/build-linux.md`）。core 本身也统一用 gcc-10/g++-10 编译。

> 本机约束（协作要求，复现时同样建议遵守）：不做 `apt upgrade`/`dist-upgrade`，
> 不改 `~/.bashrc`、`/etc`、`ldconfig` 或全局环境变量；OCCT 只装到 `~/occt-install`；
> 构建只放在 `~/stl2stp-work` 或仓库的 `build/` 下。

---

## 3. 从源码编译 OCCT 8.0.1 到 `~/occt-install`

### 3.1 源码

```text
URL:    https://github.com/Open-Cascade-SAS/OCCT/archive/refs/tags/V8.0.1.tar.gz
SHA256: 6297cc55a1720523a437c54d07dc16b9da8c5f5ab5800da3c6ebded568d53c18   （本次重新 sha256sum，确认一致）
```

用的是官方 release tag **V8.0.1**，不是 master 或 8.1-dev。

```sh
mkdir -p ~/stl2stp-work/occt-src && cd ~/stl2stp-work/occt-src
curl -sSL -o V8.0.1.tar.gz https://github.com/Open-Cascade-SAS/OCCT/archive/refs/tags/V8.0.1.tar.gz
sha256sum V8.0.1.tar.gz      # 必须与上面的值一致
tar xzf V8.0.1.tar.gz        # 解压得到 OCCT-8.0.1/
```

### 3.2 配置（取自实际使用的 `build/occt_configure.sh`，并与 CMakeCache 核对一致）

```sh
SRC=~/stl2stp-work/occt-src/OCCT-8.0.1
B=~/stl2stp-work/occt-src/build-linux-release
rm -rf "$B"; mkdir -p "$B"; cd "$B"
cmake -G Ninja "$SRC" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DINSTALL_DIR=$HOME/occt-install \
  -DCMAKE_INSTALL_PREFIX=$HOME/occt-install \
  -DBUILD_LIBRARY_TYPE=Shared \
  -DBUILD_MODULE_FoundationClasses=ON \
  -DBUILD_MODULE_ModelingData=ON \
  -DBUILD_MODULE_ModelingAlgorithms=ON \
  -DBUILD_MODULE_DataExchange=ON \
  -DBUILD_MODULE_ApplicationFramework=OFF \
  -DBUILD_MODULE_Visualization=OFF \
  -DBUILD_MODULE_Draw=OFF \
  -DBUILD_MODULE_DETools=OFF \
  -DBUILD_DOC_Overview=OFF \
  -DBUILD_SAMPLES_QT=OFF \
  -DUSE_FREETYPE=OFF -DUSE_TK=OFF -DUSE_TCL=OFF -DUSE_OPENGL=OFF -DUSE_GLES2=OFF -DUSE_XLIB=OFF \
  -DUSE_FREEIMAGE=OFF -DUSE_RAPIDJSON=OFF -DUSE_DRACO=OFF -DUSE_TBB=OFF -DUSE_VTK=OFF
```

实际 `CMakeCache.txt` 中的关键值（本次只读 grep 得到）：`CMAKE_GENERATOR=Ninja`、`CMAKE_BUILD_TYPE=Release`、
`CMAKE_C_COMPILER=/usr/bin/gcc-10`、`CMAKE_CXX_COMPILER=/usr/bin/g++-10`、`BUILD_CPP_STANDARD=C++17`、
`BUILD_LIBRARY_TYPE=Shared`、`BUILD_MODULE_{FoundationClasses,ModelingData,ModelingAlgorithms,DataExchange}=ON`、
`BUILD_MODULE_{ApplicationFramework,Visualization,Draw,DETools}=OFF`、`USE_{FREETYPE,TCL,XLIB,FREEIMAGE,RAPIDJSON,DRACO,TBB}=OFF`、
`BUILD_RELEASE_DISABLE_EXCEPTIONS=ON`（OCCT 默认值）、`INSTALL_DIR=/home/grab/occt-install`、`INSTALL_DIR_LAYOUT=Unix`、
`BUILD_OCCT_VERSION_EXT=8.0.1`。

### 3.3 编译与安装（取自 `build/occt_build.sh`）

```sh
cd ~/stl2stp-work/occt-src/build-linux-release
ninja -j12  > ~/stl2stp-work/occt-src/build.log   2>&1
ninja install > ~/stl2stp-work/occt-src/install.log 2>&1
```

记录：12 核上 `ninja -j12` 耗时约 **1196 s**，安装体积 **123 MB**（本次 `du -sh ~/occt-install` 复核为 123M）。

### 3.4 安装结果核对

- `~/occt-install/{bin,include,lib,share}`；头文件在 `include/opencascade/`，
  `Standard_Version.hxx` 中 `OCC_VERSION_COMPLETE "8.0.1"`。
- CMake 配置：`lib/cmake/opencascade/OpenCASCADEConfig.cmake`（以及 `...ConfigVersion.cmake`）。
- `lib/` 下共 48 个 `libTK*.so`（每个带 `.so.8.0` / `.so.8.0.1` 软链）。
- 打开 DataExchange 后，即使关掉 Visualization/ApplicationFramework，依赖闭包仍会拉进
  `TKXCAF/TKCAF/TKLCAF/TKCDF/TKV3d/TKService/TKVCAF` 等库。core 不使用这些库的可视化 API。
  没有 `TKOpenGl`、OpenGL、FreeType。`USE_RAPIDJSON=OFF` 时没有 `TKDEGLTF`。

---

## 4. 相对原版的代码改动（按领域）

### 4.1 去除 MFC / Windows 依赖

| 原版 | 新实现 |
| --- | --- |
| `CDocument` 成员函数、`BOOL`、`CString`/`CStringA`、`wchar_t` 阶段文案 | 自由函数 + C++17 STL；状态码改为 `stl2stp_status` |
| `CExportProgressDlg` 的 `SetStage`/`IsCanceled` | `stl2stp_callbacks{progress,cancelled,user_data}`；内部 `Progress` 类 |
| `unsigned __int64`、`0xFFFFFFFFui64` 后缀、`(b<<32)|a` 边键 | `std::pair<size_t,size_t>` 作为 `std::map` 键，`size_t` 索引；边仍按**首次出现顺序**生成 |
| `ToUtf8(CString)` 路径转换 | API 只接受 UTF-8 路径，用 `std::filesystem::u8path` |
| 不检查 `Transfer`/`Write` 返回值，失败也可能显示成功 | 每一步都检查，失败返回负状态码，`stl2stp_last_error` 给出诊断 |
| 直接写目标文件 | 先写同目录下的临时目录，成功后 rename 发布；失败或取消时不改动已有目标文件（`OutputFile` 类） |
| `RebuildLog` 调试日志、显示用 `Poly_Triangulation` 挂载（`bAttachDisplayTri`） | 删除（只用于显示） |
| 编码：GBK/UTF-8 BOM、CRLF 混用 | 全部 UTF-8 无 BOM、LF（见 4.12） |

为兼容 OCCT 8：拓扑计数用 `NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>`，
替代已废弃的 `TopTools_IndexedMapOfShape` 别名；异常信息在 OCCT ≥ 8 用 `Standard_Failure::what()`，
7.x 用 `GetMessageString()`（7.x 分支**未编译验证**【未验证】）。

### 4.2 公共 C API（`core/include/stl2stp/stl2stp.h`）

纯 C 头文件，用 opaque 句柄 `stl2stp_handle*`。所有 API 都不会让 C++ 异常越过 C ABI。

**状态码 `stl2stp_status`**：`STL2STP_OK=0`；`WARNING_PARTIAL=1`（截断等，只载入了可用部分）；
`WARNING_TARGET_NOT_REACHED=2`（简化没有达到目标面数）；`ERROR_ARGUMENT=-1`；`ERROR_IO=-2`；`ERROR_FORMAT=-3`；
`ERROR_EMPTY=-4`；`ERROR_CANCELLED=-5`；`ERROR_OCCT=-6`；`ERROR_MEMORY=-7`；`ERROR_INTERNAL=-8`。
正值表示带警告的成功，负值表示失败。

**结构体**：

```c
typedef void (*stl2stp_progress_callback)(double fraction, void* user_data);
typedef int  (*stl2stp_cancel_callback)(void* user_data);
typedef struct stl2stp_callbacks { stl2stp_progress_callback progress; stl2stp_cancel_callback cancelled; void* user_data; } stl2stp_callbacks;
typedef struct stl2stp_mesh_stats { uint64_t vertices, edges, faces; double minimum[3]; double maximum[3]; } stl2stp_mesh_stats;
typedef struct stl2stp_topology   { uint64_t solids, shells, faces, edges, vertices; } stl2stp_topology;
typedef enum stl2stp_mode
#ifdef __cplusplus
    : int32_t            /* C++ 固定底层类型，避免 UBSan 对非法枚举值报 invalid load */
#endif
{ STL2STP_MODE_ORIGINAL = 0, STL2STP_MODE_SEWING = 1 } stl2stp_mode;

typedef struct stl2stp_export_options {
    double      sewing_tolerance;  /* 默认 1e-6；SEWING 缝合公差，也是共面合并的线性公差；必须是有限正数 */
    int         merge_coplanar;    /* 默认 0 */
    stl2stp_mode mode;             /* 默认 STL2STP_MODE_ORIGINAL       （7d961fe 追加） */
    const char* author;            /* 默认 NULL = 空                   （c0a553a 追加） */
    const char* organization;      /* 默认 NULL = 空 */
    const char* product;           /* 默认 NULL = 空 */
    const char* description;       /* 默认 NULL = 空 */
    const char* timestamp;         /* 默认 NULL = 每次取当前 UTC       （6344109 追加） */
} stl2stp_export_options;
```

默认值来自 `mesh.cpp`：

```cpp
stl2stp_export_options stl2stp_default_export_options(void) {
    return {1e-6, 0, STL2STP_MODE_ORIGINAL, nullptr, nullptr, nullptr, nullptr, nullptr};
}
```

**函数（共 11 个，`nm` 确认是未经 C++ 名字修饰的 C 符号）**：

| 函数 | 说明 |
| --- | --- |
| `stl2stp_create(void)` | 新建空句柄；失败返回 NULL |
| `stl2stp_destroy(h)` | 销毁句柄；传 NULL 安全 |
| `stl2stp_last_error(h)` | 读取最近一次诊断（不清空）；传 NULL 读取当前线程的创建诊断。指针由库持有，不要 free |
| `stl2stp_default_export_options(void)` | 返回上面的默认值 |
| `stl2stp_load_stl(h, path, cb)` | 自动识别 binary/ASCII；成功后替换当前网格 |
| `stl2stp_get_mesh_stats(h, &stats)` | 唯一顶点数、无向唯一边数、三角面数、AABB |
| `stl2stp_simplify(h, target_faces, cb)` | QEM，目标是正整数面数 |
| `stl2stp_save_stl(h, path, cb)` | 写 little-endian binary STL，法向按绕序重新计算 |
| `stl2stp_export_step(h, path, opts, cb, &topo)` | `opts=NULL` 等于默认；`topo` 可为 NULL，返回的是导出前的拓扑 |
| `stl2stp_export_iges(h, path, opts, cb, &topo)` | 同上 |
| `stl2stp_read_step(h, path, cb, &topo)` | STEP 回读并统计拓扑（`topo` 必填）；不改变当前网格 |

> ABI 注意：字段都追加在结构体末尾，但结构体大小变了，所以**库和桥接代码必须一起重新编译**。
> 始终用 `stl2stp_default_export_options()` 初始化，不要用全零结构体（公差 0 是非法值）。
> bridge 不要用 `-fshort-enums` 编译（C 头文件测试会检查枚举大小为 4 字节）。

线程模型：同一个句柄上的所有操作由调用者串行化。不同句柄的网格操作可以并行，
但所有 OCCT 交换操作（构形、写出、回读）都在库内部的全局 `std::timed_mutex exchange_mutex` 下串行执行，
因为 `Interface_Static` 是进程级全局设置；等锁期间每 10 ms 轮询一次取消。

### 4.3 STL 读取（`mesh.cpp`）

- **Binary**：同时看头部声明的面数和文件长度，最多读两者都允许的完整 50 字节记录；不足 84 字节头部，
  或没有一个完整有效面时，返回失败而不是生成伪网格。以 `solid` 开头的 binary 文件通过长度和非文本字节识别
  （原版 `ReadStlTolerant` 只处理“实际记录少于声明面数”的 binary）。
- **ASCII**：使用 classic locale，只有读到完整的 `endfacet` 才接纳一个 facet，支持多个 `solid`；
  缺少 `endsolid`、半个 facet 或非法尾部时保留完整前缀，并返回 `WARNING_PARTIAL`。
- 任何位置出现 NaN/Inf 坐标都直接失败（`ERROR_FORMAT`）。
- 坐标焊接使用**精确相等**，不是按公差聚类。
- 内部保存两份三角形视图（`internal.hpp` 中的 `Mesh`）：
  - `triangles`：去掉零面积面后的“干净视图”，供 stats、QEM、STL 保存和 SEWING 使用；
    零面积面被跳过时返回 warning。
  - `original_triangles` + `original_weld_required=true`：按源顺序保存的 facet，
    供 ORIGINAL 模式按原版方式重新焊接（见 4.5）。三个节点各不相同的共线面在 ORIGINAL 中**不会**被过滤。

### 4.4 QEM 简化（`simplify.cpp`，对照原版 `MeshSimplify.cpp`）

保留了原版的主要算法：平面 quadric、3×3 最优点求解、端点/中点回退、greedy 优先队列
（`std::priority_queue` + `std::greater`）、法向翻转保护、每条边最多 16 次延迟重试。

| 差异点 | 原版 `SimplifyTriangulation` | 新 `stl2stp_simplify` |
| --- | --- | --- |
| 目标参数 | `theKeepRatio ∈ [0,1]`，目标 = round(ratio × 面数) | 正整数 `target_faces`；闭合模型一次折叠通常删 2 面，所以结果可能比目标少 1 面 |
| 达不到目标 | — | 返回最佳非空结果和 `WARNING_TARGET_NOT_REACHED` |
| 过期堆元素 | 无专门处理 | 用顶点 generation 检查剔除 |
| 等代价时的顺序 | 不确定 | 按 edge id 打破平局，结果确定 |
| 中点回退 | `const Vec3 mid = best;` 不一定是真正的中点 | 显式计算 `(p_a + p_b) / 2` |
| 进度/取消 | `std::function<bool(int%)>`，返回空句柄表示取消 | `stl2stp_callbacks`，返回 `ERROR_CANCELLED`，原网格不变 |
| 输入 | `Poly_Triangulation`，按 `std::map<tuple<double,double,double>>` 焊接 | 库内 `Mesh`，不依赖 OCCT（mesh-only 构建也可用） |

结论：**不保证**与桌面版的简化结果逐顶点、逐字节一致。法向翻转保护不能修复非流形输入。

### 4.5 两种转换模式：ORIGINAL（默认）与 SEWING

#### ORIGINAL：逐步复刻 `RebuildShapeFromMesh`（`exchange.cpp::make_original_shape`）

| 步骤 | 原版（Doc.cpp 行号） | 新实现 |
| --- | --- | --- |
| 0. 焊接 | `RWStl::ReadFile`（754–755）内部的 `Poly_MergeNodesTool`：merge angle `M_PI/2`、tolerance `0.0`、float32 坐标键；只丢掉有两个节点索引相同的三角形 | 未简化的网格：`Poly_MergeNodesTool weld(acos(-1)/2, 0.0)`，按源顺序 `AddTriangle`，与 `RWStl` 同语义（坐标超出 float32 范围时返回 FORMAT）。QEM 之后的网格直接用它自己的共享索引，**不再**按 float32 键焊接 |
| 1. 顶点 | 每个节点一个 `BRepBuilderAPI_MakeVertex`（1639–1653） | 同 |
| 2. 边去重 | 顺序扫描 `(a,b)/(b,c)/(c,a)`，用 64 位键记录首次出现的方向；同向查到给 +idx，反向查到给 −idx（1655–1696） | `std::map<pair>` 先查同向再查反向，`SignedEdge{index, reversed}`；新边按首次出现顺序追加 |
| 3. 边 | 每条唯一边调用 `BRepBuilderAPI_MakeEdge(vA, vB)`（1698–1713） | 同，并检查 `IsDone()` |
| 4. 面 | 只在**第一条**边为负时 `Reverse()`；`BRepBuilderAPI_MakeWire(e1,e2,e3)` → `BRepBuilderAPI_MakeFace(wire)`（1715–1730）。**不用** `MakePolygon` | 完全相同（第二、三条边不显式反转），并检查 `IsDone()`，失败返回 `ERROR_OCCT` |
| 5. 壳 | `BRep_Builder::MakeShell`，所有面放进**一个** shell，`BRepTools::Update(shell)`（1768–1773） | 同 |
| 6. 体 | 无条件 `MakeSolid`，把这一个 shell 放进**一个** solid（1775–1780），不调用 `OrientClosedSolid` | 同 |
| 公差 | 没有任何显式公差（没有 Sewing、ShapeFix、`SetTolerance`），全用 OCCT 默认值（顶点/边 `Precision::Confusion()=1e-7`；`MakeFace` 从边容差推导） | 同，不额外设公差 |
| 可选 | 无 | `merge_coplanar=1` 时对 solid 做共面合并（见 4.6） |

关键代码（摘自 `exchange.cpp`）：

```cpp
TopoDS_Edge first = edge_shapes[triangle[0].index];
if (triangle[0].reversed) { first.Reverse(); }
BRepBuilderAPI_MakeWire wire(first, edge_shapes[triangle[1].index], edge_shapes[triangle[2].index]);
BRepBuilderAPI_MakeFace face(wire.Wire());
...
builder.MakeShell(shell);  for (face : faces) builder.Add(shell, face);
BRepTools::Update(shell);
builder.MakeSolid(solid);  builder.Add(solid, shell);
```

行为后果（与原版一致）：两个分离的立方体、开放网格也会被装进 1 shell / 1 solid；
OCCT 的 STEP reader 回读时可能拆成多个 solid，或把 solid 降级为 shell（见第 6 节）。
重复三角形保留；三个节点不同的共线面交给 OCCT 尝试构面。

#### SEWING：新增模式（`make_sewing_shape`，即 `9ec394a` 时的旧默认行为）

1. 对干净视图中的每个三角形调用 `BRepBuilderAPI_MakePolygon`（三点 + `Close()`），再 `BRepBuilderAPI_MakeFace`。
2. `BRepBuilderAPI_Sewing sewing(sewing_tolerance /*默认 1e-6*/, true, true, true, false)`，用带进度的 `Perform` 缝合。
3. 可选共面合并。
4. 遍历 shell：闭合且 `BRepLib::OrientClosedSolid` 成功的做成 solid，否则保留 shell；游离的 face 也收集进来。
   只有一个组件就直接返回它，多个组件放进 compound。
5. STEP 写出时显式设 `write.step.schema=AP214IS`、`write.step.unit=MM`；IGES 单位固定为 `"MM"`。

`1e-6` 是本库自定的值，**不是**原版的参数（原版没有 Sewing）。SEWING 遇到只有共线面的网格时返回 FORMAT。

#### ORIGINAL 与 SEWING 写出参数的区别

- ORIGINAL 与原版一样，不覆盖 schema/unit/precision。在 OCCT 8.0.1 下实际默认为 AP214IS、MM、precision mode Average。
  IGES 构造时用 `Interface_Static::CVal("write.iges.unit")`。
- 两种模式都保留原版查询的旧键 `Interface_Static::IVal("write.brep.mode")`（OCCT 8.0.1 中没有注册这个键，返回 0；
  新键其实是 `write.iges.brep.mode`）。这是有意保留原版的可观察行为。

### 4.6 共面合并（新增，两种模式都可用）

```cpp
ShapeUpgrade_UnifySameDomain unify(shape, /*UnifyEdges*/true, /*UnifyFaces*/true, /*ConcatBSplines*/false);
unify.SetLinearTolerance(options.sewing_tolerance);   // 默认 1e-6
unify.Build();
```

只合并共面的 face/edge，不做曲面拟合。打开后就不再保持“一个三角形一个面”的原始拓扑。
`UnifySameDomain::Build` 没有进度挂钩，只能在调用前后检查取消。

### 4.7 STEP AP214 / IGES 写出

**STEP**（两种模式相同）：`STEPControl_Controller::Init()` → `STEPControl_Writer` →
`StaticSetting("write.step.product.name", product)` → `Transfer(shape, STEPControl_AsIs, true /*compgraph*/, progress)` →
`APIHeaderSection_MakeHeader(writer.Model(false))` 覆盖文件头 → 遍历模型中所有 `StepBasic_Product`，设置 id/name → `Write(临时文件)` →
`normalize_step_strings` 处理字符串 token → 原子发布。

**IGES**：`IGESControl_Controller::Init()` → 用 RAII 设置 `write.iges.header.{author,company,product,receiver}` →
`IGESControl_Writer(unit, IVal("write.brep.mode"))` → `AddShape(shape, progress)` → `ComputeModel()` →
复制 `IGESData_GlobalSection` 并改写字段 → `SetGlobalSection` → `ClearStartSection()`，
description 非空时 `AddStartLine(description)` → `Write(临时文件, false /*非 FNES*/)` → 原子发布。

两种格式的文件名字段都用**最终输出文件的 basename**，而不是事务用的临时文件名。

### 4.8 可配置 metadata（替换原版硬编码）

原版硬编码（`Doc.cpp` 2005–2030 和 1885–1911）：

```cpp
// STEP
Interface_Static::SetCVal("write.step.product.name", "STL2STP 1.0.0");
author/organization = "VINCENT"; originating system = "STL2STP";
description = "STL TO STEP FILE"; preprocessor = "STL2STP STEP processor";
// IGES
author/company = "VINCENT"; interface version / send name / system id = "STL2STP IGES processor";
```

现在**默认全部为空**，同时清空 OCCT 自己注入的默认值（`APIHeaderSection_MakeHeader` 默认的 `Author`、`Open CASCADE`、
`Open CASCADE STEP processor 8.0`、`Open CASCADE 8.0`、authorisation `Unknown`、`Open CASCADE Model`；
IGES product 默认 `Open CASCADE IGES processor 8.0`、author 默认宿主用户名；STEP product 名为空时 OCCT 会回退为 `Product`，
这个回退通过 Transfer 之后逐个设置 `StepBasic_Product` 消除）。

字段映射：

| 选项 | STEP | IGES |
| --- | --- | --- |
| `author` | `FILE_NAME.author`（唯一元素） | Global 参数 21 |
| `organization` | `FILE_NAME.organization` | Global 参数 22 |
| `product` | `FILE_NAME.originating_system`；每个 `PRODUCT.id`/`PRODUCT.name` | Global 参数 3（send name）、5（system id） |
| `description` | `FILE_DESCRIPTION.description` | Start section（IGES Global 没有 description 槽位） |
| `timestamp` | `FILE_NAME.time_stamp` | Global 参数 18、25 |
| 固定清空 | preprocessor_version、authorisation | interface version（6）、receiver（12）、application protocol（26） |

- 四个文本字段是 UTF-8 C 字符串；`NULL` 和 `""` 都表示空，`options=NULL` 也**不会**恢复旧的硬编码。
- 库在第一次回调、获取 mutex **之前**就把它们复制成私有 `std::string`；调用者只需保证同步调用期间内存有效。

本次实测的默认 STEP 头（ORIGINAL，cube）：

```text
FILE_DESCRIPTION((''),'2;1');
FILE_NAME('demo-cube.step','2026-10-20T13:14:24Z',(''),(''),'','','');
FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));
...
#7 = PRODUCT('','','',(#8));
```

仍然非空的只是格式和几何必需的字段（schema、AP214 上下文、单位 MM、uncertainty 1.E-07；
IGES 分隔符、数值精度 `32,308,15,308,15`、单位 `1.,2,2HMM`、版本 `11`（IGES 5.3）等），详见 `docs/core-api.md`。

### 4.9 时间戳

- `timestamp=NULL`：每次导出都重新取**当前 UTC**（`std::gmtime`），格式 `YYYY-MM-DDTHH:MM:SSZ`。
- `timestamp=""`：**非法**，返回 `ERROR_ARGUMENT`（不提供空日期选项）。
- 覆盖值只接受 ISO 8601 扩展格式的子集：`YYYY-MM-DDTHH:MM:SS[.fraction][Z|+HH:MM|-HH:MM]`。
  - 年份 0001–9999，按 Gregorian 闰年规则校验，时 00–23、分秒 00–59。
  - 省略时区视为 UTC。
  - 不接受 `24:00`、闰秒 60、逗号小数、`+0800` 这种无冒号偏移、basic/week/ordinal 格式，或前后带空白。
- **STEP**（`core/utc-timestamp` 起，见第 10 节）：与 IGES 一样按偏移换算成 UTC，写成 `YYYY-MM-DDTHH:MM:SS[.fraction]Z`，小数秒原样保留；`Z`/`+00:00`/`-00:00`/无偏移统一写 `Z`。例：`2026-10-21T10:30:00+08:00` → `2026-10-21T02:30:00Z`。此前（`65e30ad`–`03d3713`）STEP 原样保留覆盖字符串，包括偏移。
- **IGES**（`65e30ad`）：按偏移换算成 UTC，自行用 Gregorian 算术处理跨日、跨月、跨年和闰年，不依赖本地时区或 `timegm`；
  小数秒直接截断，写成 `15HYYYYMMDD.HHNNSS`，参数 18 和 25 写相同的值。换算后超出 0001–9999 时返回 ARGUMENT。
  例：`2026-10-20T18:52:00+08:00` → `15H20261020.105200`；`2024-02-29T18:52:00.123+08:00` → `15H20240229.105200`。

### 4.10 字符编码与转义

**STEP**（OCCT 8.0.1 的 writer 不会把 UTF-8 自动转成 Unicode directive）：`step_text()` 先校验 UTF-8，
拒绝 overlong、截断、surrogate 和超过 U+10FFFF 的序列（返回 ARGUMENT），然后：

- 可打印 ASCII 0x20–0x7E 保持原样；
- BMP 字符写成 `\X2\` + 4 位大写十六进制 + `\X0\`，非 BMP 写成 `\X4\` + 8 位大写十六进制 + `\X0\`，相邻的同类字符共用一个 directive；
  例：`致知屋` → `\X2\81F477E55C4B\X0\`，`😀𠮷` → `\X4\0001F60000020BB7\X0\`；
- 单引号交给 writer，由 OCCT 加倍一次，最终为 `''`；
- 字面反斜杠先编码成 `\X2\005C\X0\` 交给 writer（防止用户文本被当成 directive），写完后由 `normalize_step_strings`
  在临时文件的字符串 token 内流式还原为标准的 `\\`；同时去掉 writer 在长字符串中按 72 列插入的物理 CR/LF，避免 directive 被拆断。

本次实测（CLI `--author '致知屋' --org "O'Brien" --product STL2STP --description 'a\b' --timestamp '2024-02-29T18:52:00.123+08:00'`）：

```text
FILE_DESCRIPTION(('a\\b'),'2;1');
FILE_NAME('demo-cube-meta.step','2024-02-29T10:52:00.123Z',(
    '\X2\81F477E55C4B\X0\'),('O''Brien'),'','STL2STP','');
#7 = PRODUCT('STL2STP','STL2STP','',(#8));
```

**IGES**：没有 Unicode directive，保留原始 UTF-8 字节，Hollerith 长度按**字节数**计算：
`9H致知屋`、`12H测试公司`（而不是 `3H`/`4H`）。
为避免 OCCT setter 把以 `6H...` 开头的用户字符串误认为 Hollerith，库会先自己加上长度前缀。

- **195 字节上限**：OCCT `IGESData_GlobalSection::Params` 使用 `char text[200]` 和无边界 `Sprintf`，
  所以 author / organization / product / 最终 basename 各自限制在 **195 个 UTF-8 字节**以内（195 + `195H` + NUL = 200）。
  超过就返回 ARGUMENT，不截断也不写文件。
- IGES 的四个文本和 basename、description 都不允许包含 CR/LF。

本次实测 IGES Global（`--author '致知屋' --timestamp '2026-10-20T18:52:00+08:00'`）：

```text
,,,13Hdemo-cube.igs,,,32,308,15,308,15,,1.,2,2HMM,1,0.01,               G0000001
15H20261020.105200,1E-07,1.,9H致知屋,,11,0,15H20261020.105200,;      G0000002
```

### 4.11 `Interface_Static` 全局参数的 RAII 保存/恢复

```cpp
class StaticSetting {               // exchange.cpp
public:
    StaticSetting(const char* name, const char* value) : name_(name) {
        const char* previous = Interface_Static::CVal(name);
        if (previous) { previous_ = previous; }
        if (!Interface_Static::SetCVal(name, value)) throw Error(STL2STP_ERROR_OCCT, "Cannot configure OCCT parameter: " + name_);
    }
    ~StaticSetting() { try { Interface_Static::SetCVal(name_.c_str(), previous_.c_str()); } catch (...) {} }
};
```

每次导出都在 `exchange_mutex` 锁内设置 `write.step.product.name`（SEWING 还会设置 `write.step.schema` 和 `write.step.unit`）
以及 IGES 的四个 header 键。无论成功、取消还是异常，析构时都会恢复原值，所以连续两次导出不会串值（有测试覆盖）。
锁之外直接调用 OCCT 的代码不受保护，需要应用自行协调。

### 4.12 进度与取消

- `progress(fraction)` 在调用线程上同步执行，单调递增，取值范围 [0,1]，成功时一定包含 0 和 1。
- `cancelled()` 返回非零即请求取消。加载、QEM、STL 写出在循环中检查；
  OCCT Sewing、STEP/IGES Transfer 和 STEP 回读通过 `Message_ProgressIndicator`（内部类 `Indicator`）+ `Message_ProgressRange` 检查，
  取消请求会转成 OCCT 的 UserBreak。
- OCCT 的 `ReadFile`、`UnifySameDomain::Build`、`Write`、IGES `ComputeModel` 没有进度挂钩，只能在调用前后检查，
  **不保证能立即中断这些阶段**。
- 取消后返回 `ERROR_CANCELLED`；网格、已有目标文件和 topology 输出都不变，不残留临时目录。
- 回调不能重入本库，也不能抛异常；UI 更新应 dispatch 到主队列。

### 4.13 STEP 回读

`stl2stp_read_step`：`STEPControl_Reader::ReadFile` → 带进度的 `TransferRoots` → 用 `TopExp::MapShapes` 统计唯一的
solid/shell/face/edge/vertex（shell 的计数包含 solid 里的 shell，不能和 solid 数相加）。
它也在 `exchange_mutex` 下执行。IGES 没有公共回读 API。

### 4.14 CLI `stl2stp-cli`

```text
stl2stp-cli info <in.stl>
stl2stp-cli simplify <in.stl> <out.stl> --faces N
stl2stp-cli convert <in.stl> <out.step|out.stp|out.igs|out.iges> [--mode original|sewing] [--merge-coplanar] [--simplify N]
      [--author TEXT] [--org TEXT] [--product TEXT] [--description TEXT] [--timestamp ISO8601]
stl2stp-cli readback <in.step>
```

- 输出格式由扩展名决定（不区分大小写）。省略 `--mode` 等于 `original`，导出前打印 `mode=original|sewing`。
- 打印 `load_ms`、`simplify_ms`、`export_ms`、`readback_ms`、mesh stats、bbox、导出前拓扑和 `output_bytes`。
  STEP 导出后会自动回读并打印回读拓扑。
- 选项未知、重复或缺值时失败（退出码 1）；参数个数不对时打印用法（退出码 2）；Ctrl-C 取消（退出码 130）。
- `sewing_tolerance` **没有** CLI 参数，CLI 固定使用默认值 1e-6。

示例：

```sh
C=build/core-linux/stl2stp-cli; D=build/core-linux/tests/data
$C info $D/cube-binary.stl
$C simplify $D/sphere.stl build/core-linux/reduced.stl --faces 2000
$C convert $D/cube-binary.stl build/core-linux/cube.step                       # ORIGINAL，元数据为空，时间为当前 UTC
$C convert $D/cube-binary.stl build/core-linux/cube.step --mode sewing --merge-coplanar
$C convert $D/sphere.stl build/core-linux/sphere.step --simplify 9900
$C convert $D/cube-binary.stl build/core-linux/cube.igs --author '致知屋' --org '测试公司' --product '测试产品' --description 'test'
$C convert $D/cube-binary.stl build/core-linux/cube.step --timestamp '2024-02-29T18:52:00.123+08:00'
$C readback build/core-linux/cube.step
```

### 4.15 构建系统（`core/CMakeLists.txt`）

- `cmake_minimum_required(3.16)`，项目 `stl2stp-core 0.1.0`，C++17（`CXX_EXTENSIONS OFF`，PIC）。
- 选项：`STL2STP_WITH_OCCT`（默认 ON；OFF 时只构建网格部分，导出返回 `ERROR_OCCT`，不运行 OCCT 测试，
  **不能代替完整验收**）、`STL2STP_BUILD_CLI`（默认 ON）、`BUILD_TESTING`（CTest）。
- 缓存变量：`OCCT_ROOT`、`OCCT_INCLUDE_DIR`（默认在 `OCCT_ROOT/include/opencascade` 或 `OCCT_ROOT/inc` 查找）、
  `OCCT_LIBRARY_DIR`（默认在 `lib`/`lib64` 查找）、`OCCT_EXTRA_LIBRARIES`（给自定义的静态 OCCT 用）。
- 直接链接的 toolkit：`TKDESTEP TKDEIGES TKXSBase TKBool TKBO TKPrim TKShHealing TKTopAlgo TKGeomAlgo TKBRep TKGeomBase TKG3d TKG2d TKMath TKernel`，
  如果存在 `TKDE` 也加上。优先使用 `OpenCASCADEConfig.cmake` 提供的 imported targets，没有时用 `find_library`。
  直接选中 `TKV3d|TKOpenGl|TKService` 时配置会报错；作为传递依赖被引入时只打印 STATUS。
- Linux 专用：`LINKER:--disable-new-dtags` + build/install RPATH = `OCCT_LIBRARY_DIR`。因为 OCCT 的 `.so` 本身不带 RPATH，
  默认的 `DT_RUNPATH` 找不到间接依赖（例如 `libTKDE.so.8.0`）。这样就不需要 `LD_LIBRARY_PATH` 或 `ldconfig`。

### 4.16 编码规范化提交与 `.gitattributes` / `.gitignore`

`224958d` 去掉了 `.gitattributes`、`.gitignore`、`AGENTS.md`、`README.md` 开头的 UTF-8 BOM（由 Windows 端创建时带入）。
现在的内容：

```gitattributes
* text=auto eol=lf
*.png binary
*.jpg binary
*.stl binary
```

```gitignore
reference/
build/
out/
*.step
*.stp
*.igs
*.iges
core/tests/data/*.stl
!core/tests/data/small_*.stl
.vs/
*.user
```

所有新源码和文档都是 UTF-8 无 BOM、LF、无行尾空白，`git diff --check` 通过（`docs/core-validation.md` 记录）。
原版 `stl2stp-codeDoc.cpp` 实际是 UTF-8 with BOM / CRLF，部分注释已经乱码；reference 不入库，也没有改动。

---

## 5. 构建与测试 core

### 5.1 命令

在仓库根目录 `/mnt/d/stl2stp-ios` 执行。`TMPDIR` 只对当前命令生效，不修改全局环境：

```sh
cd /mnt/d/stl2stp-ios
mkdir -p build/tmp
export TMPDIR="$PWD/build/tmp"     # 只在当前 shell 中有效

# Release
cmake -S core -B build/core-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DOCCT_ROOT="$HOME/occt-install" -DSTL2STP_WITH_OCCT=ON
cmake --build build/core-linux --parallel 8
ctest --test-dir build/core-linux --output-on-failure -j 6

# Debug + -Werror + UBSan
FL="-Wall -Wextra -Wpedantic -Werror -fsanitize=undefined -fno-sanitize-recover=undefined"
cmake -S core -B build/core-linux-debug-ubsan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DCMAKE_CXX_FLAGS="$FL" -DCMAKE_C_FLAGS="$FL" \
  -DOCCT_ROOT="$HOME/occt-install" -DSTL2STP_WITH_OCCT=ON
cmake --build build/core-linux-debug-ubsan --parallel 8
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/core-linux-debug-ubsan --output-on-failure -j 6
```

测试数据由 CTest fixture（`stl2stp-fixture-generator`）在 `<build>/tests/data/` 中自动生成，不需要从 reference 拷贝模型。
可以用 `readelf -d build/core-linux/stl2stp-cli` 检查 RPATH，用 `ldd` 检查依赖。

### 5.2 本次实测结果（2026-10-20 21:13–21:14 UTC+8，HEAD `65e30ad`，从零配置，构建目录 `build/repro-release` 和 `build/repro-debug-ubsan`）

| 构建 | 配置/编译 | 编译 warning | CTest |
| --- | --- | --- | --- |
| Release | rc=0，配置 + 编译约 12 s（`--parallel 8`） | 0 | **104/104 通过**，real 4.30 s |
| Debug `-Werror` + UBSan | rc=0（配置 + 编译 + 测试共约 23 s） | 0 | **104/104 通过**，real 4.53 s，日志中 `runtime error` 出现 0 次 |

Release 标签汇总：`mesh = 1.07 s (10 tests)`，`metadata = 14.91 s (58 tests)`，`occt = 15.90 s (54 tests)`。
Debug 标签汇总：`mesh = 2.41 s (10)`，`metadata = 14.54 s (58)`，`occt = 15.53 s (54)`。
（标签有重叠，三者相加不等于 104。）

测试数量演进：25（`9ec394a`）→ 46（`7d961fe`）→ 85（`c0a553a`）→ 104（`6344109`/`65e30ad`）。
覆盖范围包括：C99 头文件/ABI、binary/ASCII/截断/非法输入、统计、QEM、STL 往返、UTF-8 路径、
两种模式 × cube/two-shells/退化+重复/共线/仅共线/截断/IGES、merge 开和关、各阶段取消（0/0.45/0.8/1）且不留临时文件、
metadata 默认为空/中文往返/连续导出不串值/两个句柄并发、IGES 195 字节边界和 CR/LF 拒绝、时间戳格式/闰年/非法值/UTC 换算、
STEP X2/X4/引号/反斜杠字节对照与 OCCT 回读、`IGESModel::VerifyCheck`，以及 CLI 的各种参数组合。

---

## 6. 验证与基准（数字全部来自 `docs/core-validation.md`，另有一项注明为本次实测）

环境：WSL Ubuntu 20.04，12 核，Release，g++-10，OCCT 8.0.1。

### 6.1 拓扑（solids / shells / faces / edges / vertices）

| fixture | 模式 | 导出前 | STEP 回读 |
| --- | --- | --- | --- |
| cube | 两种模式 | 1/1/12/18/8 | 1/1/12/18/8 |
| cube，merge ON | 两种模式 | 1/1/6/12/8 | 1/1/6/12/8 |
| two-shells | ORIGINAL | 1/1/24/36/16 | 2/2/24/36/16 |
| two-shells | SEWING | 2/2/24/36/16 | 2/2/24/36/16 |
| degenerate-duplicate cube | ORIGINAL | 1/1/13/18/8 | 1/2/13/18/8 |
| degenerate-duplicate cube | SEWING | 1/1/13/21/11 | 1/2/13/21/11 |
| cube + collinear | ORIGINAL | 1/1/13/20/9 | 1/2/13/20/9 |
| cube + collinear | SEWING | 1/1/12/18/8 | 1/1/12/18/8 |
| collinear-only | ORIGINAL | 1/1/1/3/3 | 0/1/1/2/2 |
| collinear-only | SEWING | FORMAT，不写出 | — |
| truncated binary（5 个完整面） | ORIGINAL | 1/1/5/12/8 | 0/2/5/12/8 |
| truncated binary | SEWING | 0/2/5/12/9 | 0/2/5/12/9 |

### 6.2 19,800 面 UV 球：ORIGINAL 与 SEWING（输出写到 WSL ext4，3 次交替运行）

| 模式 | 导出 ms | STEP 字节 | 回读 ms | 回读拓扑 | 峰值内存 |
| --- | --- | ---: | --- | --- | ---: |
| ORIGINAL | 3885 / 4170 / 4331 | 48,554,145 | 6930–7217 | 1/1/19800/29700/9902 | 448 MB |
| SEWING | 8401 / 8492 / 8874 | 51,183,093 | 7280–7603 | 1/1/19800/29700/9902 | 468 MB |

ORIGINAL 导出大约快 2 倍（省掉了 Sewing），文件大约小 5%。

### 6.3 其它组合（输出写到 `/mnt/d`，单次运行，耗时只看量级）

| 输入 / 选项 | ORIGINAL：导出 ms，字节 | SEWING：导出 ms，字节 |
| --- | --- | --- |
| 球 + `--merge-coplanar` | 8388，47,867,342（回读 19408 面） | 16967，50,391,239（19408 面） |
| 球 + `--simplify 9900` | 5219，24,255,046 | 8469，25,547,311 |
| 球 → IGES | 9579，40,219,416，峰值 175 MB | 14095，40,219,416，峰值 271 MB |
| cube | 27，24,429 | 44，26,038 |
| two-shells | 34，47,445 | 65，53,846 |

注：上表 ORIGINAL 的字节数是**旧版带 VINCENT 元数据时**测得的。本次（元数据为空的默认值）实测 ORIGINAL cube STEP 为 **24,284 字节**，
SEWING + merge 的 cube 为 **15,937 字节**。

### 6.4 早期（SEWING 默认时期）的规模数据

- QEM：19,800 面球简化到 9,900 面只用 **31 ms**；简化到 50% 后，导出时间和文件大小都大约减半。
- 共面合并：cube 的 STEP 从 26,038 降到 16,080 字节（约 −38%）；球几乎没有共面三角形，只减少约 1.5%。
- STEP 大约 2.6 KB/面，导出大约 0.87 ms/面（SEWING），基本是线性的。
- 2 万面 STEP 回读约 10–12 s；在 `/mnt/d` 上单次测到过 32 s（I/O 波动）。写 `/mnt/d` 明显比写 WSL 自身的 ext4 慢。

---

## 7. 已知限制与 TODO

1. ~~OCCT 8.0.1 读不回纯中文 IGES Start section~~：`core/fixes` 起 IGES metadata 只写 ASCII（非 ASCII → `?`，返回 METADATA_CHANGED），此限制不再适用（9.2 第 7 条）。
2. **OCCT 往 stdout 打印彩色统计**：STEP 写出时 OCCT 会输出 `Statistics on Transfer (Write)`（带 ANSI 颜色），本次 CLI 实测仍可见。
   iOS 集成前应通过 `Message` 系统静默掉。**未处理**。
3. **没有在真实 CAD 中验证**【未验证】：SolidWorks 等能否正确导入 `\X2\`/`\X4\` STEP 和 UTF-8 Hollerith IGES 尚未测试；
   本项目只用 OCCT 自己回读过。IGES 5.3 原始规范 PDF 没能下载，没有逐条审核过。
4. **OCCT 7.9 兼容性**【未验证】：代码里保留了 7.x 条件分支，但只在 8.0.1 上编译验证过。
5. **iOS 构建**【未验证】：需要用 iOS toolchain 把 OCCT 编译成 `BUILD_LIBRARY_TYPE=Static`（device 和 simulator 分开）；
   core 用同一个 toolchain，设 `OCCT_ROOT` 指向它，并设置 `BUILD_SHARED_LIBS=OFF`、`STL2STP_BUILD_CLI=OFF`、`BUILD_TESTING=OFF`；
   之后再做 XCFramework 打包和 SwiftUI + Objective-C++ bridge。
6. **需要 Mac + Xcode + Apple 开发者账号**（¥688/年）才能做 iOS 编译和上架。
7. **LGPL-2.1 合规**：OCCT 是 LGPL-2.1（带 OCCT exception）。iOS 只能静态链接，发布前需要评审许可证义务（如何允许用户重新链接、源码提供、声明等）。
8. 与原版**不保证**逐字节一致：OCCT 7.0 → 8.0.1 的内部处理、动态公差、实体排序不同；QEM 有改进；时间戳和 metadata 是有意改变的。
9. 没有隐含的 100000 面上限，也不会自动简化，相当于原版确认框里选“是”。UI 层需要自己提示或限流。
10. 无进度挂钩的 OCCT 阶段（`ComputeModel`、`UnifySameDomain`、`ReadFile`、IGES `Write`）不能立即取消；`core/fixes` 起 STEP 写盘每 1 MB 检查一次取消。
11. `core/extract` 还**没有合并到 `main`**（等待 Bo 决定）。
12. `core/fixes` 的其余未决问题见 9.6。

---

## 8. 从零复现清单

1. 准备 WSL Ubuntu 20.04（或原生 Ubuntu 20.04），装好 gcc-10/g++-10、ninja、git、curl，以及 CMake ≥ 3.16（我们用的是 3.26.4）（第 2 节）。
2. `g++-10 --version` 应显示 10.5.0；不要用默认的 g++ 9.4 编译 OCCT。
3. 下载 OCCT `V8.0.1.tar.gz`，核对 SHA256 `6297cc55…3c18`，解压到 `~/stl2stp-work/occt-src/OCCT-8.0.1`（3.1）。
4. 按 3.2 的完整参数配置 OCCT；按 3.3 执行 `ninja -j12 && ninja install`（12 核约 20 分钟）。
5. 核对 `~/occt-install/include/opencascade/Standard_Version.hxx` 为 8.0.1，`lib/` 下有 48 个 `libTK*.so`，以及 `lib/cmake/opencascade/OpenCASCADEConfig.cmake`（3.4）。
6. 获取本仓库：从 Bo 处拷贝 `D:\stl2stp-ios` 整个目录，或用 `git bundle` 传 `core/extract` 分支（不要添加 remote，也不要 push）。
   原版源码另外放到 `reference/stl2stp-master/`，只读，仅用于对照，构建不需要。
7. `git checkout core/extract`，确认 `git log --oneline` 包含 `65e30ad … d1a8084` 和本文提交。
8. 按 5.1 构建 Release，运行 `ctest`，应为 **104/104 通过**、0 warning（`core/fixes`：**118/118**）。
9. 按 5.1 构建 Debug `-Werror` + UBSan，运行 `ctest`，应为 **104/104 通过**，没有 `runtime error`（`core/fixes`：118/118；ASan 用 `core/tools/run-sanitizer-ctest.sh <dir> asan`）。
10. 用 4.14 的 CLI 示例抽查：默认 STEP 头的作者、组织、产品都为空，时间为当前 UTC 并以 `Z` 结尾；
    中文 author 写成 `\X2\81F477E55C4B\X0\`；IGES 是 `9H致知屋`、`15H20261020.105200`。
11. 可选：用 `build/core-linux/tests/stl2stp-fixture-generator <dir> --rings 200 --segments 200` 生成大模型，对照第 6 节的量级做性能复测。
12. 可选：用真实 CAD（SolidWorks 等）打开导出的 STEP/IGES，并把结果反馈回来（第 7 节第 3 条）。

---

## 9. core/fixes：代码审查后的修复（分支 `core/fixes`，基于 `core/extract` `4c71bcb`）

2026-10-20 的代码审查（box 上的 `stl2stp-review-and-research.md`）列出的问题，先写成会失败的 C++ 测试，
再逐项修复。新测试可执行文件 `stl2stp-review-tests`，CTest 标签 `review`。读回 STEP 时**关闭 OCCT reader 端的
shape healing**（`SetShapeProcessFlags(空)`），因为别的 CAD 拿到的是文件原样；不关的话 OCCT 读入时会自动修好，
掩盖问题（一开始就是这样漏掉的）。

### 9.1 复现结果（修复前，`core/extract` 4c71bcb）与修复后对照

| 用例 | 修复前（4c71bcb，读回关闭 healing） | 修复后 |
| --- | --- | --- |
| 开口盒（缺 2 个三角形） | 返回 0，写成 solid，BRepCheck 无效，体积 0.8 | 返回 3（NOT_SOLID），1 个 open shell，有效 |
| 一个三角形反向 | 无效 solid，体积 0.8333 | 返回 4（REPAIRED），有效，体积 1 |
| 整体反向 | 体积 −1.0 | 返回 4，体积 +1.0，有效 |
| 两立方体共享一条边（非流形） | 无效，体积 2 | 返回 4，改走 SEWING，1 个有效 solid，体积 2 |
| 两个分离立方体 | 1 个 solid，无效 | 2 个 solid，有效，体积 2 |
| 共线退化三角形 | 无效 | 返回 4，丢弃 1 个零面积三角形，有效 |
| QEM 开放平面 800→200 | 面积 0.250024（边界塌缩） | 面积 1.000000（800→10 也是 1.0） |
| QEM 立方体 1200→8 | 1 条非流形边、1 个重复面、体积 0.333、Euler 3 | 流形、无重复面、Euler 2、体积 0.5；1200→12 精确还原立方体 |
| 旋转 float32 立方体 192 三角形 + merge | 129 个面 | 6 个面（n=10 / 1200 三角形也是 6） |
| 旋转立方体 12 三角形 + merge | 9 个面 | 6 个面（ShapeFix 后有效） |
| IGES 中文 metadata | UTF-8 Hollerith（OCCT 读不回 Start section） | ASCII `?` 替换，返回 5（METADATA_CHANGED） |
| 封闭立方体（对照） | 有效 | 不变（逐字节相同） |
| QEM 尺度不变性（0.001/1/1000） | 已通过 | 仍通过 |
| 畸形 STL fuzz 300 例 | Release 无崩溃（242 个可加载） | Release/UBSan/ASan 均无崩溃、无报告（242 加载、31 导出） |


### 9.2 改动

1. **ORIGINAL 校验与修复**（`exchange.cpp::make_original_shape`）：丢弃面积 < `1e-12 × 对角线²` 的三角形；统计边使用次数，
   非流形 → 改用 SEWING；BFS 统一每个连通分量的绕序，不可定向 → SEWING；封闭分量有符号体积为负则翻转；
   每个封闭分量一个 solid、开放分量写成 open shell；最后 `BRepCheck_Analyzer`，无效 → SEWING。
   有效、封闭、朝向一致、单连通的输入：三角形顺序与方向都不变，几何与旧版逐字节相同（9.4）。
   `strict_original=1`（CLI `--strict-original`）保留旧行为。
2. **新返回码**（追加，不改旧值）：`STL2STP_WARNING_NOT_SOLID=3`、`STL2STP_WARNING_REPAIRED=4`、
   `STL2STP_WARNING_METADATA_CHANGED=5`；`last_error` 给出全部说明。SEWING 模式结果含开放 shell 时也返回 NOT_SOLID。
3. **options 追加字段**：`merge_angular_tolerance`（默认 1e-4 rad，0 = 默认，范围 [0, 0.1]）、`strict_original`（默认 0）。
   结构体变大，bridge 必须与新头文件一起重编译，始终用 `stl2stp_default_export_options()` 初始化。
4. **QEM**（`simplify.cpp`）：面积加权 quadric；开放边界/非流形边加垂直约束平面（权重 1e3×边长²）；link condition、
   对顶点度数、合并后度数、“两端在边界但边不在边界”检查；第一阶段法向夹角余弦 > 0.25 且三角形质量 ≥ 0.05（或不变差），
   未达目标时第二阶段只放宽这两项；最优点的条件数判断和截断范围改为相对模型尺寸；被拒候选的重排惩罚也按尺度。
5. **共面合并**：设置角度容差（默认 1e-4 rad），线性容差 `max(sewing_tolerance, 1e-6×对角线, 4×FLT_EPSILON×最大坐标)`；
   合并后用 `ShapeFix_Shape` 修整，再做 BRepCheck，原本有效而合并后无效则撤销合并（REPAIRED）。
6. **STEP 写出**：去掉“写完再整份逐字符重写”的 `normalize_step_strings`，改为 `WriteStream` + 流式过滤器
   `StepStringFilter`，一次写盘，1 MB 分块，分块时检查取消；输出字节不变。
7. **IGES metadata 只写 ASCII**：非 ASCII 码点 → `?`，返回 METADATA_CHANGED；单引号不替换（STEP 按规范写双单引号）。
8. CLI：`--strict-original`、`--merge-angle RADIANS`。
9. 测试：新增 `stl2stp-review-tests`（14 项），更新 `test_core.cpp` / `test_metadata.cpp` 中被有意改变的期望
   （见 9.3）。

### 9.3 有意改变的旧行为（测试已同步更新）

- `collinear_only`（全部三角形共线）两种模式都拒绝导出（旧 ORIGINAL 会写出无效 solid）。
- `two_shells`：ORIGINAL 由 1 个无效 solid 变成 2 个 solid。
- `degenerate_duplicate`、`truncated`：返回 NOT_SOLID，写成 shell；`truncated` ORIGINAL 导出拓扑 0/2/5/12/8，读回 0/2/5/12/9。
- `duplicate_open`、`collinear`、`zero_area_duplicate`（ORIGINAL）：返回 REPAIRED。
- IGES 非 ASCII metadata / 文件名：写成 `?`，返回 METADATA_CHANGED；测试改为断言 `3H???`、`4H????`。
- 共面合并：角度容差由 OCCT 默认 1e-12 改为 1e-4 rad，合并后先 ShapeFix 再 BRepCheck，无效则撤销；因此有效封闭输入在 `--merge-coplanar` 下的输出与旧版不同（例如 sphere-ascii 合并输出 47.3 MB → 30.9 MB）。


### 9.4 与旧版逐字节对比（有效封闭输入）

用 `core/extract` 与 `core/fixes` 的 Release CLI 对 `tests/data` 下全部 18 个 STL 以 ORIGINAL 模式导出（固定 `--timestamp` 与 metadata）：
有效封闭输入 `cube-ascii`、`cube-binary`、`cube-solid-header`、`sphere`、`sphere-ascii`、`degenerate`、`cli-simplified` 的
STEP 与 IGES **逐字节相同**。不同的只有有意改变的无效/开放输入（`collinear*`、`degenerate-duplicate`、`truncated-*`、`two-shells`、
`zero-area-duplicate`）和本来就失败的输入（`empty-binary`、`huge-ascii`、`invalid`、`nonfinite`，两边都返回错误）。

性能（sphere-ascii.stl 3.6 MB → 48 MB STEP，同一台机器，单次）：普通 STEP 导出 18.4 s → 14.3 s（去掉整文件逐字符重写）；
`--merge-coplanar` 22.6 s → 16.5 s；`simplify --faces 2000` 0.19 s → 0.21 s。box 上的纯网格 harness：sphere 19800→2000 最大径向偏差
0.00227 → 0.00199，最小三角形质量 0.421 → 0.244（更紧的翻转/质量检查的代价是更保守，第二阶段放宽）。


### 9.5 测试结果

- Release：**118/118** 通过，0 warning。
- Debug `-Werror` + UBSan：**118/118** 通过，0 个 `runtime error`。
- Debug `-Werror` + ASan + UBSan：**118/118** 通过，0 报告（`detect_leaks=0`，`setarch -R`，见 9.6）。
- 统一用 `core/tools/run-sanitizer-ctest.sh` 跑 sanitizer：每测试 120 s 超时、日志上限 20 MB、目录超 2 GB 自动停止。


### 9.6 仍未解决

1. 旋转模型共面合并后，**关闭全部 reader healing** 读回时 OCCT 对部分平面报 `BRepCheck_UnorientableShape`（内存中与默认 healing 读回均有效）。
   测试只断言 healing 读回有效；需要在真实 CAD 中确认。
2. LeakSanitizer：OCCT 8.0.1 每次 transfer 在进程退出时留下约 7.6 MB（`Standard::Allocate`，栈无本项目帧），基线 `core/extract` 也有。默认关闭 LSan。
3. gcc-10 ASan 在 WSL2 内核 6.18（高 `mmap_rnd_bits`）上偶发无限 `AddressSanitizer:DEADLYSIGNAL`，曾把日志写到 31 GB。
   规避：`setarch -R` + 输出上限（脚本已内置）。根治需 `vm.mmap_rnd_bits=28`（需 root）或更新的 gcc/clang。
4. `simplify` 在大目标比例下略慢（box 测 31→39 ms、41→60 ms），最小三角形质量下降，见 9.4。
5. 共面合并输出与旧版不同（有意，容差更合理），未在真实 CAD 中验证。

---

## 10. core/utc-timestamp：STEP 时间戳统一写 UTC（分支 `core/utc-timestamp`，基于 `main` `03d3713`）

- 需求：`--timestamp`（或 API `options.timestamp`）带 UTC 偏移时，STEP 的 `FILE_NAME.time_stamp` 也换算成 UTC 并以 `Z` 结尾，与 IGES 一致。
- 改动：`core/src/exchange.cpp` 的 `timestamp_value()` 在已有的 Gregorian UTC 换算结果上，同时生成
  `YYYY-MM-DDTHH:MM:SS[.fraction]Z` 给 STEP 使用（小数秒逐字保留），IGES 输出不变。校验规则、非法值处理
  （返回 `STL2STP_ERROR_ARGUMENT`、不发布目标文件）和默认当前 UTC 都不变。
- 示例：`2026-10-21T10:30:00+08:00` → STEP `2026-10-21T02:30:00Z`、IGES `15H20261021.023000`；
  `2026-10-21T10:30:00-05:30` → `2026-10-21T16:00:00Z`；`2026-10-21T10:30:00`（无偏移，视为 UTC）→ `2026-10-21T10:30:00Z`；
  `2024-02-29T18:52:00.123+08:00` → `2024-02-29T10:52:00.123Z`。
- 测试：`core/tests/test_metadata.cpp` 的 `timestamp` 用例表增加 STEP 期望列，并新增 `+08:00`、`-05:30`、`Z`、`+00:00`、
  `-00:00`、无偏移和带小数跨日的用例；`cli_timestamp` 与 `copy` 场景的 STEP 期望改为 UTC。
- 同一分支另把中文示例作者 metadata 统一换成 `致知屋`（`\X2\81F477E55C4B\X0\`，IGES ASCII 替换为 `3H???`）。

## 11. core/weld：近重合顶点容差焊接（分支 `core/weld`，基于 `main` `1385bb5`）

- 起因：开放极点 UV 球（极点坐标相差几个 ULP）精确去重后留下缝隙，ORIGINAL 只能写出开放 shell，QEM 简化后也无法封闭。
- 改动：新文件 `core/src/weld.cpp`（容差 = `max(1e-6 × 包围盒对角线, 2 × FLT_EPSILON × 最大坐标)`，网格哈希 O(n)，
  只焊开放边界顶点，不让三角形退化）；`exchange.cpp` 在 ORIGINAL 的 `Poly_MergeNodesTool` 之前应用并返回 REPAIRED；
  新 API `stl2stp_weld_vertices`；options 追加 `weld_vertices`（默认 1）/`weld_tolerance`；CLI `--no-weld`、`--weld-tol`。
  `strict_original=1` 仍是逐字节旧行为。细节、容差理由与耗时见 `docs/core-api.md` “core/weld”。
- 测试：`weld_open_pole`、`weld_guards`；`qem_near_coincident_pole` 显式关闭焊接以保留“精确合并时报告开放”的断言。CTest 120 → 122，全部通过。

## 附录 A：原版 → 新代码对照速查

| 原版符号 | 新代码 |
| --- | --- |
| `Cstl2stpcodeDoc::LoadStlAndDisplay` / `RWStl::ReadFile` | `stl2stp_load_stl`（`mesh.cpp`）+ ORIGINAL 中的 `Poly_MergeNodesTool(M_PI/2, 0.0)` |
| `ReadStlTolerant`（截断 binary） | `mesh.cpp` 中的 binary 读取（记录数 = min(声明面数, 实际完整记录)），返回 `WARNING_PARTIAL` |
| `RebuildShapeFromMesh` | `exchange.cpp::make_original_shape` |
| （无） | `make_sewing_shape`、`merge_shape` |
| `OnExportStep` | `stl2stp_export_step`（`export_mesh(..., step=true)`） |
| `OnExportIges` | `stl2stp_export_iges`（`export_mesh(..., step=false)`） |
| `SimplifyTriangulation(theKeepRatio, std::function<bool(int)>)` | `stl2stp_simplify(target_faces, callbacks)`（`simplify.cpp`） |
| `CExportProgressDlg::SetStage/IsCanceled` | `stl2stp_callbacks` + `Progress` / `Indicator` |
| 100000 面确认框（`ExportStrings.h::kWarnBigMesh`） | 不迁移，由 UI 层自行决定 |
| 硬编码 `VINCENT` / `STL2STP 1.0.0` / `STL TO STEP FILE` / `STL2STP * processor` | `stl2stp_export_options` 的 `author/organization/product/description/timestamp`，默认空（时间为当前 UTC） |
