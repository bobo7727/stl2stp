# stl2stp-core API

## 范围与接口

`core/include/stl2stp/stl2stp.h` 是纯 C（C99 可编译）的公共接口。
实现使用 C++17 标准库和 OpenCASCADE；不包含 MFC、Windows API 或可视化模块。
Objective-C++ bridge 只需包含此头文件并持有 opaque `stl2stp_handle*`。
所有路径采用 UTF-8、NUL 结尾；调用时必须保持字符串、callback context 有效。

| English identifier | 含义 |
| --- | --- |
| `stl2stp_create` | 创建空 handle；失败返回 `NULL` |
| `stl2stp_destroy` | 销毁 handle；`NULL` 安全 |
| `stl2stp_last_error` | 读取上次诊断，不清空；`NULL` 读取当前线程的创建/null-handle 诊断 |
| `stl2stp_default_export_options` | 返回 `mode=STL2STP_MODE_ORIGINAL`、`sewing_tolerance=1e-6`、`merge_coplanar=0`，四个文本 metadata 和 `timestamp` 指针均为 `NULL` |
| `stl2stp_load_stl` | 自动识别 binary/ASCII STL，精确坐标去重，成功后替换网格 |
| `stl2stp_get_mesh_stats` | 返回 unique vertices、无向 unique edges、triangular faces 和 AABB |
| `stl2stp_simplify` | QEM edge collapse，按目标三角面数简化当前网格 |
| `stl2stp_save_stl` | 写 little-endian binary STL；法向由顶点绕序重新计算 |
| `stl2stp_export_step` | 按 ORIGINAL / SEWING 模式写 STEP，可返回导出前的 topology |
| `stl2stp_export_iges` | 同一 B-Rep 管线写 IGES，可返回导出前 B-Rep topology |
| `stl2stp_read_step` | STEP read-back，返回 unique topology counts，不改变当前网格 |

`stl2stp_mesh_stats` 的 `minimum[3]` / `maximum[3]` 顺序均为 x/y/z。
`stl2stp_topology` 的 solids/shells/faces/edges/vertices 使用 `TopExp::MapShapes`
去重；shells 包含 solid 内的 shell，**不能**把 solids 与 shells 相加作为组件数。
导出时 `topology` 可为 `NULL`；read-back 时必须提供输出地址。
失败或取消不写入 stats/topology 输出结构体。
**导出前与 STEP 回读的计数不保证一致**：writer/reader 可修复/拆分无效或不连通拓扑。

## 返回码与错误处理

- `STL2STP_OK`（0）：成功；下一次返回 status 的操作会先清空旧诊断。
- `STL2STP_WARNING_PARTIAL`（1）：成功载入可用部分；文件截断、尾部不匹配、
  ASCII 语法不完整或零面积三角面被跳过。调用者应显示 `last_error`，不能当作完整模型。
- `STL2STP_WARNING_TARGET_NOT_REACHED`（2）：正常结束并保留最佳简化结果，但未达到目标。
- `STL2STP_ERROR_ARGUMENT`（-1）：null handle/必需输出、空路径、零目标面数、非法公差。
- `STL2STP_ERROR_IO`（-2）：输入/输出打开、读取、写入或发布失败。
- `STL2STP_ERROR_FORMAT`（-3）：无完整有效三角面、非有限坐标、STL 范围溢出或无可转移 STEP。
- `STL2STP_ERROR_EMPTY`（-4）：需要网格的操作在空 handle 上调用。
- `STL2STP_ERROR_CANCELLED`（-5）：取消；原网格、目标文件及返回 topology 不变。
- `STL2STP_ERROR_OCCT`（-6）：OCCT 构面/缝合/转移异常或 mesh-only build 无 exchange 支持。
- `STL2STP_ERROR_MEMORY`（-7）/ `STL2STP_ERROR_INTERNAL`（-8）：内存或其它实现异常。

正返回码是带诊断的成功，负返回码是失败。所有 API 阻止 C++ 异常穿越 C ABI。
`last_error` 返回库管理的字符串指针，不能 free；下次修改此 handle 的诊断或 destroy
后指针失效。需要长期保留应立即拷贝。传入无效/已销毁的非 NULL 指针属于调用者错误。

```cpp
#include <stl2stp/stl2stp.h>

stl2stp_handle* mesh = stl2stp_create();
if (mesh) {
    stl2stp_status status = stl2stp_load_stl(mesh, "model.stl", nullptr);
    if (status >= STL2STP_OK) {
        stl2stp_export_options options = stl2stp_default_export_options();
        options.merge_coplanar = 1;
        stl2stp_topology topology = {};
        status = stl2stp_export_step(mesh, "model.step", &options, nullptr, &topology);
    }
    stl2stp_destroy(mesh);
}
```

实际 bridge 应检查每个 status，并在下一次操作前读取 warning/error。
export 的 `options=NULL` 等价于 default options；不要使用全零 options（公差 0 非法）。

## STL 与 QEM 语义

Binary STL 读取声明面数和文件长度，最多取两者容许的完整 50-byte records。
不足 84-byte header 或没有完整有效面不产生伪网格；`solid` 开头的 binary
通过长度与非文本字节识别，不能只按首词判定 ASCII。
ASCII 使用 classic locale，完整 `endfacet` 才接纳一个 facet，支持多个 `solid`。
没有 `endsolid`、半个 facet 或非法尾部保留完整前缀并报告 warning。
任意位置的 NaN/Inf 坐标会失败，不静默接受；零面积三角面从非退化视图中跳过并报告 warning。
原始 facet 顺序另保留供 ORIGINAL 焊接使用；重复顶点面由原式索引过滤，不写入 B-Rep。
三个不同节点的共线面在 ORIGINAL 不作面积过滤。
只有共线面时仍能带 warning 载入，但 mesh stats 的 faces/edges 为 0，
ORIGINAL 会尝试构面，SEWING、QEM 和 STL save 返回 `STL2STP_ERROR_FORMAT`。
`vertices` 和 AABB 包含读取保存的节点（可能有仅由共线面引用的节点），
`faces`/`edges` 统计非退化视图，不能把 stats.faces 当成 ORIGINAL 一定写出的面数。
坐标焊接仅使用 exact equality，**不是**公差聚类；统计包含三角面内部的对角线。

QEM 迁移了 reference `MeshSimplify.cpp` 的平面 quadrics、3×3 最优点求解、
端点/中点回退、greedy priority queue、normal-flip guard 和每条边最多 16 次延迟重试。
目标从 keep-ratio 改为正整数 `target_faces`；闭合模型一次 collapse 通常删除两面，
因此可低于目标一面。候选耗尽时返回带诊断的最佳非空结果，不承诺所有目标可达。
使用 vertex generation 检查移除过期 heap entries、按 edge id 打破等价 cost，
并明确评估真正的 midpoint（reference 的 `mid=best` 并非总是真正的中点）。
这些改进意味着不保证与原桌面版逐顶点/逐字节相同的简化结果。
normal-flip guard 不是非流形输入修复器，也不承诺任意模型严格保拓扑。

## Conversion mode 与导出边界

```c
typedef enum stl2stp_mode {
    STL2STP_MODE_ORIGINAL = 0,
    STL2STP_MODE_SEWING = 1
} stl2stp_mode;
```

`stl2stp_export_options` 顺序为 `sewing_tolerance`、`merge_coplanar`、`mode`，
再在末尾追加 `const char* author`、`organization`、`product`、`description`。
C++17 声明固定 underlying type 为 `int32_t`，使任意 C 传入的 32-bit 枚举值
都可以安全验证，不先触发 UBSan 的 invalid enum load；C99 端使用普通 enum，
C header 测试检查其大小为 4 bytes。不要用 `-fshort-enums` 编译 bridge。
未知 mode 返回 `STL2STP_ERROR_ARGUMENT`，不改变文件或 topology。
这不是旧已编译客户端的 ABI 兼容承诺：结构体大小已经改变；旧客户端没有初始化
新增字段，**库和 bridge 必须一起重新编译**。
建议总是使用 `stl2stp_default_export_options()` 初始化，不手工猜结构体布局。

**默认是 ORIGINAL，不是 SEWING**，因为所给桌面导出算法共享节点/边并直接
组装一个 shell/solid，没有 Sewing。`options=NULL` 也使用 ORIGINAL。
`merge_coplanar` 是两模式独立可选项，默认关闭。

### ORIGINAL (`STL2STP_MODE_ORIGINAL`)

复现 reference 的 `MakeVertex` → 共享无向 `MakeEdge` → `MakeWire`（仅显式反转
第一条带负号的边）→ `MakeFace` → `BRep_Builder` 单 shell → `BRepTools::Update`
→ 无条件单 solid 顺序。不 Sewing，不自动按组件分壳，不强制修复面方向。
未简化 STL 的 ORIGINAL 视图使用 `RWStl` 相同的 `Poly_MergeNodesTool(M_PI/2,0.0)`
及 float32 键焊接；简化结果直接用自己的共享节点，不再次按 STL 键焊接。
只过滤重复节点的退化面，保留节点不同的共线面和重复三角形，交给 OCCT 构面。
若 OCCT 构造失败返回 `STL2STP_ERROR_OCCT`，不能保证任意共线/非法网格都成功。
额外检查有限坐标、float32 key 范围和 OCCT int 索引范围，超界返回 FORMAT。

开放/断开输入同样先组装单 solid，**不代表它是有效闭合实体**；回读可以变成多个
shells/solids。例如 two-shells 导出前 1 solid / 1 shell，OCCT 8.0.1 回读为 2 / 2。
没有隐含 100000 面上限，没有交互确认或自动简化，相当于桌面大面数提示选择“是”。
UI 应自行提示/限流；CLI 只有显式 `--simplify N` 才执行简化。

STEP 保留 `STEPControl_AsIs`、`compgraph=true`；metadata 刻意不复刻原版硬编码，
与 SEWING 一样默认空、由调用者配置，详见后文。
文件名 metadata 使用最终 basename，不使用原子写出的临时文件名。
如 reference 一样，不覆盖 schema/unit/precision；OCCT 8.0.1 的未修改默认值为
STEP AP214IS、STEP/IGES MM、precision mode Average。原进程配置未有证据，
不能声称历史桌面输出精度/单位恒等于这些 8.0.1 默认值。

### SEWING (`STL2STP_MODE_SEWING`)

保留本库此前行为：非零面积三角面按绕序用 `MakePolygon` / `MakeFace` 构造，
`BRepBuilderAPI_Sewing(tolerance,true,true,true,false)`，闭合 shell 在可定向时
构造 solid，开放 shell 保持 shell；多个组件放入 compound。
STEP 显式 AP214IS / MM；IGES 使用 MM。metadata 与 ORIGINAL 使用同一管线。
`sewing_tolerance` 默认 **1e-6 模型坐标单位**，不是 reference 的公差。

### 独立共面合并与 CLI

`merge_coplanar=1` 使用 `ShapeUpgrade_UnifySameDomain` 合并 faces/edges，
不拟合曲面；对 ORIGINAL 已组装的形状或 SEWING 的 sewn shape 应用同一选项。
`sewing_tolerance` 同时用于共面合并；所有模式都要求它是有限正数，
但 ORIGINAL 且 merge 关闭时不用它改变建模公差。
STL 本身没有单位字段，不自动推断或缩放单位。
IGES 保留 reference 的旧键 `write.brep.mode` 查询；8.0.1 未注册此键，`IVal` 返回 0，
并非新版 `write.iges.brep.mode` 可选项；详见 `docs/original-conversion.md`。
IGES topology 是导出前 B-Rep，不是 IGES geometry read-back。

```sh
stl2stp-cli convert in.stl out.step --mode original
stl2stp-cli convert in.stl out.step --mode sewing --merge-coplanar
stl2stp-cli convert in.stl out.step --author '致知屋' --org '测试公司' --product '测试产品' --description '测试描述'
stl2stp-cli convert in.stl out.step --timestamp '2024-02-29T18:52:00.123+08:00'
```

省略 `--mode` 等价于 `original`。CLI 在导出前打印 `mode=original` 或 `mode=sewing`；
未知、重复或缺值选项失败。完整原始行号、隐式公差、prompt 和 API 替代见
`docs/original-conversion.md`；实测拓扑与测试边界见 `docs/core-validation.md`。

## 文件 metadata：文本默认空、日期默认当前 UTC（OCCT 8.0.1 实测）

四个字段都是 UTF-8、NUL 结尾 C 字符串；`NULL` 和 `""` 均表示空，
`options=NULL` 也不会恢复旧硬编码。库在首次 callback 和获取 exchange mutex **之前**
将四个文本和非 NULL timestamp 复制为私有 `std::string`，不保存 caller 指针。调用者只需在同步调用期间
提供有效内存，不必保留到调用之后；不能与最初的复制并发修改这些字符串。
默认文件不含 `VINCENT`、`STL2STP` 或 `STL TO STEP FILE`。

| 字段 | STEP | IGES |
| --- | --- | --- |
| `author` | `FILE_NAME.author` 的唯一列表元素 | Global 参数 21 author |
| `organization` | `FILE_NAME.organization` 的唯一列表元素 | Global 参数 22 company |
| `product` | `FILE_NAME.originating_system`，每个 `PRODUCT.id` / `PRODUCT.name` | Global 参数 3 send/product、5 system id |
| `description` | `FILE_DESCRIPTION.description` 的唯一列表元素 | Start section（IGES Global 没有 description 槽位） |
| `timestamp`（options 最后一个字段） | `FILE_NAME.time_stamp` | Global 参数 18 文件生成日期、25 模型创建/最后修改日期 |

### 时间戳约定

`timestamp=NULL`（含 `options=NULL`）每次导出重新取当前时间，默认采用 UTC：
`YYYY-MM-DDTHH:MM:SSZ`，不是本地无时区时间，也不是可复用的初始化常量。
四个文本 metadata 的默认空行为不变。`timestamp=""` **无效**，返回
`STL2STP_ERROR_ARGUMENT`；本接口不提供空日期 opt-out。

覆盖值接受如下明确的 ISO 8601 extended calendar-date 子集：
`YYYY-MM-DDTHH:MM:SS[.fraction][Z|+HH:MM|-HH:MM]`；四位年份 0001–9999，
Gregorian 日历（含世纪闰年规则），小时 00–23、分/秒 00–59，offset 小时 00–23、
分钟 00–59，小数若存在必须为一个或多个 ASCII 数字。必须大写 `T`/`Z`、必须含秒，
可省略时区（视为 UTC），不接受 basic/week/ordinal date、24:00、闰秒 60、逗号小数或前后空白，
也不接受 `+0800` 这种无冒号 offset。
非 NULL 覆盖值在 STEP 中逐字保留，包括小数位和原 offset；非法语法/日期不发布目标文件。
CLI `--timestamp ISO8601` 使用同一校验，缺值/重复 flag 无效。

IGES 5.3 的日期数据没有 timezone / fractional-second 槽位。本库将覆盖值按 offset
**转换为 UTC**，`Z` 或无 offset 的值已视为 UTC。使用纯 Gregorian 日历算术处理
跨日、跨月、跨年及闰年，不依赖本地时区、环境变量或 `timegm`。小数秒直接截断，不四舍五入，
写成 `15HYYYYMMDD.HHNNSS`；例如 `2026-10-20T18:52:00+08:00` 产生
`15H20261020.105200`，`2024-02-29T18:52:00.123+08:00` 产生 `15H20240229.105200`。
若 UTC 转换超出四位年份 0001–9999，则返回 `STL2STP_ERROR_ARGUMENT`（STEP/IGES 均适用）。
STEP 仍逐字保留原字符串，不改写 offset 或小数秒。默认值取 UTC，两个 IGES 日期字段使用同一次采样。
参数 18 是生成本 IGES 文件的日期；参数 25 是所传模型创建/最后修改的日期，
**不是文件系统 mtime**。STL 输入没有独立模型修改时间来源，所以新建导出模型的
两个字段使用相同值；覆盖值也同时控制两者，不推测输入文件 mtime。

核对 OCCT `IGESData_GlobalSection::{SetDate,SetLastChangeDate,Params}`：setter 接受
原始 `YYYYMMDD.HHNNSS`，内部去除已有 Hollerith 前缀，`Params` 再写出长度 15 的
Hollerith。`IGESData_IGESModel::{Print,VerifyCheck}` 将其明确映射到参数 18/25，
接受 13/15 字节日期；测试同时验证实际参数位置、字节和 OCCT global-section 检查。
`GlobalSection.hxx` 的 `NewDateString` mode 注释与实现的 -1/0 年份长度相反；
本库直接组装四位年，不依赖该有歧义的 mode 注释。
IGES 5.3 原始规范 PDF 未能在线获取（DNS / approval reviewer 阻断）；上述实现契约
已用只读 OCCT 8.0.1 源码核对，不宣称已经完成原规范逐条审核或第三方 CAD 认证。

追加字段改变了 options 的结构大小；调用方必须与更新后的头文件重新编译，
不支持把旧二进制布局传入新版库。建议总是从 `stl2stp_default_export_options()` 初始化。

两格式的文件名均使用最终输出 basename（含扩展名），不使用事务临时路径。
STEP `PRODUCT.description` 不另写 description，保持 writer 的空值。
IGES receiver、interface/preprocessor version、application protocol 都明确设为空，
不是从宿主用户/组织/系统推断。IGES description 不冒用 application protocol 字段。

### 参数名、默认回退和清空范围

核对源文件（相对只读 `OCCT-8.0.1/src/`）：

- `DataExchange/TKDESTEP/APIHeaderSection/APIHeaderSection_MakeHeader.cxx` 默认注入
  `Author`、`Open CASCADE`、`Open CASCADE STEP processor 8.0`、
  `Open CASCADE 8.0`、authorisation `Unknown`、当前 timestamp 和 `Open CASCADE Model`。
  本库在 Transfer 后用 MakeHeader setter 全部覆盖；preprocessor、authorisation
  清空，timestamp 使用上述当前 UTC / 显式覆盖值。
- `DataExchange/TKDESTEP/STEPConstruct/STEPConstruct_ContextTool.cxx::GetProductName`
  在 `write.step.product.name=""` 时回退成 `Product`，并可追加层级序号。
  本库每次设置该全局参数并恢复，同时在 Transfer 后遍历 `StepBasic_Product`
  明确设置 id/name；实测默认是 `PRODUCT('','','',(#8))`，不会残留 `Product` / 序号。
- `DataExchange/TKDEIGES/IGESData/IGESData.cxx` 注册的真实文本参数为
  `write.iges.header.author`、`write.iges.header.company`、`write.iges.header.product`、
  `write.iges.header.receiver`；没有猜测使用 `write.iges.header.system` 等不存在的键。
  author 原默认宿主用户名，product 原默认 `Open CASCADE IGES processor 8.0`。
  本库每次设置上述四个参数并恢复，ComputeModel 后再覆盖模型 GlobalSection。
  实测默认 Global 参数 3、5、6、12、21、22、26 均是空参数；18、25 写入日期。

默认 cube 的 STEP header 示例（日期每次导出变化；两模式相同约定）：

```text
FILE_DESCRIPTION((''),'2;1');
FILE_NAME('default.step','2026-10-20T11:00:00Z',(''),(''),'','','');
FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));
```

**仍然非空的是格式/几何所需值，而不是被迫泄漏的作者或应用 metadata：**

| 范围 | 实测值（cube，未改 OCCT 全局 schema/unit/precision） |
| --- | --- |
| STEP filename | `default.step`（按实际 basename 改变） |
| STEP implementation level / schema | `2;1` / `AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }` |
| STEP AP214 data context | `international standard`、`automotive_design`、`2000`、`core data for automotive mechanical design processes`；PRODUCT_CONTEXT `mechanical`；PRODUCT_DEFINITION `design`；PRODUCT_DEFINITION_CONTEXT `part definition` / `design`；product category `part` |
| STEP geometric context | `2D SPACE`、`Context #1`、`3D Context with UNIT and UNCERTAINTY`；MM length、radian、steradian；uncertainty `1.E-07`，标签 `distance_accuracy_value` / `confusion accuracy` |
| IGES filename（参数 4） | `default.igs`（按实际 basename 改变） |
| IGES 分隔符 | 参数 / record 默认 `,` / `;`（Global 前两个槽位为空表示默认分隔符） |
| IGES 数字能力（7–11） | `32,308,15,308,15` |
| IGES scale / units（13–15） | `1.,2,2HMM`（scale 1，unit flag 2，MM） |
| IGES line weight（16–17） | `1,0.01` |
| IGES resolution / max coordinate（19–20） | `1E-07,1.`（随模型/公差变化） |
| IGES version / drafting standard（23–24） | `11,0`（IGES 5.3，未指定 drafting standard） |

这些结构/单位/精度字段不属于四个文本选项，刻意保留，不通过清空破坏几何语义。
ORIGINAL 仍尊重外部配置的 schema/unit/precision；SEWING 仍为 AP214IS/MM。
作者、组织、产品、描述、处理器版本和授权保持可空；日期现在必须有效且非空。
本次未认证其它 CAD/严格 schema validator，不将 OCCT 回读成功等同于正式互操作认证。

### 中文编码与 OCCT 限制

核对 `StepData_StepWriter::{Send,CleanTextForSend}`：OCCT 8.0.1 不自动把 UTF-8 转成
Unicode directive；会自动加倍普通单引号和反斜杠，但识别到已有 directive 时逐字保留。
STEPControl 注册参数中没有 `write.step.unicode` 等写出 UTF-8 转换开关；
`read.step.codepage=UTF8` 是 reader 参数，不是 writer 编码设置。

本库对 header 文本、最终 basename 和每个 `PRODUCT.id/name` 验证 UTF-8，拒绝 overlong、
截断、surrogate 或超出 U+10FFFF 的序列（ARGUMENT）。BMP 用大写四位 UTF-16 code unit
`\X2\hhhh...\X0\`，非 BMP 用大写八位 scalar `\X4\hhhhhhhh...\X0\`，同类相邻字符
共用一个 directive。示例：`致知屋` → `\X2\81F477E55C4B\X0\`，
`😀𠮷` → `\X4\0001F60000020BB7\X0\`。ASCII 不改变；单引号仅交给 writer 一次，
最终字节为 `''`，不二次转义。

字面反斜杠先作为单独的 `\X2\005C\X0\` 交给 writer，避免 OCCT 把用户文本
`\X2\0041\X0\`、`\N\` 等误识别为 directive；写出后在临时文件的字符串 token 内
流式归一化为标准字节 `\\`。同一流式处理去掉 writer 在长字符串内部插入的物理
CR/LF，防止 directive 或加倍单引号被 OCCT 的 72 列换行拆开。
这不是对 caller 文本作全文件替换；仅处理 writer 的 string token，空间占用有界，
取消/IO 失败仍不发布目标文件，不修改只读 OCCT。用户的字面 STEP directive 可逐字回读。

实测 OCCT 默认 UTF8 reader 对 X2 中文、X4 emoji/U+20BB7、单引号、反斜杠均能还原，
包括 MakeHeader author/organization/description/originating_system 和 PRODUCT id/name，
长字符串和非 ASCII basename 也通过。测试直接比较原 UTF-8 值，不需要 encoded-form
fallback 或自定义解码器；文件字节另以独立的固定 hex 对照验证，避免只测试同一编解码器。

IGES GlobalSection 使用 `TCollection_HAsciiString::Length()`，即 **UTF-8 字节数**，
并保留原始字节：`9H致知屋`、`12H测试公司`，不是 `2H` / `4H`。
IGES 没有 STEP 的 Unicode directive，保留现有 UTF-8 byte-count Hollerith，避免把
STEP 转义当作 IGES 字面字符串，或改用未约定的本地代码页导致中文/非 BMP 损失；
这是当前可无损 OCCT GlobalSection 回读的策略，不保证所有旧 CAD 接受 UTF-8。
固定记录按 72 字节数据 + 8 字节 section/序号切分；Hollerith 可跨记录。
实测 OCCT GlobalSection author/company/send/system id 回读均得到原字符串。
setter 会把开头形如 `6H...` 的输入误当成 Hollerith，本库先包一层长度前缀，
保证用户的字面字符串 `6Hliteral` 也不被剥掉。

OCCT `IGESData_GlobalSection.cxx::Params` 使用 `char text[200]`，`MakeHollerith`
使用无边界 `Sprintf`。因此本库对 IGES author/organization/product/最终 basename
各限制 **195 UTF-8 字节**（195 + `195H` + NUL = 200）；超限返回 ARGUMENT，
不截断、不写文件。STEP 不受此上限限制。
IGES 的四个 metadata 文本和 basename 不允许 CR/LF（ARGUMENT），避免原始换行
破坏 80 字节物理记录；description 无 195 字节上限，但仍须为单行。

IGES description 写在 Start section，原始 UTF-8 保留；但 OCCT 8.0.1 的
`IGESFile/IGESFile_Read.cxx::IGESFile_ReadHeader` 用 signed `char > 32` 去掉行尾空白，
会把行尾高位 UTF-8 字节也去掉。实测纯中文 `测试描述` 写出正确，OCCT 回读 Start
却为空；ASCII description 可正常回读。测试同时断言原始 Start 字节和此 OCCT 限制，
没有伪称中文 Start 能经 OCCT 无损回读。没有修改只读 OCCT 安装/源码。

## Progress、取消与线程

可传 `stl2stp_callbacks`；任一 callback 为 NULL 都允许，整个结构为 NULL 也允许。
`progress(fraction, user_data)` 同步运行在调用线程，fraction 单调且在 [0,1]，
成功操作包含 0 和 1。`cancelled(user_data)` 返回非零要求取消。
加载、QEM、STL 写入在循环中检查；OCCT Sewing、STEP/IGES transfer 使用
`Message_ProgressIndicator` + `Message_ProgressRange`。callback 取消在 indicator 内
转为 `UserBreak`，不会从 OCCT progress 析构中抛异常。

没有 progress hook 的 OCCT `ReadFile`、`UnifySameDomain::Build`、writer `Write`
及 IGES `ComputeModel` 只能在调用前后检查，**不保证即时中断这些阶段**。
progress 的 1 表示结果准备好发布；取消 callback 即使在该时刻要求取消也不会替换目标。
导出及 STL 保存先写同一目标目录下的独立临时目录，成功后 rename 发布；失败清理临时文件，
不删除/覆盖既有目标。缺失目标父目录是 I/O error，不自动创建。

同一 handle 的所有操作（包括读取诊断和 destroy）必须由调用者串行化。
不同 handles 的 mesh 操作可并行；库内部 exchange mutex 串行化所有 OCCT exchange，
因为 `Interface_Static` 是进程全局设置。锁覆盖构面、controller 初始化、所有参数设置、
Transfer/AddShape/ComputeModel、header 修改、Write 和发布，也覆盖 STEP read-back。
每次 export 用 RAII 保存/恢复所修改的 STEP schema/unit/product 和 IGES 四项 header 参数，
成功、取消、异常路径都恢复；先有 controller 初始化，初始化的参数注册仍保留。
不同 handles 的两种格式/两模式导出不能并行进入 OCCT，等待锁时仍轮询取消。
外部直接使用 OCCT 的调用
不受本 mutex 保护，必须由应用额外协调，不能同时修改相关 OCCT 全局参数。
callback 不得重入库或销毁 handle，不得从 C/Objective-C/Swift callback 抛异常；
UI 更新应 dispatch 到 main queue。跨线程取消用原子状态 + callback，不要用存在数据竞争的普通 int。

## CMake 与 OCCT linkage

最低 CMake 3.16，C++17。完整构建默认 `STL2STP_WITH_OCCT=ON`：

```sh
cmake -S core -B build/core-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_C_COMPILER=gcc-10 -DOCCT_ROOT="$OCCT_PREFIX"
cmake --build build/core-linux
ctest --test-dir build/core-linux --output-on-failure
```

`OCCT_PREFIX` 应由调用者设置到获准读取的已安装 OCCT 前缀；本项目不安装系统依赖，
也不自动读写 home。Ubuntu 20.04 + OCCT 8 请使用 g++-10，避免默认 g++ 9.4。
`OCCT_INCLUDE_DIR` 优先用户覆盖，否则从 `OCCT_ROOT/include/opencascade` 或
`OCCT_ROOT/inc` 查找；`OCCT_LIBRARY_DIR` 同理从 lib/lib64 推导。
请用独立 build directory 切换 OCCT 安装，避免 CMake cached 路径混用。

直接选择 toolkits：`TKernel`, `TKMath`, `TKG2d`, `TKG3d`, `TKGeomBase`, `TKBRep`,
`TKGeomAlgo`, `TKTopAlgo`, `TKPrim`, `TKBO`, `TKBool`, `TKShHealing`, `TKXSBase`, `TKDESTEP`, `TKDEIGES`；
安装中有 `TKDE` 时也加入。优先采用安装配置提供的 imported targets 以携带静态依赖，
否则查找明确目录内的 toolkit libraries。仅使用标准 `Threads` 与平台 `CMAKE_DL_LIBS`
辅助 OCCT 运行时。不调用/显式链接 `TKV3d`, `TKOpenGl`, `TKService`；配置时检查
直接选择的 libraries，禁止显式添加这些 visualization toolkits。
按统筹澄清，允许 OCCT DataExchange 自身的 transitive dependencies：实际 OCCT 8.0.1
即使关闭 Visualization / ApplicationFramework modules，仍依赖 `TKXCAF` / `TKCAF`
及 `TKV3d` / `TKService` 等。本库不使用这些 toolkit 的可视化 API；不得删除 OCCT
所需的 imported-target 依赖。实测链接闭包不含 `TKOpenGl` / OpenGL / FreeType。
Linux build/install RPATH 指向选定 `OCCT_LIBRARY_DIR`；使用
`LINKER:--disable-new-dtags` 生成可覆盖间接依赖的 `DT_RPATH`，
避免默认 `DT_RUNPATH` 无法定位 `libTKDE.so.8.0`。不需要改全局 `LD_LIBRARY_PATH`。
自定义静态 OCCT 的额外依赖可用 `OCCT_EXTRA_LIBRARIES` 指定。

iOS 后续：提供基于对应 iPhoneOS 或 Simulator SDK 的 CMake toolchain，
让 `OCCT_ROOT` 指向相同架构/平台的 static OCCT 安装；device/simulator 分开构建。
设置 `BUILD_SHARED_LIBS=OFF`、`STL2STP_BUILD_CLI=OFF`、`BUILD_TESTING=OFF`。
不要将 Linux 或 macOS desktop OCCT libraries 用于 iOS，也不要把多个 SDK 的库混合链接。
最终 App/XCFramework packaging、OCCT 静态依赖及许可证交付仍需在 Mac 上验证。

## 验证分层

CTest fixture executable 在 build directory 生成单位 cube、two-shells、截断/非法输入及
19,800-face UV sphere；生成器可通过 `--rings` / `--segments` 选择规模。
完整测试还要求 cube STEP 12 faces / merged STEP 6 faces，两个 cube 各自为
shell/solid，IGES 文件结构、STEP read-back、AP214 header 和 exchange 取消回归。
CLI 打印各阶段 ms、输出 bytes，convert 打印 B-Rep topology，STEP convert 自动 read-back。

2026-10-20 已使用真实 OCCT 8.0.1 shared libraries 在 `build/core-linux` 完成
Release 构建及 **25/25 CTest PASS**，编译的是 `exchange.cpp`，没有使用 disabled stub。
完整 Debug + `-Wall -Wextra -Wpedantic -Werror` + UBSan 同样 **25/25 PASS**。
单位 cube STEP/read-back 为 1 solid / 1 shell：关闭 merge 时 12 faces / 18 edges / 8 vertices，
开启 merge 时 6 faces / 12 edges / 8 vertices。两个不相交 cube 为 2 solids / 2 shells；
merged/unmerged 的 read-back 均保留组件与拓扑。IGES 校验 80-column records、各 section
序号及 termination counters，接受 OCCT 的零填充序号；这不是 IGES 几何 read-back。
导出、read-back 与 mesh 取消回归均通过。具体证据见 `docs/core-validation.md`，
已用 OCCT recipe 与 Linux 构建说明见 `docs/build-linux.md`。

已针对 OCCT 8 去除 deprecated `TopTools_IndexedMapOfShape` alias，改用
`NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>`；异常诊断在 OCCT 8
使用 `Standard_Failure::what()`，在 OCCT 7.x 使用原 `GetMessageString()`。
实际编译验证版本为 8.0.1，尚未独立验证 7.9 或 iOS。

依赖不可访问时允许**仅验证 mesh subset**：

```sh
cmake -S core -B build/core-mesh-only -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_C_COMPILER=gcc-10 -DSTL2STP_WITH_OCCT=OFF
cmake --build build/core-mesh-only
ctest --test-dir build/core-mesh-only --output-on-failure
```

该模式仅供开发诊断，明确 warning，不执行 OCCT tests，不提供 install target；
exchange API 返回 `STL2STP_ERROR_OCCT`。其通过不能替代完整构建/全部 CTest 验收。
