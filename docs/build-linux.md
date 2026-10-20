# Linux 构建（WSL Ubuntu 20.04 / OCCT 8.0.1）

## 环境与权限

- CMake 3.26.4、Ninja、`gcc-10` / `g++-10` 10.5.0；core 最低要求 CMake 3.16、C++17。
- 统筹已完成 OCCT shared build；本次核心库工作只读取 `$HOME/occt-install`
  和必要时的 `$HOME/stl2stp-work/occt-src`，不向这两个目录写入。
- 源码、core build、CTest fixture、临时文件均放仓库内。不安装系统包，不运行
  `sudo` / `apt` / `pip` / `ldconfig`，不修改 shell profile 或系统配置。
- 不访问其它 home 项目；`reference/` 只读，不入库；不 commit/remote/push。

## OCCT 来源与校验

使用官方 GitHub release **V8.0.1** archive，而非 master 或 8.1-dev：

```text
https://github.com/Open-Cascade-SAS/OCCT/archive/refs/tags/V8.0.1.tar.gz
SHA256: 6297cc55a1720523a437c54d07dc16b9da8c5f5ab5800da3c6ebded568d53c18
```

本次已只读校验 `$HOME/stl2stp-work/occt-src/V8.0.1.tar.gz`，摘要一致；
安装的 `Standard_Version.hxx` 与 `OpenCASCADEConfig.cmake` 均为 8.0.1。
统筹报告 gcc 9.4 在 `Bnd_Box.hxx` 的 constexpr defaulted noexcept constructor
处编译失败，因此改用已安装的 `g++-10` 10.5.0。本次不重新安装或重现 gcc 9.4 问题。
统筹报告 OCCT `ninja -j12` 在 12 cores 上耗时 **1196 s**，install size **123 MB**；
这两个值是此次 OCCT 构建记录，不是 core 测试耗时，也不是通用性能承诺。

## 统筹使用的 OCCT recipe

以下是已完成的构建过程的复现说明，**本次 bot 没有执行这些会写 home 的命令**。
前提是 archive 已校验并解压为 `OCCT-8.0.1`，且编译器/CMake/Ninja 已由统筹准备。

```sh
cmake -S "$HOME/stl2stp-work/occt-src/OCCT-8.0.1" \
  -B "$HOME/stl2stp-work/occt-src/build-linux-release" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc-10 \
  -DCMAKE_CXX_COMPILER=g++-10 \
  -DINSTALL_DIR="$HOME/occt-install" \
  -DCMAKE_INSTALL_PREFIX="$HOME/occt-install" \
  -DBUILD_LIBRARY_TYPE=Shared \
  -DBUILD_MODULE_FoundationClasses=ON \
  -DBUILD_MODULE_ModelingData=ON \
  -DBUILD_MODULE_ModelingAlgorithms=ON \
  -DBUILD_MODULE_DataExchange=ON \
  -DBUILD_MODULE_ApplicationFramework=OFF \
  -DBUILD_MODULE_Visualization=OFF \
  -DBUILD_MODULE_Draw=OFF \
  -DBUILD_DOC_Overview=OFF \
  -DUSE_FREETYPE=OFF \
  -DUSE_TK=OFF \
  -DUSE_TCL=OFF \
  -DUSE_OPENGL=OFF \
  -DUSE_GLES2=OFF \
  -DUSE_XLIB=OFF \
  -DUSE_FREEIMAGE=OFF \
  -DUSE_RAPIDJSON=OFF \
  -DUSE_DRACO=OFF \
  -DUSE_TBB=OFF \
  -DUSE_VTK=OFF

cmake --build "$HOME/stl2stp-work/occt-src/build-linux-release" --parallel 12
cmake --install "$HOME/stl2stp-work/occt-src/build-linux-release"
```

实际 cache 还记录 `BUILD_MODULE_DETools=OFF`；它不是 core 所需模块。
`DataExchange` 即使在 Visualization / ApplicationFramework modules 为 OFF 时，
仍按 toolkit dependency closure 拉入 `TKXCAF` / `TKCAF` / `TKLCAF` / `TKCDF`
及 `TKV3d` / `TKService` / `TKVCAF`。这不代表 core 使用可视化 API，也不能在链接时
强行删除 OCCT 必需的依赖。`TKOpenGl` 未列入本次依赖；没有 OpenGL / FreeType 后端。
`USE_RAPIDJSON=OFF` 时 `TKDEGLTF` 被排除，不应要求它存在。

若另获授权重建 OCCT，并要求所有写入留在仓库，请把 recipe 的 source/build/install
路径分别替换为 `build/occt/OCCT-8.0.1`、`build/occt/build-linux-release`、
`build/occt/install`；两项 install-prefix 参数必须一起更换。不要写入现有只读安装。

## 构建与测试 core

从仓库根目录执行，使用已经完成的 OCCT 安装；`STL2STP_WITH_OCCT=ON` 编译真实
`exchange.cpp`，不是 disabled stub。临时目录环境只影响以下进程，不修改全局环境：

```sh
mkdir -p build/tmp
TMPDIR="$PWD/build/tmp" cmake -S core -B build/core-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-10 \
  -DCMAKE_C_COMPILER=gcc-10 \
  -DOCCT_ROOT="$HOME/occt-install" \
  -DSTL2STP_WITH_OCCT=ON
TMPDIR="$PWD/build/tmp" cmake --build build/core-linux
TMPDIR="$PWD/build/tmp" ctest --test-dir build/core-linux --output-on-failure
```

头文件默认从 `OCCT_ROOT/include/opencascade` 或 `OCCT_ROOT/inc` 查找，libraries
从 lib/lib64 推导，也可以分别设置 `OCCT_INCLUDE_DIR` / `OCCT_LIBRARY_DIR`。
换 OCCT 安装建议另用 build directory，避免复用旧 cache 路径。
所有生成的 STL / STEP / IGES 位于 `build/core-linux/tests/data/`，由 fixture setup
自动生成；不需要从 reference 拷贝模型或把大文件入库。

CLI 示例：

```sh
build/core-linux/stl2stp-cli info build/core-linux/tests/data/cube-binary.stl
build/core-linux/stl2stp-cli simplify build/core-linux/tests/data/sphere.stl build/core-linux/reduced.stl --faces 2000
build/core-linux/stl2stp-cli convert build/core-linux/tests/data/cube-binary.stl build/core-linux/cube.step --merge-coplanar
build/core-linux/stl2stp-cli readback build/core-linux/cube.step
build/core-linux/stl2stp-cli convert build/core-linux/tests/data/cube-binary.stl build/core-linux/cube.igs
```

## Linkage 与 RPATH

core 选择 STEP、IGES、B-Rep、Sewing、ShapeHealing 所需 toolkits；采用 OCCT imported
targets 自动携带真实 transitive dependencies，而不是链接整个 `OpenCASCADE_LIBRARIES`。
core 不包含 Visualization headers，不创建 viewer，不直接选择 `TKV3d` / `TKService`
或 `TKOpenGl`；前两者可以由 OCCT DataExchange 间接依赖引入。

本次 OCCT `.so` 未带自己的 RPATH。Linux 默认 ELF `DT_RUNPATH` 仅覆盖直接依赖，
会造成 `libTKDE.so.8.0` 等间接依赖找不到。core 在 Linux 使用
`LINKER:--disable-new-dtags` 并设置 build/install RPATH 为 `OCCT_LIBRARY_DIR`，
让 ELF `DT_RPATH` 覆盖依赖链；无需修改全局 `LD_LIBRARY_PATH`，也无需 `ldconfig`。
该 linker option 仅在 `CMAKE_SYSTEM_NAME=Linux` 时使用，不传给 iOS。

可检查 ELF 信息：

```sh
readelf -d build/core-linux/stl2stp-cli
ldd build/core-linux/stl2stp-cli
```

具体实测结果、拓扑统计与完整 CTest 摘要见 `docs/core-validation.md`。

## 后续 iOS

OCCT 改为 `BUILD_LIBRARY_TYPE=Static`，使用 iOS toolchain / 对应 SDK / 架构，
device 与 simulator 分开编译。core 同样用该 toolchain，把 `OCCT_ROOT` 指向对应
static OCCT prefix，并设置 `BUILD_SHARED_LIBS=OFF`、`STL2STP_BUILD_CLI=OFF`、
`BUILD_TESTING=OFF`。若没有完整 OCCT imported targets，自定义 archive 的其它依赖
通过 `OCCT_EXTRA_LIBRARIES` 提供；不能把 Linux `.so` 用于 iOS。
App / XCFramework packaging 与 iOS 链接闭包仍需在 Mac 上另行验证。
