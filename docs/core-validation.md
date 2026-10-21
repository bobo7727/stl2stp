# 核心库验证交接（2026-10-20）

## 当前：core/weld（基于 `1385bb5`，2026-10-21）

WSL Ubuntu 20.04，Release，g++-10，OCCT 8.0.1，`ctest -j4`：**122/122 通过**（新增 `weld_open_pole`、`weld_guards`）。
开放极点 sphere50k：焊接开 1 solid（焊接 199 个顶点），关 0 solids。耗时与峰值内存（max RSS）见
`docs/core-api.md` “core/weld”；峰值内存：ORIGINAL 约 19–20 kB/三角形 + ~45 MB（5k 140 MB，20k 437 MB，
50k 1016 MB，102k 2017 MB），SEWING 约 +5%，简化 50% 约按输出面数。

## 历史：时间戳与 STEP Unicode（基于 `c0a553a`）

branch `core/extract`，未 commit。`stl2stp_export_options` 最后追加 `timestamp`，
NULL 每次取当前 UTC（STEP `YYYY-MM-DDTHH:MM:SSZ`），非 NULL 严格校验后在 STEP
逐字保留，空字符串无效；IGES 参数 18/25 均为 `15HYYYYMMDD.HHNNSS`。
覆盖值保留墙钟时间，IGES 去掉时区/小数秒；四个文本 metadata 仍默认空。
CLI 新增 `--timestamp`；调用方须用新版头文件重新编译。

STEP 写出 BMP X2 / 非 BMP X4，大写 hex；单引号由 OCCT 加倍一次，字面反斜杠
受保护后归一化为两个反斜杠。OCCT 72 列换行可能拆开长字符串的 directive，
流式 string-token 归一化已消除这种断裂，发布仍通过原子事务。
OCCT 8.0.1 MakeHeader / PRODUCT 回读中文、emoji、U+20BB7、单引号、反斜杠、
字面 directive 及长字符串均无损；不需要自定义 reader 解码。
IGES 继续 UTF-8 byte-count Hollerith，既有纯中文 Start 行尾裁剪限制未改。

保留全部 85 个测试，新增 19 个 CTest，共 104 个（58 个 metadata label）。
两模式 × 两格式覆盖默认日期正则及距当前 UTC 小于 180 秒、精确覆盖/IGES 转换、
覆盖→默认及默认→覆盖隔离、Gregorian 闰年/非法日期/时间/offset/空值拒绝且目标不变，
callback 修改 timestamp 后私有副本仍有效、四组合 CLI 覆盖回读及缺值/重复/非法 flag。
编码测试独立比较实际 X2/X4/引号/反斜杠 bytes 和 OCCT 回读，含 ASCII 不变、
字面 directive、跨行长文本和非法 UTF-8；IGES 另通过 `IGESModel::VerifyCheck`。
字段语义/格式与 setter 行为已核对只读 OCCT 8.0.1 源码；原 IGES 5.3 PDF 下载被
DNS 与 approval reviewer 阻断，未声称原规范逐条审核或第三方 CAD 认证完成。

构建仍使用 gcc-10/g++-10；TMPDIR、构建/日志/数据均在本仓库 build/。
Debug C/C++ 均开启 `-Wall -Wextra -Wpedantic -Werror -fsanitize=undefined -fno-sanitize-recover=undefined`；
只读 OCCT 仍为既有 Release 共享库，未对 OCCT 自身追加 UBSan。
最终记录：`build/core-linux/ctest-timestamp-validation.log` 与
`build/core-linux-debug-ubsan/ctest-timestamp-validation.log`。

Release：

```text
100% tests passed, 0 tests failed out of 104

Label Time Summary:
mesh        =   2.50 sec*proc (10 tests)
metadata    =  25.16 sec*proc (58 tests)
occt        =  31.01 sec*proc (54 tests)

Total Test time (real) =  47.98 sec
```

Debug `-Werror` + UBSan：

```text
100% tests passed, 0 tests failed out of 104

Label Time Summary:
mesh        =   7.83 sec*proc (10 tests)
metadata    =  31.36 sec*proc (58 tests)
occt        =  33.47 sec*proc (54 tests)

Total Test time (real) =  49.22 sec
```

## 历史基线：可配置空 metadata（基于 `7d961fe`）

以下 85 项摘要与 raw UTF-8 / 空日期描述是历史证据；当前日期/编码行为以上节及
`docs/core-api.md` 为准。

四个 UTF-8 指针 `author`、`organization`、`product`、`description` 追加在 options 尾部，
默认 NULL；两模式 × STEP/IGES 均去掉原版及 OCCT 可控的默认 metadata，几何算法未改。
历史章节的硬编码 header 观察不是当前默认行为。branch 仍为 `core/extract`，未 commit。

保留全部 46 个既有测试，新增 39 个 metadata CTest，共 85 个。
覆盖四组合默认空/中文自定义、OCCT Header/PRODUCT/IGES GlobalSection 回读、连续两个
不同值后跟空字符串及 NULL defaults（包括 options=NULL）、非 ASCII basename、
调用中 callback 修改 caller 字符串后的私有副本、取消后恢复 ambient Interface_Static、
两个独立 handle 的 ORIGINAL/SEWING 并发调用、IGES 195 字节边界及 CR/LF 拒绝、
四组合 CLI 中文 flags 与缺值/重复 flags。生成数据都位于各 build/tests/data/。

STEP 是原始 UTF-8（不是自动 X2），OCCT 默认 UTF8 codepage 可回读；
IGES Global 是 UTF-8 字节数 Hollerith，中文 author/company/product 无损回读。
纯中文 IGES Start description 写出字节正确，但 OCCT signed-char 行尾裁剪后回读为空；
这是明确保留的外部 OCCT 8.0.1 限制，测试未用成功返回码掩盖该数据损失。
空 timestamp/date 的严格格式合规性和其它 CAD 的兼容性未认证。
准确字段清单、已清空和仍保留的结构值见 `docs/core-api.md`。

构建使用 gcc-10/g++-10、只读 OCCT 8.0.1；所有生成文件、TMPDIR、日志都在仓库 build/，
未写入 home、reference 或全局配置。Release 与 Debug `-Werror` + UBSan 最终摘要如下。
Debug C/C++ flags 均为 `-Wall -Wextra -Wpedantic -Werror -fsanitize=undefined -fno-sanitize-recover=undefined`。
OCCT 是现有 Release 共享库，未重建或对 OCCT 自身新增 UBSan instrumentation。

Release，`build/core-linux/ctest-metadata-validation.log`：

```text
100% tests passed, 0 tests failed out of 85

Label Time Summary:
mesh        =   0.61 sec*proc (10 tests)
metadata    =   2.54 sec*proc (39 tests)
occt        =   3.68 sec*proc (46 tests)

Total Test time (real) =   7.46 sec
```

Debug `-Werror` + UBSan，`build/core-linux-debug-ubsan/ctest-metadata-validation.log`：

```text
100% tests passed, 0 tests failed out of 85

Label Time Summary:
mesh        =   1.97 sec*proc (10 tests)
metadata    =   3.81 sec*proc (39 tests)
occt        =   5.01 sec*proc (46 tests)

Total Test time (real) =  10.35 sec
```

## 双模式验证（当前工作，基于 `9ec394a`）

默认已经改为 `STL2STP_MODE_ORIGINAL`，因为 reference 的导出重建没有 Sewing。
`STL2STP_MODE_SEWING` 保留前版管线；`merge_coplanar` 在两模式独立可选。
接口与旧二进制 bridge 必须一起重新编译，不能把新增 mode 的尾部字段当成旧 ABI 承诺。
原始构造顺序、所有参数的显式/默认边界、100000 面 UI 语义以及 OCCT 8.x 替代
见 `docs/original-conversion.md`，使用方法见 `docs/core-api.md`。

本次保持 `core/extract` / HEAD `9ec394a`；没有 commit、remote、push、checkout、
reset 或 stash。所有新代码、文档、fixture、构建/测试日志写在仓库内，
OCCT install/source 仅只读，未触碰其它 home 项目。以下历史 25 项/性能数据对应
**前版默认 SEWING**，不能用来代表本次 ORIGINAL；新增性能数据见下文“独立复测”。

### 模式测试范围

保留原有 25 项，新增两模式各 8 个 CTest case，以及 5 个 CLI mode case，共 46 项。
`original_*` / `sewing_*` 分别检查 cube、two-shells、退化+重复 cube、
手写重复面小夹具、共线面、只有共线面、截断输入和 IGES。
cube / two-shells 的每个 case 同时检查 merge OFF / ON。

- `core/tests/data/small_zero-area-duplicate.stl` 是小型手写 ASCII fixture：
  一个有效 facet、一个重复顶点零面积 facet、一个与首面相同的副本。
  两模式都过滤重复节点 facet，保留 2 个重复有效面，不擅自去重三角面。
- `degenerate-duplicate.stl` 由 generator 产生：12 面 cube + 重复顶点面 + 首面副本。
  从 14 records 保留 13 faces；ORIGINAL 的共享边计数必须仍为 18。
- `collinear.stl` 是 cube 加一个三点不同的共线面。ORIGINAL 保留它交给 OCCT，
  SEWING 保持面积过滤；不能把“零面积”一律误读为 reference 会跳过。
- `collinear-only.stl` 验证 loader 保留 ORIGINAL 可用的源视图，clean stats 为 0 faces；
  ORIGINAL 在本 fixture 上成功导出，SEWING/QEM/STL save 返回 FORMAT。
  SEWING 的失败不替换既有目标文件或 topology，不残留临时目录。
- truncated binary 声明 12 faces，只有 5 个完整 records + 17 bytes 尾部；
  load 必须返回 PARTIAL，继续导出并回读 5 faces，不能静默当作完整 cube。
- ORIGINAL 的 STEP 检查原 product/author/description/preprocessor、最终 basename，
  并检查文件中仅一个 `CLOSED_SHELL`：两不相交 cube 和开放输入也不能被构造器
  主动改成多 shell 的 SEWING 算法。这里的实体名不构成模型有效性证明。
- 两模式 IGES 都验证 80-column records、sections/序号/counters；ORIGINAL 另检查
  `VINCENT`、processor 和最终 filename。公共 API 尚未增加 IGES geometry read-back。
- C99 header case 检查默认 ORIGINAL、枚举值及 4-byte enum；exchange errors 检查
  非法 mode=99 安全返回 ARGUMENT。第一次 UBSan 发现非固定 enum 的非法值读取，
  已通过 C++ `int32_t` underlying type 修复，而不是去掉非法值测试或关闭 sanitizer。
- CLI 检查省略 mode 的 default ORIGINAL、显式 original/sewing 输出模式名，
  unknown/missing/duplicate mode 均失败。保留 readback、merge、simplify 和 IGES CLI case。
- 原有取消 case 现在对两模式分别在 0 / 0.45 / 0.8 / 1 请求取消 STEP/IGES，
  验证文件/topology 不变、无临时目录；不声称无 progress hook 的 OCCT 阶段可即时取消。

### OCCT 8.0.1 拓扑断言

每项按 `solids / shells / faces / edges / vertices` 计数；回读列是本安装的
实际 STEP reader 结果，不是简单复制 export 计数。无 merge 除非另行标注。

| fixture | mode | export S/Sh/F/E/V | STEP read-back S/Sh/F/E/V |
| --- | --- | --- | --- |
| cube | 两模式 | 1/1/12/18/8 | 1/1/12/18/8 |
| cube，merge ON | 两模式 | 1/1/6/12/8 | 1/1/6/12/8 |
| two-shells | ORIGINAL | 1/1/24/36/16 | 2/2/24/36/16 |
| two-shells | SEWING | 2/2/24/36/16 | 2/2/24/36/16 |
| two-shells，merge ON | ORIGINAL | 1/1/12/24/16 | 2/2/12/24/16 |
| two-shells，merge ON | SEWING | 2/2/12/24/16 | 2/2/12/24/16 |
| degenerate-duplicate cube | ORIGINAL | 1/1/13/18/8 | 1/2/13/18/8 |
| degenerate-duplicate cube | SEWING | 1/1/13/21/11 | 1/2/13/21/11 |
| small duplicate ASCII | 两模式 | 1/1/2/3/3 | 1/1/2/3/3 |
| cube + collinear | ORIGINAL | 1/1/13/20/9 | 1/2/13/20/9 |
| cube + collinear | SEWING | 1/1/12/18/8 | 1/1/12/18/8 |
| collinear-only | ORIGINAL | 1/1/1/3/3 | 0/1/1/2/2 |
| collinear-only | SEWING | FORMAT，不写出 | 不适用 |
| truncated binary | ORIGINAL | 1/1/5/12/8 | 0/2/5/12/8 |
| truncated binary | SEWING | 0/2/5/12/9 | 0/2/5/12/9 |

ORIGINAL 的 faces 对应原始节点重复过滤后存活的 facets（merge OFF），
不是 core clean stats 的面积过滤面数。共享 edges/vertices 与单 shell/solid 来自
reference 的明确构造；回读拆壳、降级和退化边处理来自 OCCT，不能“修复”测试期望
来掩盖两模式构造算法不同。上述不是对非法/非流形几何的正式有效性验收。

### 本次构建与日志

真实 OCCT 8.0.1 shared libraries；gcc-10 / g++-10（GCC 10.5.0），C++17。
Release 使用 `build/core-linux`；Debug 使用 `build/core-linux-debug-ubsan`，
C/C++ flags 均为 `-Wall -Wextra -Wpedantic -Werror -fsanitize=undefined
-fno-sanitize-recover=undefined`。预编译 OCCT 本身没有重新插桩。
本次配置/构建/CTest 命令的 `TMPDIR` 都指向仓库 `build/tmp`，没有设置全局环境。

```sh
mkdir -p build/tmp
TMPDIR="$PWD/build/tmp" cmake -S core -B build/core-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DOCCT_ROOT="$HOME/occt-install" -DSTL2STP_WITH_OCCT=ON
TMPDIR="$PWD/build/tmp" cmake --build build/core-linux --parallel 4
TMPDIR="$PWD/build/tmp" ctest --test-dir build/core-linux --output-on-failure -j 6

TMPDIR="$PWD/build/tmp" cmake -S core -B build/core-linux-debug-ubsan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -fsanitize=undefined -fno-sanitize-recover=undefined" \
  -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic -Werror -fsanitize=undefined -fno-sanitize-recover=undefined" \
  -DOCCT_ROOT="$HOME/occt-install" -DSTL2STP_WITH_OCCT=ON
TMPDIR="$PWD/build/tmp" cmake --build build/core-linux-debug-ubsan --parallel 4
TMPDIR="$PWD/build/tmp" UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/core-linux-debug-ubsan --output-on-failure -j 6
```

完整最终日志（gitignored）：`build/core-linux/ctest-modes-validation.log` 和
`build/core-linux-debug-ubsan/ctest-modes-validation.log`。

Release 最终摘要：

```text
100% tests passed, 0 tests failed out of 46

Label Time Summary:
mesh    =   1.60 sec*proc (10 tests)
occt    =   3.67 sec*proc (23 tests)

Total Test time (real) =   2.00 sec
```

Debug `-Werror` / UBSan 最终摘要（无编译 warning 或 sanitizer 诊断）：

```text
100% tests passed, 0 tests failed out of 46

Label Time Summary:
mesh    =   2.74 sec*proc (10 tests)
occt    =   3.21 sec*proc (23 tests)

Total Test time (real) =   2.29 sec
```

## 历史基线：25 项完整 OCCT 回归（前版默认 SEWING）

分支仍为 `core/extract`。未 commit、未添加 remote、未 push，未修改 reference。
已有 `.gitattributes` / `.gitignore` / 协作约定文件 / `README.md` 工作区改动保留原样。
现有 `.gitignore` 已排除整个 `build/`，无需再修改。

构建时约定只读 `$HOME/occt-install` 与 `$HOME/stl2stp-work/occt-src`，
前次路径限制导致的阻塞已经解除。未向这两个目录写入，也未访问其它 home 项目。
使用真实 OCCT 8.0.1 shared libraries 编译、链接 `exchange.cpp`，
**完整 Release 构建零编译错误，25/25 CTest PASS**；未使用 disabled stub。

## 已确认结果

- CMake 3.26.4；g++-10 / gcc-10，GCC 10.5.0；C++17。
- OCCT 的安装 header/config 版本均为 8.0.1；只读校验 official V8.0.1 archive
  SHA256 为 `6297cc55a1720523a437c54d07dc16b9da8c5f5ab5800da3c6ebded568d53c18`。
- `build/core-linux`：Release，`STL2STP_WITH_OCCT=ON`，25/25 CTest PASS。
- `build/core-linux-check`：完整 Debug + `-Wall -Wextra -Wpedantic -Werror`
  + `-fsanitize=undefined`，25/25 PASS，无编译 warning 或 UBSan 诊断。
  sanitizer 覆盖本项目 core/CLI/tests，不代表预编译 OCCT 的内部代码也被 instrumented。
- 前次 mesh-only 的 Release / Debug 子集仍保留在各自 build directory，
  但不再作为完整验收的替代证据。
- 包含纯 C header/ABI、binary/ASCII、binary solid header、截断、统计、QEM、
  STL round-trip、UTF-8 路径、取消事务、数值范围与 CLI 参数回归；
  STEP/IGES export、AP214 header、STEP read-back 与 exchange 取消均已实际运行。
- 默认 UV sphere：19,800 faces / 9,902 vertices / 29,700 edges；
  简化到 2,000 faces / 1,002 vertices / 3,000 edges，Euler characteristic 为 2。
- 新源码/文档 UTF-8、无 BOM、LF；无 trailing whitespace；`git diff --check` 通过。
- 所有持久代码、文档、fixture 和构建输出均在仓库内。
- `nm` 确认 11 个 `stl2stp_*` public functions 为 unmangled C symbols。

Release CTest 摘要（最终复测，子进程显式移除 `LD_LIBRARY_PATH`）：

```text
100% tests passed, 0 tests failed out of 25

Label Time Summary:
mesh    =   2.72 sec*proc (10 tests)
occt    =   1.70 sec*proc (7 tests)

Total Test time (real) =   7.94 sec
```

完整运行日志为 `build/core-linux/ctest-validation.log`（gitignored）。

完整 Debug / UBSan 摘要：

```text
100% tests passed, 0 tests failed out of 25

Label Time Summary:
mesh    =   2.99 sec*proc (10 tests)
occt    =   1.20 sec*proc (7 tests)

Total Test time (real) =   7.15 sec
```

## 实际拓扑与文件结果

STEP topology 为导出前与实际 read-back 的共同结果：

| 模型 | merge | solids | shells | faces | edges | vertices |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 单位 cube | OFF | 1 | 1 | 12 | 18 | 8 |
| 单位 cube | ON | 1 | 1 | 6 | 12 | 8 |
| 两个不相交 cube | OFF | 2 | 2 | 24 | 36 | 16 |
| 两个不相交 cube | ON | 2 | 2 | 12 | 24 | 16 |

另外运行 CLI 生成 `build/core-linux/evidence-*`：unmerged cube STEP **26,038 bytes**，
merged cube STEP **16,080 bytes**，two-shells STEP **53,846 bytes**，cube IGES **21,141 bytes**。
文件大小会随 OCCT metadata/文件名/版本变化，这些是一次实际生成的值，不是恒定断言。

IGES 检查 80-column records、S/G/D/P/T section 顺序、每 section 序号、termination
counters 与实际 record count 一致。OCCT 8 使用 `T0000001` 等零填充序号；
测试不再错误地假定空格填充。IGES geometry read-back 不在公共 API/本次验收范围内。

取消测试覆盖 load、QEM、STL save、STEP/IGES export 与 STEP read-back。
exchange 在 progress 0 / 0.45 / 0.8 / 1 请求取消，检查既有目标文件和 topology
输出不变、无临时目录遗留；STEP read-back 取消不改变网格或输出计数。
无 progress hook 的 OCCT 阶段仍只能在调用前后取消，不能宣称可即时打断。

## 已修复的兼容与加载问题

- `NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>` 替代 deprecated alias；
  OCCT 8 异常诊断使用 `what()`，7.x 分支保留 `GetMessageString()`。
- Linux 链接器默认 `DT_RUNPATH` 不覆盖 OCCT 的间接 `.so` 依赖，曾导致
  `libTKDE.so.8.0` 找不到；改为 Linux-only `--disable-new-dtags` + `DT_RPATH`。
  实际 CLI ELF 的 RPATH 指向所选 `OCCT_LIBRARY_DIR`，没有设置 `LD_LIBRARY_PATH`。
- 按约定保留 DataExchange imported targets 的 transitive dependencies，
  不因 `TKV3d` / `TKService` 间接依赖拒绝配置；core 仍不使用 visualization API，
  不直接选择这些 libraries。

直接选择 toolkits：`TKDESTEP TKDEIGES TKXSBase TKBool TKBO TKPrim TKShHealing
TKTopAlgo TKGeomAlgo TKBRep TKGeomBase TKG3d TKG2d TKMath TKernel`，本次还有 `TKDE`。
`ldd` 实测共解析 25 个 OCCT shared toolkits，额外闭包为
`TKXCAF TKLCAF TKCAF TKCDF TKVCAF TKV3d TKService TKMesh TKHLR`。
无 unresolved libraries，不含 `TKOpenGl` / OpenGL / FreeType / X11 libraries。

实际验证的是 Linux + OCCT 8.0.1；OCCT 7.9 和 iOS static builds 尚未独立构建验证。

## 历史基线的原始逻辑差异（本次 ORIGINAL 已补齐构造及 metadata）

- 实际 reference 使用手工单 shell/solid，不含 Sewing 或 Sewing tolerance；
  按任务需求新增 per-triangle Sewing（默认 1e-6），不能声称此参数忠实复刻原始值。
- 多个组件分别生成 shell/solid；开放 shell 不强行转 solid。
- QEM 保留原算法主要逻辑，但改为 target face count、过期候选检查、确定性 tie-break、
  真正 midpoint 回退与数值范围检查，不保证逐顶点/逐字节一致。
- MFC UI、渲染、注册表、undo 与原始硬编码作者/header 文案不迁移。

接口、取消边界、线程模型和 iOS 接入见 `docs/core-api.md`；
official archive 校验、OCCT 构建步骤 与 core 构建命令见 `docs/build-linux.md`。

## 独立复测（2026-10-20，WSL Ubuntu 20.04，12 核，Release，g++-10，OCCT 8.0.1）

从零重新配置构建 `build/core-linux-verify`：构建 47 s，0 个 warning；`ctest` 25/25 通过。
夹具由 `stl2stp-fixture-generator` 生成；时间是 CLI 自报的 `export_ms`（包含缝合、可选的共面合并和 STEP 写出）。

| 输入 | 面数 | 选项 | 导出 ms | STEP 字节 | 回读 faces/edges/vertices | 峰值内存 |
|---|---:|---|---:|---:|---|---:|
| cube | 12 | raw | 42 | 26,038 | 12/18/8 | 40 MB |
| cube | 12 | --merge-coplanar | 43 | 16,080 | 6/12/8 | 40 MB |
| two-shells | 24 | raw | 48 | 53,846 | 24/36/16（2 solids） | 40 MB |
| two-shells | 24 | --merge-coplanar | 56 | 33,830 | 12/24/16（2 solids） | 40 MB |
| UV sphere | 19,800 | raw | 17,328 | 51,183,093 | 19800/29700/9902 | 467 MB |
| UV sphere | 19,800 | --merge-coplanar | 17,233 | 50,391,239 | 19408/29308/9902 | 462 MB |
| UV sphere | →9,900（QEM 50%，31 ms） | raw | 8,171 | 25,547,311 | 9900/14850/4952 | 253 MB |
| UV sphere | →9,900 | --merge-coplanar | 8,117 | 25,547,311 | 9900/14850/4952 | 253 MB |
| UV sphere | 19,800 | IGES | 14,203 | 40,219,416 | - | 271 MB |

结论：
- 平面零件合并共面面片效果明显（cube 体积减少 38%）；曲面模型（球）几乎没有共面三角形，合并只减少 1.5%。
- STEP 约 2.6 KB/面，导出约 0.87 ms/面，基本呈线性；先做 QEM 简化是减小体积最有效的手段。
- 2 万面的 STEP 回读需要 10–12 s。在 `/mnt/d` 上单次测到过 32 s，属于 I/O 波动。
- OCCT 的 STEP writer 会在 stdout 打印带颜色的 Transfer 统计；iOS 集成前应通过 `Message` 静默掉。

## 独立复测：ORIGINAL 与 SEWING 对比（2026-10-20，WSL Ubuntu 20.04，12 核，Release，g++-10，OCCT 8.0.1）

在 `build/v2-release` 和 `build/v2-debug-ubsan` 从零构建：Release 0 warning，46/46 通过；Debug `-Werror` + UBSan（`-fno-sanitize-recover`）46/46 通过，0 条 `runtime error`。

### 19,800 面 UV 球（输出写到 WSL ext4 `~/stl2stp-work/bench`，3 次交替运行）

| 模式 | 导出 ms（3 次） | STEP 字节 | 回读 ms | 回读 solids/shells/faces/edges/vertices | 峰值内存 |
|---|---|---:|---:|---|---:|
| ORIGINAL | 3885 / 4170 / 4331 | 48,554,145 | 6930–7217 | 1/1/19800/29700/9902 | 448 MB |
| SEWING | 8401 / 8492 / 8874 | 51,183,093 | 7280–7603 | 1/1/19800/29700/9902 | 468 MB |

ORIGINAL 导出约快 2 倍（省掉 Sewing），文件约小 5%（同一 shape 复用同一组边/顶点）。

### 其它组合（输出写到 `/mnt/d`，单次运行；/mnt/d 写入较慢，耗时只用于看量级）

| 输入 / 选项 | ORIGINAL：导出 ms，字节，回读 faces | SEWING：导出 ms，字节，回读 faces |
|---|---|---|
| 球 + `--merge-coplanar` | 8388，47,867,342，19408 | 16967，50,391,239，19408 |
| 球 + `--simplify 9900` | 5219，24,255,046，9900 | 8469，25,547,311，9900 |
| 球 → IGES | 9579，40,219,416，峰值 175 MB | 14095，40,219,416，峰值 271 MB |
| cube | 27，24,429，1 solid / 12 faces | 44，26,038，1 solid / 12 faces |
| two-shells | 34，47,445；导出 1 solid / 1 shell，回读 2 solids / 2 shells | 65，53,846；导出、回读均为 2 solids |
| zero-area + duplicate | 18，5,458，2 faces | 31，5,457，2 faces |
| truncated-binary（5 个完整面） | 28，12,360；导出 1 solid，回读 0 solids / 2 shells | 22，15,887；导出、回读均为 0 solids / 2 shells |

说明：
- ORIGINAL 和桌面版一样，把所有面放进一个 shell 和一个 solid，所以 two-shells 导出时只有 1 个 solid。OCCT STEP reader 回读时会按连通域拆成 2 个；开放网格（截断文件）回读时 solid 会降级成 shell。
- 本节历史版本的 ORIGINAL STEP 头部写入原版元数据：作者和组织为 `VINCENT`，产品名为 `STL2STP 1.0.0`，描述为 `STL TO STEP FILE`；已被当前默认空/可配置 metadata 决策取代。
