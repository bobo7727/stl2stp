# STL2STP

手机端 STL 转 STEP/IGES 工具。参考原 MFC 桌面版 (gitee zigzagwag/stl2stp)。

**接手维护请先读 [docs/HANDOFF.md](docs/HANDOFF.md)**（交接总入口：架构、环境、构建、界面、算法、验证、路线图）。

- `core/`  跨平台 C++ 核心库 stl2stp-core（STL 读取、QEM 简化、STEP/IGES 导出），默认 ORIGINAL 复现桌面共享边/单壳单体算法，另可选 SEWING；CMake 构建，含 CLI 与测试
- `app/`   Flutter App（iOS 为目标平台，Linux 桌面用于开发和测试），通过 dart:ffi 调用核心库
- `design/ui/`  界面设计稿、文案表、HTML 原型
- `ios/`   占位（iOS 工程在 `app/ios/`）
- `docs/`  设计、验证与交接文档
- `reference/` 原桌面版源码（只读参考，不入库）

仅本地 git 管理，不添加远程仓库。协作约定见 [CONTRIBUTING.md](CONTRIBUTING.md)。
