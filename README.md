# STL2STP — STL 转 STEP/IGES 桌面工具（完整交付包）

基于 Visual Studio 2019 MFC 与 OpenCASCADE 的 STL → STEP/IGES 转换工具。
本仓库为完整工程包：**源代码 + 可编译 + 可运行成品 + 产品文档**。

## 目录结构

| 目录 | 说明 |
| --- | --- |
| `stl2stp-code/` | Visual Studio 2019 解决方案（`stl2stp-code.sln`）与 MFC 全部源码 |
| `occt-build/` | OpenCASCADE Release 编译子集：`inc` 头文件 + `win64` 运行库（编译所需） |
| `stl2stp-code-occt/` | 已编译成品：免安装，进入目录双击 `stl2stp-code-occt.exe` 即用 |
| `mobile/` | 跨平台 C++ 转换核心及 Flutter 应用（面向 iOS），含测试、设计稿与移植文档；详见 [mobile/README.md](mobile/README.md) |

移动版从全新克隆开始的构建和验证步骤见 [mobile/QUICKSTART.md](mobile/QUICKSTART.md)。

**产品文档**

- `项目说明.txt` — 目录结构、编译与运行步骤、混合编码注意事项
- `转换能力说明书.txt` — 哪些 STL 能转 / STEP 产物用途 / 最佳流程
- `产品介绍与销售要点.txt` — 商业化定位 / 场景 / 优势 / 模式 / 合规要点
- `性能与使用场景分析.txt` — 性能实测、瓶颈解读、功能全景、适用场景与使用建议

## 编译（VS2019，Release | x64）

1. 需 VS2019 且装有「使用 C++ 的桌面开发」工作负载 + C++ MFC v142 组件；
2. 打开 `stl2stp-code/stl2stp-code.sln`，配置选 **Release | x64**，「生成 → 生成解决方案」；
3. 依赖的 50 个 OpenCASCADE DLL 由工程生成后事件自动拷到输出目录；
4. `stl2stp-code/` 与 `occt-build/` 按相对路径引用，**相对位置不可变动**；
5. 注意：源码为 GBK / UTF-8 混合编码（历史迁移形成），编辑前先确认文件编码。

## 直接运行成品

进入 `stl2stp-code-occt/`，双击 `stl2stp-code-occt.exe`（Windows 10/11 64 位，无需任何安装）。

## 主要功能

- STL 载入：进度对话框、可取消、截断容错；模型统计
- QEM 网格简化（可撤销）
- 导出 STEP / IGES（>10 万面确认框，语义与原版 v1.0.2 一致）
- STEP 纯预览（B-Rep 拓扑统计）
- 深/浅双主题、标准视图自动居中、拖动旋转/平移
