# 原始 STL → B-Rep → STEP / IGES 转换取证

## 证据范围

本文的 `Doc.cpp` 指只读文件
`reference/stl2stp-master/stl2stp-code/stl2stp-code/stl2stp-codeDoc.cpp`。
行号按原文件计数，未修改 reference。实际文件是 **UTF-8 with BOM / CRLF**，
不是 GBK；GBK `iconv` 在 BOM 处失败后改为直接读取。部分早期注释已经乱码，
不把乱码推断成代码行为。`ExportStrings.h` 位于同一目录。

这个 reference 自身包含标为“新 UI / P4”的改动和原版逆向函数名注释。
下述结论是对所给源码和只读 OCCT 8.0.1 helper 的静态取证，
不是运行历史 Windows EXE 得出的逐字节等价证明。
不把显示降采样、STEP 预览、QEM 简化误认为 STL 导出必经步骤。

## 1. STL 读取、共享节点与过滤

`Cstl2stpcodeDoc::LoadStlAndDisplay`（Doc.cpp:632）调用 `RWStl::ReadFile`（Doc.cpp:754–755），
得到一个 `Poly_Triangulation`。当没有读到三角面时调用
`ReadStlTolerant`（Doc.cpp:776–787），不是正常文件的一律替代读取器。
`ReadStlTolerant` 定义于 Doc.cpp:454，只接管实际完整 records 少于声明面数的 binary 输入，按实际 records 读取；
binary STL 一个 record 为 50 bytes，84-byte header 后才有数据（Doc.cpp:461–483）。
它跳过文件法向的 12 bytes，读取三点坐标（Doc.cpp:500）。

只读 OCCT 8.0.1 源码基目录记为 `$HOME/stl2stp-work/occt-src/OCCT-8.0.1/src/`：

- `DataExchange/TKDESTL/RWStl/RWStl.hxx:61–69`：默认 merge angle 为 `M_PI/2`，
  意思是焊接节点时忽略相邻面法向夹角，不是 90° 面片简化阈值。
- `RWStl_Reader.cxx:45–82`：`MergeNodeTool` 使用 `Poly_MergeNodesTool`，
  merge tolerance 为 `0.0`，最后仅在三个节点索引互异时调用 `AddTriangle`。
- `FoundationClasses/TKMath/Poly/Poly_MergeNodesTool.cxx:275–280,342–365`：
  `myToDropDegenerative=true`，`myToMergeElems=false`。退化检测是节点重复，
  **不是三角形面积/叉积检测**；默认不合并重复三角形。
- 其坐标键是 `NCollection_Vec3<float>`：binary STL 本来就是 float32；
  ASCII 输入也以 float32 键比较，保留首次节点的 double 坐标。
  因此“精确焊接”指该键相等，不能声称任意 ASCII double 位型不同就不合并。
- `RWStl.cxx:149–160` 的 `Reader` 有意忽略读取失败 bool，返回已读数据；
  没有三角面时 `GetTriangulation()` 返回 null。普通单结果 overload 不是
  `MultiDomainReader` 的“每个 solid 分别返回”的 overload。

`ReadStlTolerant` 用 `unordered_map<tuple<float,float,float>,int>` 焊接节点
（Doc.cpp:494–512）；同样只检查三个索引不同（Doc.cpp:514–515）：

```cpp
if (anIdx[0] != anIdx[1] && anIdx[1] != anIdx[2] && anIdx[0] != anIdx[2])
    aTris.push_back(Poly_Triangle(anIdx[0], anIdx[1], anIdx[2]));
```

结论：重复顶点导致的零面积面被过滤；**三个不同节点的共线零面积面不被明确过滤**。
重复三角形保留，包括同向副本；不同节点坐标是否被焊接由读取器的键决定。
重建阶段没有另外的顶点距离聚类、去重面、法向一致化或面积阈值。
共线面最终能否构造/写出由 OCCT 决定，不保证所有共线输入都成功。
原法向不用于重建面方向；绕序来自 `Poly_Triangle`。

## 2. `RebuildShapeFromMesh` 的准确顺序

函数定义在 Doc.cpp:1611–1783。导出调用不传显示网格/显示三角化开关，
使用完整 `m_hTriang`，写入 `m_rebuiltShape`（Doc.cpp:1616–1620、1870、1991）。

1. 创建 `BRep_Builder`（Doc.cpp:1637）。每个网格节点调用
   `BRepBuilderAPI_MakeVertex(gp_Pnt)`，保存共享 `TopoDS_Vertex`
   （Doc.cpp:1639–1653）。不是每个三角面新建三套顶点。
2. 顺序扫描三角形的 `(a,b)/(b,c)/(c,a)`。用 64-bit `(b << 32) | a`
   保存首次遇到的边方向；同向查询给正索引，反向给负索引
   （Doc.cpp:1655–1696）。去重的是 **无向节点对边**，不是重复三角面。
3. 对每条唯一边调用 `BRepBuilderAPI_MakeEdge(sharedVertexA, sharedVertexB)`
   （Doc.cpp:1698–1713）。共享边实体跨面复用。
4. 每三角形取三条共享边，只在第一条带负号时 `Reverse()`；第二、三条只取
   绝对索引，由 `BRepBuilderAPI_MakeWire` 连接/决定边方向。
   然后 `BRepBuilderAPI_MakeFace(aMakeWire.Wire())`
   （Doc.cpp:1715–1730）。**没有 `BRepBuilderAPI_MakePolygon`**。
   不能“修正”为人为反转所有负索引边，否则不再照原始构造顺序。
5. `BRep_Builder::MakeShell`，把所有 faces 放入 **一个** `TopoDS_Shell`，
   调用 `BRepTools::Update(aShell)`（Doc.cpp:1768–1773）。
   `BRep_Tool::IsClosed` 仅记录日志，不参与条件分支。
6. 无条件 `BRep_Builder::MakeSolid`，把该 shell 加入 **一个**
   `TopoDS_Solid`（Doc.cpp:1775–1780）。不调用 `OrientClosedSolid`。

没有显式 `BRepBuilderAPI_Sewing`、`ShapeFix_*`、共面合并、曲面拟合或连通域拆分。
显示分支挂载 normals/`Poly_Triangulation`（Doc.cpp:1731–1755）仅用于呈现，
导出没有该分支。`BRepTools::Update` 不是 Sewing/闭洞操作。

### 多组件与开放网格

两个分离 cube 仍然先被装入一个 shell/solid，不构造 compound 或两个独立 solids。
开放网格也无条件装成 solid；这是一个拓扑容器，不是“模型确为闭合有效实体”的承诺。
原函数不调用 `BRepCheck_Analyzer` 验收模型有效性。
OCCT writer / reader 自带处理可能拆分连通域、移除退化边或把无效 solid 降级为 shell。
因此“传给 writer 的拓扑”与“STEP read-back 拓扑”必须分别统计。
在本次 OCCT 8.0.1 回归中，two-shell 输入导出前为 1 solid / 1 shell，回读为 2 / 2；
截断开放输入导出前也为 1 / 1，回读为 0 solids / 2 shells。
这不能倒推成原函数有主动分壳逻辑。

## 3. 公差与精度：显式值和 helper 默认值分开

`RebuildShapeFromMesh` **没有传入任何显式建模公差值**，没有 Sewing tolerance，
也没有 `SetTolerance`、`ShapeFix::SetPrecision` 或 `STEPControl_Writer::SetTolerance`。
不能凭 `#include <Precision.hxx>`（Doc.cpp:32）猜出应用显式设置了 `1e-6`。

| 项目 | 所给应用设置 | OCCT 8.0.1 默认/实现（不是历史 OCCT 7.0 实测） |
| --- | --- | --- |
| STL 节点 merge tolerance | `RWStl` 默认；fallback 精确 float 键 | `0.0` |
| STL merge angle | `RWStl` 默认 | `M_PI/2`，忽略角度 |
| 新建 vertex / edge 建模精度 | 未设置 | `BRepLib::Precision()` 初值 `Precision::Confusion()=1e-7` |
| wire → face 寻面公差 | 未传公差，`MakeFace(Wire)` | `BRepLib_FindSurface(W,-1,OnlyPlane,true)`；`-1` 为从边容差推导，不是负几何精度 |
| 实际 face tolerance | 未设置 | `max(1.2 * ToleranceReached(), Tolerance())`，可能随输入变化 |
| STEP/IGES `write.precision.mode` | 未设置 | `0` / `Average`；`Min=-1, Max=1, User=2` |
| `write.precision.val` | 未设置 | `1e-3`；不能把它当作 Average 模式下一律使用的输出公差 |
| Sewing / coplanar merge tolerance | 原函数没有这些步骤 | 不存在可复刻的应用常数 |

helper 证据：`ModelingAlgorithms/TKTopAlgo/BRepLib/BRepLib.cxx:84`、
`BRepLib_MakeVertex.cxx:26–30`、`BRepLib_MakeFace.cxx:189–252` 和
`DataExchange/TKXSBase/Interface/Interface_StaticStandards.cxx:68–76`。
`MakeFace` 内部会调用 `BRepLib::UpdateTolerances`、`SameParameter`；
应用没有给这些调用自定义数值。`Precision.hxx` 的基础常量还包括
`Angular=1e-12`、`Intersection=1e-9`、`Approximation=1e-6`，
它们是 OCCT 内部常量，不是本应用分别设置的导出精度选项。
精确写出公差还取决于形状与全局 OCCT 配置，不能从应用这几行代码列出一个固定结果。

## 4. 大面数确认不是自动简化

`OnExportIges`（Doc.cpp:1811–1816）和 `OnExportStep`（Doc.cpp:1938–1942）
都检查当前 `m_hTriang->NbTriangles() > 100000`，**恰好 100000 不提示**。
检查发生在保存文件对话框之前，计数不是 STL header 声明面数或显示降采样面数。
提示文本是 `ExportStrings.h:22–24` 的 `expstr::kWarnBigMesh`，建议先用工具栏
黄色 J 按钮简化，然后问：

> 是否继续（“是”：继续转换当前STL模型，“否”：停止转换）？

`IDNO` 直接返回，不选文件、不重建、不写出、不自动跳入简化器。
“是”继续原来的完整当前网格；仍可在后续保存对话框取消。
没有额外转换模式选择，没有“是=简化 / 否=保留”的含义。

本库/CLI 没有交互确认、没有隐含 100000 面上限、没有自动简化，相当于选择继续。
调用者应按自己的内存/时限策略提前确认或拒绝大模型。
CLI 的 `--simplify N` 必须显式提供，才先执行 QEM；这不是上述提示的替身。

## 5. STEP 写出

`OnExportStep` 定义于 Doc.cpp:1924，重建后执行（Doc.cpp:2005–2030）：

1. 从用户所选路径取 UTF-8 文件名 `aName`。
2. 创建 `STEPControl_Writer`。
3. `Interface_Static::SetCVal("write.step.product.name", "STL2STP 1.0.0")`。
4. `Transfer(m_rebuiltShape, STEPControl_AsIs, true)`；`true` 是 compgraph 参数。
5. `Model(false)` → `APIHeaderSection_MakeHeader`，设置下表字段。
6. `Write(UTF8 full path)`。原 handler 未检查 Transfer/Write 返回状态。

| STEP 字段 | 值（Doc.cpp:2015–2027） |
| --- | --- |
| name | 最终输出 basename（包含扩展名） |
| author / organization | `VINCENT` / `VINCENT` |
| originating system | `STL2STP` |
| description | `STL TO STEP FILE` |
| preprocessor version | `STL2STP STEP processor` |

没有显式设置 `write.step.schema`、`write.step.unit`、`write.precision.mode`、
`write.precision.val` 或时间戳；其它文件 header 项保持 writer 的默认值。
当前 OCCT 8.0.1 `STEPControl_Controller.cxx:87–94,151–167` 默认 schema 为
`AP214IS`、unit 为 `MM`；历史桌面进程中的全局配置/OCCT 版本不能仅由本 handler 证明。
不要把 AP214IS / MM 写成这个 handler 主动设置的参数。
STL 没有单位字段；数字按 writer 的模型单位解释，原 handler 没有输入缩放逻辑。
OCCT transfer 的内部 shape processing 也不能描述成应用显式调用了 `ShapeFix`。

## 6. IGES 的差异

`OnExportIges` 定义于 Doc.cpp:1796；重建同样的 `m_rebuiltShape` 后
（Doc.cpp:1880–1911）：`IGESControl_Controller::Init()` →
读取 `Interface_Static::IVal("write.brep.mode")` 和
`CVal("write.iges.unit")` → `IGESControl_Writer(unit, mode)`。
复制 writer model 的 `IGESData_GlobalSection`，写回 6 个字段：

| 字段（Doc.cpp:1893–1906） | 值 |
| --- | --- |
| author name / company name | `VINCENT` / `VINCENT` |
| file name | 最终输出 basename |
| interface version / send name / system id | 三者都是 `STL2STP IGES processor` |

然后 `AddShape` → `ComputeModel` → `Write(path, false)`。
`false` 为非 FNES 写法，不是“不要写几何”。没有 STEP schema、STEP product 参数或
`APIHeaderSection_MakeHeader`，也不主动设置 precision mode/value。
同样没有检查 AddShape/Write 的返回值。

**参数名歧义保留：** 所给 handler 确实写的是 `write.brep.mode`，
但 OCCT 8.0.1 注册的是 `write.iges.brep.mode`
（`DataExchange/TKDEIGES/IGESData/IGESData.cxx:90–94`，默认 `0=Faces`；`1=BRep`）。
8.0.1 在不存在的旧键上 `IVal` 返回 0，故本库保留原字面查询的可观察行为，
不擅自将它改为受新键控制的不同模式。
当前 `write.iges.unit` 默认 MM（同文件:75–88）；原进程是否另设该旧键无证据。

## 7. 本次 ORIGINAL 迁移边界与 OCCT 8.x 替代

- `MakeVertex` → 共享无向 `MakeEdge` → `MakeWire`（只显式反转第一条边）→
  `MakeFace` → 单 shell / 单 solid 的顺序及 `BRepTools::Update` 保留；
  默认不合并共面，不 Sewing，不按闭合性改装配，不删除重复三角形。
- 公共 STL reader 仍独立于 OCCT，保留旧 SEWING 的面积过滤视图；另外保存被面积检查
  排除的源 facets 及源顺序，让 ORIGINAL 不误过滤共线面，且不丢失退化 facet
  先出现的节点坐标（共享坐标键必须保留首次出现的点）。
  ORIGINAL 对未简化源网格调用 OCCT 自己的 `Poly_MergeNodesTool(M_PI/2,0.0)`，
  复现 `RWStl` 的 float32 坐标键。QEM 产生新网格后使用它的现有共享节点索引，
  **不再次按 STL float32 键焊接**。本库 QEM 本来就不承诺与 reference 数值一致。
- `unsigned __int64` / `ui64` 的 Windows 类型/后缀改为 C++17 节点对键和
  `size_t` 索引，边仍按首次出现顺序生成，不按 map 排序生成。
- 所给 reconstruction 已经使用可用于 8.0.1 的 `BRep_Builder`，
  不需要再把它替换成 MakePolygon、Sewing 或 ShapeFix。
  原注释中的历史手工 `TShell/TSolid` 不是本次 reference 的可执行代码。
- 拓扑统计用 `NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher>`，
  避免 8.x deprecated alias；8.x 异常读 `Standard_Failure::what()`，
  7.x 条件分支保留 `GetMessageString()`。这些是统计/诊断兼容，不改变构造算法。
- STEP `Model(false)`、header setter、IGES GlobalSection、Transfer/AddShape/Write
  的 API 在 8.0.1 仍有效，无需改名。仅增加 progress range/cancellation。
  ORIGINAL 不覆盖 schema/unit/precision，STEP product 改动在操作后恢复，
  header 的 name 使用最终 basename，而不是库原子写出的临时文件名。
- 原 handler 的未检查返回值改为 C status 错误；增加有限坐标/float32 key 范围/
  OCCT int 索引范围检查、事务写入及取消，不复刻崩溃、未定义读取或“失败仍显示成功”。
  所有退化/重复顶点文件仍无有效三角面时失败；只有共线面时可载入带 warning，
  ORIGINAL 尝试原式构面，SEWING/QEM/STL save 返回 FORMAT。
- `merge_coplanar` 是独立新增选项，两模式都可用；其默认 tolerance `1e-6`
  不是原函数的建模 tolerance。开启后已不要求保持原始逐三角面拓扑。
- 不迁移 MFC/注册表/渲染/显示采样/UI 确认。无法保证历史 OCCT 7.0 与当前 8.0.1
  的内部处理、动态公差、实体排序、时间戳或整个文件逐字节相同。

可复核 topology 断言和实际 Release / Debug UBSan 结果见 `docs/core-validation.md`。
