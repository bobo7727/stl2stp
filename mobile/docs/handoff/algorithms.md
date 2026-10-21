# 核心算法

[← 返回总入口](../HANDOFF.md)

> 本页讲“做了什么、为什么”，便于新同事建立整体认识。精确契约（每个边界条件、错误码）以 [core-api.md](../core-api.md) 为准；与原版的逐条差异见 [CHANGES-from-original.md](../CHANGES-from-original.md)；原版代码分析见 [original-conversion.md](../original-conversion.md)。不熟悉 CAD 术语请先看 [术语表](risks-faq-glossary.md#6-术语表)。

## 目录

1. [总体流程](#1-总体流程)
2. [STL 读取](#2-stl-读取)
3. [ORIGINAL 模式](#3-original-模式默认)
4. [SEWING 模式](#4-sewing-模式)
5. [容差焊接](#5-容差焊接weld)
6. [共面合并](#6-共面合并)
7. [QEM 减面](#7-qem-减面)
8. [文件信息（metadata）与 `\X2\` 转义](#8-文件信息metadata与-x2-转义)
9. [UTC 时间戳](#9-utc-时间戳)
10. [导出后校验](#10-导出后校验)
11. [已知的 OCCT 8.0.1 限制](#11-已知的-occt-801-限制)

---

## 1. 总体流程

```text
STL ──读取/精确去重──► Mesh ──(焊接)──(QEM 减面)──► 三角形网格
     ──ORIGINAL：共享顶点/边直接组装 B-rep   ┐
     ──SEWING ：每个三角形一个面，再缝合      ┘──(共面合并)──► TopoDS_Shape
     ──STEP/IGES 写出（流式过滤、元数据、时间戳）──► 文件 ──(STEP 回读校验)
```

每个三角形最终变成一个平面 B-rep 面（`Geom_Plane` + 3 条直线边）。所以输出是“由许多小平面组成的实体”，不是光滑曲面——这是 STL→STEP 转换的本质，见 [FAQ](risks-faq-glossary.md#5-闲鱼买家常见问题support-faq)。

## 2. STL 读取

- 二进制/ASCII 自动识别：`solid` 开头但长度满足 `84 + 50×n` 的是二进制。
- 二进制按声明面数和文件长度取较小值读完整记录；ASCII 只接受完整的 `facet … endfacet`，支持多个 `solid`。
- 截断、尾部不完整、零面积三角形 → 读出可用部分，返回 WARNING_PARTIAL。NaN/Inf 坐标 → ERROR_FORMAT。
- 顶点按 **float32 坐标精确相等** 去重（与原版和 OCCT `RWStl` 一致）。

## 3. ORIGINAL 模式（默认）

复现原版算法：建顶点 → 共享无向边 → 每个三角形一个 wire/face → 放进一个 shell → 一个 solid。不缝合。

`core/fixes` 之后（`strict_original=0`，默认）增加了校验与修复，**对本来就有效、封闭、朝向一致、单一连通的输入不改变任何三角形**（输出与旧版逐字节相同）：

1. 焊点后丢弃面积 < `1e-12 × 对角线²` 的三角形 → REPAIRED。
2. 有边被 ≥3 个三角形共用（非流形）→ 整体改用 SEWING → REPAIRED。
3. 按连通分量用 BFS 统一绕序；不可定向 → 改用 SEWING。封闭分量按有符号体积判断朝向，负的整体翻转 → REPAIRED。
4. 每个封闭分量一个 shell + solid；开放分量只写 shell → NOT_SOLID。多个分量放进 compound（STEP 里是多个 `MANIFOLD_SOLID_BREP` / `SHELL_BASED_SURFACE_MODEL`）。
5. `BRepCheck_Analyzer` 检查结果；无效 → 改用 SEWING → REPAIRED。

`strict_original=1`（CLI `--strict-original`）保留原版逐字节行为（可能写出无效实体），只用于对比，App 不用。

为什么 ORIGINAL 是默认：见 [decisions.md D3](decisions.md#d3-默认转换模式original)。

## 4. SEWING 模式

每个非零面积三角形独立建面（`BRepBuilderAPI_MakePolygon` + `MakeFace`），再用 `BRepBuilderAPI_Sewing(1e-6, …)` 按容差缝合；闭合且可定向的 shell 变成 solid，开放的保持 shell；多个部件放进 compound（每个部件一个 solid）。STEP 显式 AP214IS / MM。实测比 ORIGINAL 慢约 20–25%（不是设计稿里写的“一倍”）。适合：多个零件、有小缝隙、ORIGINAL 校验没通过的模型。

## 5. 容差焊接（weld）

问题：很多建模软件导出的 STL 把同一个点写成相差几个 float ULP 的坐标（典型：UV 球极点 `sin(π) ≠ 0`），精确去重后留下肉眼看不见的缝，ORIGINAL 只能得到开放 shell（NOT_SOLID）。

做法（`core/src/weld.cpp`）：
- 容差 `tol = max(1e-6 × 包围盒对角线, 2 × FLT_EPSILON × 最大坐标绝对值)`（1 m 零件 = 1 µm；后一项是远离原点时的 float32 精度下限）。调用方也可以指定（`weld_tolerance > 0`）。
- 边长 = tol 的均匀网格哈希，查 27 个邻格，贪心并入最近的代表点，整体 O(n)。
- 防护：只有**开放边界上的顶点**参与（封闭部分永远不动，薄壁不会被压扁）；一个顶点不会并入已含它所在三角形其它顶点的簇（**不产生退化三角形**）；索引上限 2^32。
- 生效位置：ORIGINAL 导出时（`Poly_MergeNodesTool` 之前）；`stl2stp_weld_vertices` 供减面前调用；SEWING 不额外焊接。有顶点被焊接时返回 REPAIRED，说明里写“N near-coincident vertex(es) welded (tolerance X)”。
- 开关：默认开（App“合并重合顶点”、CLI `--no-weld`）。耗时影响在单次运行噪声范围内（sphere50k 38.7 s vs 39.8 s）。

为什么默认开：见 [decisions.md D4](decisions.md#d4-焊接默认开)。

## 6. 共面合并

`ShapeUpgrade_UnifySameDomain` 合并同一平面上的相邻面和共线边，不拟合曲面。
- 角度容差 `merge_angular_tolerance` 默认 **1e-4 rad**（原版没设，OCCT 默认 1e-12 rad，斜放的平面几乎合并不了）。
- 线性容差 `max(sewing_tolerance, 1e-6 × 对角线, 4 × FLT_EPSILON × 最大坐标)`，覆盖 float32 STL 的量化误差。
- 合并前后各做一次 BRepCheck：原本有效、合并后无效 → 撤销合并，返回 REPAIRED。
- 效果：平面零件极好（20k 三角形的方块 45 MB → 16 KB，6 个面），曲面模型只省约 35%（UV 球相邻两个三角形共面）。耗时增加约 0.34 ms/三角形。

为什么是这些容差：见 [decisions.md D6](decisions.md#d6-共面合并容差)。

## 7. QEM 减面

基于原版 `MeshSimplify.cpp` 的二次误差度量（Garland–Heckbert）边折叠，做了这些改进（详见 core-api.md “STL 与 QEM 语义”和 CHANGES §9）：
- 面 quadric 按面积加权；开放边界（和非流形边）加垂直约束平面（权重 `1e3 × 边长²`），开放网格不会从边界被“吃掉”。
- 折叠前检查 link condition、顶点度数、两端都在边界但边不在边界时禁止，保证结果是流形、没有重复面。
- 第一阶段要求相邻面法向夹角余弦 > 0.25、三角形质量 ≥ 0.05（或不比原来差）；达不到目标时第二阶段只放宽这两项。
- 数值判断相对于模型尺寸 → 结果与单位无关（测试 `review_qem_scale_invariance`）。
- 目标是绝对三角形数（App 用比例换算）；闭合模型每次折叠删 2 个面，所以结果可能比目标少 1；达不到时返回最好结果 + WARNING_TARGET_NOT_REACHED。
- App 在减面前先焊接（`stl2stp_weld_vertices`），否则微小缝隙会被 QEM 当成边界保护起来，结果仍然不封闭。
- 闭合球减到 50%/25% 仍是 1 个闭合实体（`review_qem_closed_sphere`）。

## 8. 文件信息（metadata）与 `\X2\` 转义

- 四个文本字段（作者、单位、产品、描述）**默认留空**，不再写原版硬编码的“VINCENT / STL2STP / STL TO STEP FILE”，也不写 OCCT 默认的 “Open CASCADE …”。字段对应位置见 core-api.md 的表。
- **STEP**：OCCT 8.0.1 不会自动把 UTF-8 转成 STEP 的 Unicode 指令，我们自己编码：BMP 字符写 `\X2\hhhh…\X0\`（UTF-16，大写四位），BMP 以外写 `\X4\hhhhhhhh…\X0\`。例：`致知屋` → `\X2\81F477E55C4B\X0\`。字面反斜杠先编码成 `\X2\005C\X0\` 交给 OCCT，写出时由流式过滤器 `StepStringFilter` 还原成标准的 `\\`，同时去掉 OCCT 在长字符串中间插入的换行；写盘按 1 MB 分块并检查取消。
- **IGES**：IGES 5.3 字符串是 ASCII，按字节计数的 Hollerith（如 `3H???`）遇到 UTF-8 会被部分读取器读错 → 非 ASCII 字符一律替换为 `?`，返回 METADATA_CHANGED。每个字段（替换后）最多 195 字节（OCCT 内部缓冲区 200 字节）；不允许换行。App 在 Dart 端就去掉了非 ASCII 字符，并在预览页提示。
- 文件名字段用最终 basename（不是临时文件名）。

为什么写“致知屋”作示例作者、为什么默认留空：见 [decisions.md D10](decisions.md#d10-文件信息默认留空示例作者用致知屋)。

## 9. UTC 时间戳

- 不传 `timestamp` 时，每次导出取当前时间，写 **UTC**：STEP `FILE_NAME.time_stamp` = `YYYY-MM-DDTHH:MM:SSZ`；IGES 参数 18（文件生成日期）和 25（模型修改日期）= `15HYYYYMMDD.HHNNSS`。
- 传入覆盖值（ISO 8601 扩展格式子集，见 core-api.md）时，按偏移量换算成 UTC：`2026-10-21T10:30:00+08:00` → STEP `2026-10-21T02:30:00Z`、IGES `15H20261021.023000`。STEP 保留小数秒，IGES 截断。纯公历算术，不依赖本地时区或 `timegm`。
- 非法日期（如 2026-02-29、24:00、无冒号偏移 `+0800`）→ ERROR_ARGUMENT，不写文件。

为什么用 UTC：见 [decisions.md D5](decisions.md#d5-时间戳用-utc)。

## 10. 导出后校验

- CLI `convert` 和 App（STEP 且开了“导出后校验”）在写完后用 `stl2stp_read_step` 回读。
- App 的判定：回读成功 && faces > 0 && (solids ≥ 1 或核心库已经报告 NOT_SOLID)。
- 校验约占 STEP 总耗时的一半（≈0.18 s/MB）。IGES 不校验（OCCT 读 IGES 得到的是曲面集合，拓扑数无法和导出时比较）。
- 回读成功 ≠ 所有 CAD 软件都能正确打开；外部验证见 [validation.md](validation.md)。

## 11. 已知的 OCCT 8.0.1 限制

1. **回读时的朝向检查（readback orientation）**：斜放（旋转）的模型做共面合并后，如果**关闭 OCCT reader 的全部 healing** 再回读，部分平面会报 `BRepCheck_UnorientableShape`；内存中的形状和默认（带 healing）回读都是有效的，FreeCAD 1.0（OCC 7.8.1）读入也有效，纯 Python 检查器 `step_check.py` 的环向/法向检查也通过。测试只断言默认 healing 回读有效。需要用 SolidWorks 等确认（见 [validation.md §4](validation.md#4-solidworks-测试包与待做的-parasolid-检查)）。
2. OCCT 写 STEP 时往 **stdout 打印彩色统计**（`Statistics on Transfer (Write)`），CLI 输出里能看到；App 里无害但浪费，iOS 上应通过 `Message` 静默。**未处理**。
3. LeakSanitizer：OCCT 每次传输在进程退出时留下约 7.6 MB（无本项目栈帧），所以 sanitizer 脚本默认关 LSan。
4. 没有进度钩子的阶段（`ReadFile`、`UnifySameDomain::Build`、IGES `ComputeModel`、writer `Write`）不能立即取消。
5. IGES 写出的是“一组面”，FreeCAD 读入为 0 个实体、BOP 检查报 SelfIntersect——这是 IGES 曲面格式的正常表现，不是错误。
6. 与原版不保证逐字节一致：OCCT 7 → 8 的内部处理、动态公差、实体排序不同；QEM 有改进；时间戳和 metadata 是有意改变的。
7. 只在 OCCT 8.0.1 上编译验证过；代码里留有 7.x 条件分支【未验证】。
