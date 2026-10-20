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
| `stl2stp_default_export_options` | 返回 `sewing_tolerance=1e-6`、`merge_coplanar=0` |
| `stl2stp_load_stl` | 自动识别 binary/ASCII STL，精确坐标去重，成功后替换网格 |
| `stl2stp_get_mesh_stats` | 返回 unique vertices、无向 unique edges、triangular faces 和 AABB |
| `stl2stp_simplify` | QEM edge collapse，按目标三角面数简化当前网格 |
| `stl2stp_save_stl` | 写 little-endian binary STL；法向由顶点绕序重新计算 |
| `stl2stp_export_step` | Sewing 后写 STEP AP214IS，可返回导出形状的 topology |
| `stl2stp_export_iges` | 同一 B-Rep 管线写 IGES，可返回导出前 B-Rep topology |
| `stl2stp_read_step` | STEP read-back，返回 unique topology counts，不改变当前网格 |

`stl2stp_mesh_stats` 的 `minimum[3]` / `maximum[3]` 顺序均为 x/y/z。
`stl2stp_topology` 的 solids/shells/faces/edges/vertices 使用 `TopExp::MapShapes`
去重；shells 包含 solid 内的 shell，**不能**把 solids 与 shells 相加作为组件数。
导出时 `topology` 可为 `NULL`；read-back 时必须提供输出地址。
失败或取消不写入 stats/topology 输出结构体。

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
任意位置的 NaN/Inf 坐标会失败，不静默接受；零面积三角面跳过并报告 warning。
坐标焊接仅使用 exact equality，**不是**公差聚类；统计包含三角面内部的对角线。

QEM 迁移了 reference `MeshSimplify.cpp` 的平面 quadrics、3×3 最优点求解、
端点/中点回退、greedy priority queue、normal-flip guard 和每条边最多 16 次延迟重试。
目标从 keep-ratio 改为正整数 `target_faces`；闭合模型一次 collapse 通常删除两面，
因此可低于目标一面。候选耗尽时返回带诊断的最佳非空结果，不承诺所有目标可达。
使用 vertex generation 检查移除过期 heap entries、按 edge id 打破等价 cost，
并明确评估真正的 midpoint（reference 的 `mid=best` 并非总是真正的中点）。
这些改进意味着不保证与原桌面版逐顶点/逐字节相同的简化结果。
normal-flip guard 不是非流形输入修复器，也不承诺任意模型严格保拓扑。

## Sewing 与导出边界

所有 triangle 的原始绕序保留，以 `MakePolygon` / `MakeFace` 构造平面 face，
然后使用 `BRepBuilderAPI_Sewing(tolerance, true, true, true, false)`。
默认 tolerance 是 **1e-6 模型坐标单位**；可由 export options 覆盖为有限正数。
`merge_coplanar` 默认关闭；开启时使用 `ShapeUpgrade_UnifySameDomain` 合并 faces/edges，
不拟合曲面。闭合 shell 在 OCCT 可定向时构造 solid，开放 shell 保持 shell，
多个组件放在 compound 中，不强行把不相交组件放进一个 shell。
STL 没有单位字段，当前按输入数字直接作为 mm 写出 STEP/IGES；不自动缩放。
IGES 按 OCCT `write.brep.mode` 默认值工作。返回的 IGES topology 是导出前的 B-Rep，
不是 IGES read-back（公共接口只提供 STEP read-back）。

**与提供的 reference 不一致之处必须明确：** 实际检查的
`stl2stp-codeDoc.cpp::RebuildShapeFromMesh` 使用共享边、手工单 shell + solid 组装，
没有 `BRepBuilderAPI_Sewing`，也没有 Sewing tolerance。任务描述中的
“与原始 Sewing 相同公差”在该源码中不存在可对照值；本库按任务要求新增 Sewing，
1e-6 是本库明确默认值，不能宣称已复刻原始 Sewing 参数。
不迁移 MFC UI、渲染、注册表、显示降采样、undo 和硬编码作者/company/header 文案。

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
因为 `Interface_Static` 是进程全局设置。STEP 参数操作后恢复。外部直接使用 OCCT 的调用
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
