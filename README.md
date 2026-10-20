# STL2STP iOS

手机端 STL 转 STEP/IGES 工具。参考原 MFC 桌面版 (gitee zigzagwag/stl2stp)。

- `core/`  跨平台 C++ 核心库 stl2stp-core（STL 读取、QEM 简化、缝合导出 STEP/IGES），CMake 构建，含 CLI 与测试
- `ios/`   SwiftUI App + Objective-C++ 桥接（待有 Mac 后开发）
- `docs/`  设计与任务文档
- `reference/` 原桌面版源码（只读参考，不入库）

仅本地 git 管理，暂不添加远程仓库。
