# STL2STP

手机端 STL 转 STEP/IGES 工具，基于本仓库的 MFC 桌面版实现重新组织。此目录与桌面版并存，构建入口分别位于 `core/` 和 `app/`。

**首次构建请先读 [QUICKSTART.md](QUICKSTART.md)**（全新克隆、依赖准备、构建、测试和真实转换验收）。维护细节见 [docs/HANDOFF.md](docs/HANDOFF.md)。

- `core/`  跨平台 C++ 核心库 stl2stp-core（STL 读取、QEM 简化、STEP/IGES 导出），默认 ORIGINAL 复现桌面共享边/单壳单体算法，另可选 SEWING；CMake 构建，含 CLI 与测试
- `app/`   Flutter App（iOS 为目标平台，Linux 桌面用于开发和测试），通过 dart:ffi 调用核心库
- `design/ui/`  界面设计稿、文案表、HTML 原型
- `ios/`   占位（iOS 工程在 `app/ios/`）
- `docs/`  设计、验证与交接文档
- `reference/` 原桌面版源码（只读参考，不入库）

协作约定见 [CONTRIBUTING.md](CONTRIBUTING.md)。iOS 构建仍需 macOS、Xcode 及 OCCT iOS 库；步骤见 [docs/mac-setup-guide.md](docs/mac-setup-guide.md)。

GitHub → Codemagic → TestFlight 的配置步骤见 [发布指南](docs/codemagic.md)，工作流位于仓库根 codemagic.yaml。
