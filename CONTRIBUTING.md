# 协作约定

1. 只在本地 git 工作，禁止添加 remote、禁止 push。
2. 每个任务在独立分支开发：core/*、app/*、ios/*、validate/* 等，完成后合并到 main（流程见 docs/HANDOFF.md §3）。
3. 所有源码统一 UTF-8（无 BOM），换行 LF。不要沿用原仓库的 GBK 编码。
4. core 库不得依赖 MFC / Windows API，只依赖 C++17 标准库与 OpenCASCADE。
5. 对外接口集中在 core/include/stl2stp/，以 C 风格接口暴露，方便 Flutter（dart:ffi）和 iOS 原生代码调用。
6. 每个功能必须带测试，测试数据放 core/tests/data/（大文件不入库）。
7. reference/ 目录只读，不入库。
8. 提交身份统一用 `致知屋 <zhizhiwu@local>`（或接手者自己的身份），提交信息和文件里不写个人信息。

## 模块分工
- 核心库：从 reference 中 stl2stp-codeDoc.cpp / MeshSimplify.cpp 抽取逻辑 -> core/
- 构建与测试：CMake、OCCT 依赖、回归与性能测试；OCCT iOS 交叉编译
- App 界面：Flutter（app/），iOS 原生部分需要 Mac
