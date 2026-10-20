# 协作约定（给所有 AI 开发 bot）

1. 只在本地 git 工作，禁止添加 remote、禁止 push。
2. 每个任务在独立分支开发：core/*、ios/*、build/*，完成后由统筹合并到 main。
3. 所有源码统一 UTF-8（无 BOM），换行 LF。不要沿用原仓库的 GBK 编码。
4. core 库不得依赖 MFC / Windows API，只依赖 C++17 标准库与 OpenCASCADE。
5. 对外接口集中在 core/include/stl2stp/，以 C 风格或简单 C++ 类暴露，方便 iOS 端 Objective-C++ 桥接。
6. 每个功能必须带测试，测试数据放 core/tests/data/（大文件不入库）。
7. reference/ 目录只读，不入库。

## 分工
- 核心库 bot：从 reference 中 stl2stp-codeDoc.cpp / MeshSimplify.cpp 抽取逻辑 -> core/
- 构建与测试 bot：CMake、OCCT 依赖、回归与性能测试；后续负责 OCCT iOS 交叉编译
- iOS 界面 bot：有 Mac 后启动
