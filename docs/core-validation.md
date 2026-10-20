# 核心库验证交接（2026-10-20）

## 当前状态：完整 OCCT 构建与回归通过

分支仍为 `core/extract`。未 commit、未添加 remote、未 push，未修改 reference。
已有 `.gitattributes` / `.gitignore` / `AGENTS.md` / `README.md` 工作区改动保留原样。
现有 `.gitignore` 已排除整个 `build/`，无需再修改。

统筹已明确允许只读 `$HOME/occt-install` 与 `$HOME/stl2stp-work/occt-src`，
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
- 按统筹澄清保留 DataExchange imported targets 的 transitive dependencies，
  不因 `TKV3d` / `TKService` 间接依赖拒绝配置；core 仍不使用 visualization API，
  不直接选择这些 libraries。

直接选择 toolkits：`TKDESTEP TKDEIGES TKXSBase TKBool TKBO TKPrim TKShHealing
TKTopAlgo TKGeomAlgo TKBRep TKGeomBase TKG3d TKG2d TKMath TKernel`，本次还有 `TKDE`。
`ldd` 实测共解析 25 个 OCCT shared toolkits，额外闭包为
`TKXCAF TKLCAF TKCAF TKCDF TKVCAF TKV3d TKService TKMesh TKHLR`。
无 unresolved libraries，不含 `TKOpenGl` / OpenGL / FreeType / X11 libraries。

实际验证的是 Linux + OCCT 8.0.1；OCCT 7.9 和 iOS static builds 尚未独立构建验证。

## 原始逻辑的差异

- 实际 reference 使用手工单 shell/solid，不含 Sewing 或 Sewing tolerance；
  按任务需求新增 per-triangle Sewing（默认 1e-6），不能声称此参数忠实复刻原始值。
- 多个组件分别生成 shell/solid；开放 shell 不强行转 solid。
- QEM 保留原算法主要逻辑，但改为 target face count、过期候选检查、确定性 tie-break、
  真正 midpoint 回退与数值范围检查，不保证逐顶点/逐字节一致。
- MFC UI、渲染、注册表、undo 与原始硬编码作者/header 文案不迁移。

接口、取消边界、线程模型和 iOS 接入见 `docs/core-api.md`；
official archive 校验、统筹使用的 OCCT recipe 与 core 构建命令见 `docs/build-linux.md`。

## 统筹独立复测（2026-10-20，WSL Ubuntu 20.04，12 核，Release，g++-10，OCCT 8.0.1）

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