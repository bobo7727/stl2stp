# 验证证据与如何重做

[← 返回总入口](../HANDOFF.md)

## 目录

1. [验证分层](#1-验证分层)
2. [现有证据（2026-10-21）](#2-现有证据2026-10-21)
3. [怎样重做外部校验](#3-怎样重做外部校验)
4. [SolidWorks 测试包与待做的 Parasolid 检查](#4-solidworks-测试包与待做的-parasolid-检查)
5. [什么时候必须重做](#5-什么时候必须重做)

---

## 1. 验证分层

| 层 | 工具 | 证明什么 | 不能证明什么 |
|---|---|---|---|
| 单元/回归 | CTest 122 个 | 每个 API 行为、错误码、元数据字节、拓扑计数、取消/原子写出、修复逻辑 | 别的 CAD 能否读 |
| OCCT 自回读 | `stl2stp_read_step`（CLI `readback`、App 校验） | 文件能被 OCCT 8.0.1 读回、拓扑一致 | 同一套库写和读，可能“自洽地错” |
| 不同版本 OCCT | FreeCAD 1.0.0（OCC 7.8.1）`fc_check.py` | `isValid`、BOP 检查、实体数、体积、壳是否闭合 | 仍然是 OCCT 家族 |
| 独立实现 | `step_check.py`（纯 Python，不用 OCCT） | 引用完整、环闭合、顶点在线/面上、边方向、环与面法向一致、闭壳边使用（可定向）、欧拉数、有符号体积、文件头时间戳与 `\X2\` 解码 | 只支持平面 B-rep |
| 独立实现 | `pcurve_check.py` | 每条平面上的 PCURVE 与 3D 直线一致 | |
| 商业 CAD | SolidWorks（Parasolid 内核） | 真实用户环境 | **待做** |

## 2. 现有证据（2026-10-21）

**CTest**：122/122 通过（Release，gcc-10，OCCT 8.0.1）。见 [core-validation.md](../core-validation.md)。

**FreeCAD 1.0.0 / OCC 7.8.1**（Windows 版 FreeCADCmd，跑 `fc_check.py`）：

| 文件 | valid | BOP | solids | faces | 体积 | 说明 |
|---|---|---|---|---|---|---|
| cube-original.step / cube-sewing.step / cube-default-ts.step | True | ok | 1 | 12 | 1.0 | |
| meta-cn.step（中文元数据） | True | ok | 1 | 12 | 1.0 | |
| two-shells-original/sewing.step | True | ok | 2 | 24 | 1.0, 1.0 | |
| rot4/rot10-original/sewing-merge.step（斜放 + 合并） | True | ok | 1 | 6 | ≈1.0 | 合并在斜面上生效 |
| rot10-original-nomerge.step | True | ok | 1 | 1200 | 1.0 | |
| rot-two-bodies-original-merge.step | True | ok | 2 | 12 | ≈1.0, 1.0 | |
| open-cube-original.step / open-cube-sewing-merge.step | True | ok | 0（1 个开放壳） | 160 / 5 | — | 预期：开放模型写成曲面 |
| sphere-original.step（19,800 面，48 MB） | True | ok | 1 | 19,800 | 4.185 | |
| sphere-sewing-merge.step | True | ok | 1 | 10,000 | 4.185 | |
| cube-original.igs / meta-cn.igs / rot10-original-merge.igs | True | **FAIL（SelfIntersect）** | 0 | 12 / 12 / 6 | — | IGES 读入为散面，是 IGES 曲面格式的正常表现（见 [algorithms.md §11](algorithms.md#11-已知的-occt-801-限制)） |

**step_check.py**：上表全部 STEP 文件 PASS（闭壳边使用一致、体积为正、顶点到线/面的最大偏差 ≤ 1.8e-5，斜放合并的模型在 1e-5 量级，其余 ≤ 1e-12）。中文元数据解码正确（作者“致知屋”、组织“测试公司（深圳）”、产品“零件 A-01”）。

**pcurve_check.py**：13 个 STEP 文件全部 `bad=0`。

**注意**：这批文件是在 STEP 时间戳改成 UTC 之前生成的，文件头里时间戳仍是 `+08:00` 形式（`step_check` 报 `timestamp_utc_ok: false`）；只有 `cube-default-ts.step`（不传时间戳）是 `…Z`。当前代码对覆盖值也写 UTC（2026-10-21 用 CLI 实测：`--timestamp 2026-10-21T10:30:00+08:00` → `2026-10-21T02:30:00Z`）。重做时会全部变成 `timestamp_utc_ok: true`。

**NIST STEP File Analyzer 5.51**：需要额外安装 IFCsvr 组件，未做。

## 3. 怎样重做外部校验

脚本在 `validate/cad` 分支的 `core/tools/cadcheck/`（只是工具，没有合进 main）。说明见该目录的 `README.md`。

```bash
# 1) 取出脚本（在 PowerShell 里建一个临时 worktree，或直接 checkout 该分支的那个目录）
#    git -C D:\stl2stp-ios worktree add D:\stl2stp-cadcheck validate/cad
# 2) 准备 CLI 和夹具（WSL）
R=/mnt/d/stl2stp-flutter                       # 用当前代码编出的 CLI
CC=/mnt/d/stl2stp-cadcheck/core/tools/cadcheck
OUT=$R/build/cadcheck; FX=$OUT/fx; O=$OUT/out
mkdir -p $FX $O
cp $R/build/rel/tests/data/{cube-binary,sphere,two-shells}.stl $FX/     # ctest 夹具生成的
python3 $CC/gen.py $FX                          # 旋转立方体 n=4/10、开口立方体、两个旋转体
# 3) 生成导出（run.sh 里写死了路径 /mnt/d/stl2stp-ios 和 build/cc2-release，按需改成上面的变量；命令清单就是 run.sh 的内容）
CLI=$R/build/rel/stl2stp-cli; TS=2026-10-21T10:30:00+08:00
$CLI convert $FX/cube-binary.stl $O/cube-original.step --mode original --timestamp $TS
$CLI convert $FX/rot-cube-10.stl $O/rot10-original-merge.step --mode original --merge-coplanar --timestamp $TS
$CLI convert $FX/cube-binary.stl $O/meta-cn.step --mode original --author "致知屋" --org "测试公司（深圳）" --product "零件 A-01" --description "中文描述 °±" --timestamp $TS
# …其余见 run.sh
# 4) 独立检查
python3 $CC/step_check.py $O/*.step            # 写 step_check_report.json，逐文件 PASS/FAIL
python3 $CC/pcurve_check.py $O/*.step
python3 $CC/render.py $OUT/render.png $O/rot10-original-merge.step   # 需要 matplotlib
```

FreeCAD（Windows）：

```powershell
& "C:\Program Files\FreeCAD 1.0\bin\FreeCADCmd.exe" D:\stl2stp-cadcheck\core\tools\cadcheck\fc_check.py -- D:\stl2stp-flutter\build\cadcheck\out D:\stl2stp-flutter\build\cadcheck\freecad.txt
```

（FreeCAD 的安装路径以你的机器为准；`fc_check.py` 的参数格式见 README：`freecadcmd fc_check.py -- <dir> <report.txt>`。）

## 4. SolidWorks 测试包与待做的 Parasolid 检查

- 测试包：`D:\stl2stp-ios\build\stl2stp-cad-test.zip`（78,968 字节，在 `build/` 下不入库），包含：

| 文件 | 检查什么 |
|---|---|
| `cube-original.step` | 最基本的实体能否打开、是否是 1 个实体 |
| `meta-cn.step` | 文件属性里中文作者/组织/产品是否正确（`\X2\` 解码） |
| `rot10-original-merge.step` | 斜放模型共面合并后是否有效（对应 §11 第 1 条的 OCCT 朝向疑点） |
| `rot-two-bodies-original-merge.step` | 两个实体是否被识别为两个 body |
| `open-cube-original.step` | 开放模型是否作为曲面导入、不报错 |

- 请测试者在 SolidWorks 里：打开每个文件 → 记录导入诊断（Import Diagnostics）是否报错、FeatureManager 里是实体还是曲面、实体个数；对 meta-cn 查看“文件属性/自定义属性”；对实体运行“检查”（Tools › Evaluate › Check）。
- **Parasolid 检查（待做）**：SolidWorks 用 Parasolid 内核；如果能拿到其它 Parasolid 系软件（Solid Edge、NX、Onshape 等）或 SolidWorks 导出的 `.x_t`，可以把上面文件导入后再导出 Parasolid，确认几何/拓扑没有被修复或丢失。目前**没有任何 Parasolid 结果**。
- 结果反馈后：如果有问题，在 `core/` 建 `core/<主题>` 分支修正，补测试，把结论写进本页和 [core-validation.md](../core-validation.md)。
- 想重新打包：从 §3 的输出里取同名文件 `zip` 即可（建议用 UTC 时间戳的新输出）。

## 5. 什么时候必须重做

- 改了 `exchange.cpp`（构面、合并、写出、元数据）→ CTest + §3 全套。
- 升级 OCCT → CTest + §3 全套 + 估算/内存重新标定（见 [recipes.md R6](recipes.md#r6-升级-occt-或-flutter)）。
- 改了 `weld.cpp` / `simplify.cpp` → CTest（review_*）+ 至少 sphere/rot 用例的 step_check。
