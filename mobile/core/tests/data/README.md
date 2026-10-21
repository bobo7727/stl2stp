# 测试数据

权威 fixture 由 `core/tests/fixtures.cpp` 在 CTest 执行时生成到构建目录
`tests/data/`，不修改本目录，不把生成的 STL、STEP、IGES 或大模型加入 Git。
唯一的小型手写 STL `small_zero-area-duplicate.stl` 留在本目录；CMake 只把它复制到构建目录。

默认生成单位立方体（12 三角面，binary/ASCII）、两个不相交立方体、
截断 binary/ASCII STL、以 `solid` 开头的 binary STL、退化面及非法输入，
以及 UV sphere（100 rings × 100 segments，19,800 三角面，binary/ASCII）。

模式回归新增：
- `small_zero-area-duplicate.stl`（构建副本 `zero-area-duplicate.stl`）：3 个 ASCII facets；一个正常面、一个重复顶点的零面积面、
  一个与首面相同的重复三角形。两模式过滤重复顶点面，但都保留 2 个重复的有效面。
- 生成的 `degenerate-duplicate.stl`：12 面 cube + 1 个重复顶点零面积面 + 首面的副本；
  14 records，加载保留 13 面，不能把重复面当作可自动删除的垃圾。
- 生成的 `collinear.stl`：cube + `(0,0,0)/(0.5,0,0)/(1,0,0)`；共线但节点不同。
  ORIGINAL 保留共线面交给 OCCT，SEWING 保持旧的面积过滤。
  `collinear-only.stl` 只有该共线面，检查 ORIGINAL 可导出而 SEWING/QEM/STL save 拒绝。
- `truncated-binary.stl`：声明 12 面，实有 5 个完整 records + 17 字节尾部；
  带 warning 导出 5 面，不把开放网格当作闭合实体保证。

两模式的 export / STEP read-back topology 断言和 OCCT 自动修复边界见
`docs/core-validation.md`，原始算法证据见 `docs/original-conversion.md`。

性能实验可单独运行：

```sh
build/core-linux/tests/stl2stp-fixture-generator build/core-linux/performance-data --rings 200 --segments 200
build/core-linux/stl2stp-cli simplify build/core-linux/performance-data/sphere.stl build/core-linux/performance-data/reduced.stl --faces 5000
```

`--rings` / `--segments` 范围为 3–1000。默认回归测试的精确统计断言对应
100 × 100；不要覆盖默认 fixture 目录后跳过 CTest 的 fixture setup。
