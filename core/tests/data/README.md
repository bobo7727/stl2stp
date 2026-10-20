# 测试数据

权威 fixture 由 `core/tests/fixtures.cpp` 在 CTest 执行时生成到构建目录
`tests/data/`，不修改本目录，不把 STL、STEP、IGES 或大模型加入 Git。

默认生成单位立方体（12 三角面，binary/ASCII）、两个不相交立方体、
截断 binary/ASCII STL、以 `solid` 开头的 binary STL、退化面及非法输入，
以及 UV sphere（100 rings × 100 segments，19,800 三角面，binary/ASCII）。

性能实验可单独运行：

```sh
build/core-linux/tests/stl2stp-fixture-generator build/core-linux/performance-data --rings 200 --segments 200
build/core-linux/stl2stp-cli simplify build/core-linux/performance-data/sphere.stl build/core-linux/performance-data/reduced.stl --faces 5000
```

`--rings` / `--segments` 范围为 3–1000。默认回归测试的精确统计断言对应
100 × 100；不要覆盖默认 fixture 目录后跳过 CTest 的 fixture setup。
