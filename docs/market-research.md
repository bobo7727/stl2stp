# 3D-STL2STP 竞品调研与商业化建议

调研日期：2026-10-28

> 结论先行：手机端「STL→STEP」赛道很新，基本没人评价也没有收入证据；真正值钱的是「平面/孔/圆柱识别」，纯三角面输出已经被免费网站覆盖。

标注说明：「未查到」= 本次调研未找到可靠来源；「推断」= 分析判断，非查到的数据；「内部信息」= 合伙人提供，无外部来源；「建议」= 定价等建议值，非查到的数据。

## 目录

- [首要推荐：先攻谁、怎么定价（赚钱第一）](#首要推荐先攻谁怎么定价赚钱第一)
- [核心结论](#核心结论)
- [iOS 竞品（iTunes Search/Lookup API，us/cn，2026-10-28）](#ios-竞品itunes-searchlookup-apiuscn2026-10-28)
- [桌面与在线替代：用户现在怎么解决](#桌面与在线替代用户现在怎么解决)
- [用户原话与痛点](#用户原话与痛点)
- [用户画像与付费意愿](#用户画像与付费意愿)
- [商业化建议](#商业化建议)
- [QuickSurface 对比](#quicksurface-对比)
- [数据缺口（未查到）](#数据缺口未查到)

## 首要推荐：先攻谁、怎么定价（赚钱第一）

> 先攻国内机械工程师和外包设计（客户只给 STL，需要进 SolidWorks/UG）。主力产品用 Windows 桌面版，建议定价：¥9.9 基础版引流，Pro 买断 ¥39–69，里面放 IGES、批量转换、大文件、减面/焊接；以后做出孔/平面识别，再按次收费 ¥5–10 或包月 ¥29–49。这些价格是建议，不是查到的数据。依据：这类用户可替代的方案是 Fusion 商业版 $680/年（https://www.autodesk.com/solutions/autodesk-fusion-subscription-faq）或 Geomagic；闲鱼上 ¥10 档已经有 7 天 7 单的成交信号（内部信息）；SolidWorks 用户求助最集中（https://www.reddit.com/r/SolidWorks/comments/1nkmpsn/）。手机 App 只用来引流，买断 ¥18–30。

| 排名 | 用户群 | 付费意愿 | 可能客单价 | 依据 |
|---:|---|---|---|---|
| 1 | 机械工程师/外包/小工厂 | 高 | ¥39–69 买断，按次 ¥5–10（建议） | 替代方案 Fusion $85/月、$680/年：https://www.autodesk.com/solutions/autodesk-fusion-subscription-faq |
| 2 | 逆向/扫描用户 | 最高，但三角面实体满足不了 | Geomagic 级，价格未查到 | https://www.reddit.com/r/SolidWorks/comments/1qxp9r8/ （需要 Design X） |
| 3 | PCB/KiCad 元件建模 | 中 | ¥10–30（建议） | 三角面就够用：https://anyconv.com/stl-to-step-converter/ |
| 4 | 3D 打印玩家 | 低 | $0–5 | 嫌 $70/月贵：https://forums.autodesk.com/t5/fusion-support-forum/hobbyist-want-to-convert-mesh-to-prismatic-solid-body/m-p/13192898 |
| 5 | 学生/毕设 | 低 | ¥10 | 闲鱼 ¥10 成交（内部信息） |

逆向用户排第二，原因是他们客单价最高，但要等产品能做特征识别或曲面重建才能拿下。

## 核心结论

1. iOS 已有至少 3 款本地 STL→STEP/格式转换 App，都在 2026 年 5–9 月上架，免费下载，评分数均为 0（iTunes API，2026-10-28 查询，见下表链接）。说明赛道刚起步，没有头部，也没有付费验证。
2. 纯三角面 STEP 输出已经有免费的替代品：ImageToStl、AnyConv、FreeCAD（https://imagetostl.com/convert/file/stl/to/step 、https://anyconv.com/stl-to-step-converter/ ）。3D-STL2STP 现在也输出三角面实体，单靠「能转」很难收费。
3. 用户真正想要的是平面、孔、圆柱这类可编辑面，不想要一堆三角面。Fusion 把这个功能（Prismatic）锁在 $85/月的商业版里（autodesk.com 支持文档：https://www.autodesk.com/support/technical/article/caas/sfdcarticles/sfdcarticles/The-prismatic-option-is-missing-in-the-convert-mesh-menu-in-Fusion-360.html ），STLBench、MeshWeave、stl2step 也都把它当作主要卖点。差异化应该优先投在这里。
4. 国内需求有内部信号（闲鱼 ¥10 桌面版 7 天卖出 7 单，咨询很多），但客单价很低。国外 Reddit 上的常见说法是「STL 转不了 STEP」，信任门槛高，所以要诚实标注「输出仍是三角面实体」。
5. 建议布局：桌面端（Windows）为主力收入；手机 App 用来引流和预览，并买断解锁；云端按次付费，专做「特征识别」高级转换。

> ⚠️ 诚实对比：3D-STL2STP 目前的输出是由三角面组成的实体，不做参数化重建，也不识别特征。在 SolidWorks/UG 里能打开、测量、做布尔运算，但孔不是真正的圆柱面。这一点必须写进 App Store 描述，否则差评风险很高（参考 https://grabcad.com/tutorials/convert-stl-mesh-to-solidworks-model ）。

## iOS 竞品（iTunes Search/Lookup API，us/cn，2026-10-28）

| App | 开发者 | 定价 | 评分（数量） | 本地/功能 | 链接 |
|---|---|---|---|---|---|
| STL to STEP Assistant | 维 艾 | 免费（内购未查到） | 0 评分 | 本地，STL/OBJ/3MF/GLB 等→STEP | https://apps.apple.com/us/app/id6769270457 |
| Local 3D Model Converter | Shanghai Fuwei | 免费 | 0 评分 | 本地，批量/文件夹扫描 | https://apps.apple.com/us/app/id6815480133 |
| 3D File Converter: STL & 3MF | Berkay Taha Tutcu | 免费 | 0 评分 | 本地，网格格式互转 | https://apps.apple.com/us/app/id6801810344 |
| 3D Model Viewer & Converter | 维 艾 | 免费 | 美区 1.5（2） | 查看+转换 | https://apps.apple.com/us/app/id6748610196 |
| STEP File Viewer | 维 艾 | 免费 | 美区 3.17（6）/ 国区 4.76（17） | STEP 查看 | https://apps.apple.com/us/app/id6755721056 |
| 3D File Viewer: OBJ STP & more | Andeesh Mehdi Syed | 免费 | 4.37（54） | 多格式查看 | https://apps.apple.com/us/app/id6745021064 |
| IGES & STEP Viewer | Victor Bovio | 付费 ¥48 | 国区 3.5（4） | IGES/STEP 查看 | https://apps.apple.com/cn/app/id1489625660 |
| Autodesk Fusion | Autodesk | 免费下载 | 4.52（4095） | 云端 CAD 查看/协作 | https://apps.apple.com/us/app/id991074843 |
| Onshape 3D CAD | PTC | 免费下载 | 3.53（416） | 云端参数化 CAD | https://apps.apple.com/us/app/id923421284 |
| SView看图纸 | 山东华天 | 免费 | 国区 4.86（29382） | 国内三维看图 | https://apps.apple.com/cn/app/id954963044 |

CAD Assistant（Open CASCADE）、Shapr3D、eDrawings 用 Search API 没有搜到对应条目，评分和价格未查到。直接竞品的内购价格 Lookup API 不返回，未查到。

| 安卓 App（Google Play） | 开发者 | 定价 | 评分/下载 | STL→STEP | 链接 |
|---|---|---|---|---|---|
| CAD Assistant | OPEN CASCADE | 免费 | 4.3（3.37K 评价），500K+ 下载 | 可以：网格查看，可导出 STEP/IGES（离线） | https://play.google.com/store/apps/details?id=org.opencascade.cadassistant |
| CAD Exchanger | CADEX-SOFT | 免费，有付费年订阅（价格未查到） | 3.7（736 评价），100K+ 下载 | 支持 STL→STEP/IGES；移动端导入限 25MB | https://play.google.com/store/apps/details?id=com.cadexchanger.gui |

CAD Exchanger 官方博客说「最常被问的问题就是 How do I convert STL to STEP or IGES?」（https://cadexchanger.com/blog/stl-to-step-and-iges-smartphone-support-and-other-improvements-in-cad-exchanger-3-2-1/ ）。这说明需求真实存在，但免费的 OCC 方案在安卓端已经有了。

## 桌面与在线替代：用户现在怎么解决

| 方案 | 价格 | 输出质量 | 限制 | 来源 |
|---|---|---|---|---|
| ImageToStl（在线） | 免费，广告 | 三角面（未说明） | 500MB，批量 200 个 | https://imagetostl.com/convert/file/stl/to/step |
| AnyConv（在线） | 免费 | 三角面（官方承认） | 100MB | https://anyconv.com/stl-to-step-converter/ |
| STLBench（浏览器本地） | beta 免费，将来收费 | 重建平面/圆柱/圆锥/圆角 | 无上限 | https://stlbench.com/stl-to-step |
| MeshWeave（在线） | 免费每天 2 次 | 重建解析曲面+B 样条 | 需上传 | https://meshweave.app/compare/stl-to-step-converters |
| stl2step（命令行） | 未查到价格 | 重建平面/圆柱/圆锥/球 | 本地 | https://next.brep.io/stl2step |
| 2STEP-Converter（开源） | 免费 | OCC，原语+孔识别 | 本地批量 | https://github.com/yaneony/2STEP-Converter |
| FreeCAD | 免费开源 | 三角面，需多步手动 | 本地 | https://wiki.freecad.org/FreeCAD_and_Mesh_Import |
| Fusion 网格转实体 | Prismatic 选项需商业版 $85/月、$680/年 | 棱柱识别（付费） | 建议 <1 万面 | https://www.autodesk.com/solutions/autodesk-fusion-subscription-faq |
| SolidWorks 导入为实体 / ScanTo3D | 随 SolidWorks，单价未查到 | 三角面 / 半手动曲面 | 本地 | https://jingyan.baidu.com/article/aa6a2c140566af4d4d19c46e.html |
| Geomagic Design X | 价格未查到 | 专业逆向参数化 | 本地 | https://www.reddit.com/r/SolidWorks/comments/1qxp9r8/ |
| MeshLab | 免费，不导出 STEP（未核实官方说明） | 仅网格修复 | 本地 | 未查到 |

闲鱼/淘宝：公开网页搜不到，未查到。内部信息（合伙人提供，无外部来源）：合伙人桌面版 ¥10，7 天 7 单，买家没有反馈；同类卖家「麦芽儿zzy」也标价 ¥10，宣传「一键 STL 转 STEP/IGES，无缝进 SolidWorks、UG」。

## 用户原话与痛点

> I'm trying to convert some really complex STL files (40k+ triangles) into STEP for SolidWorks... I don't want to pay for professional reverse-engineering software.
>
> — u/（楼主），r/SolidWorks，https://www.reddit.com/r/SolidWorks/comments/1nkmpsn/

> Is there a way to convert Stl to step file without all of the triangles from the STL file?
>
> — r/FreeCAD，https://www.reddit.com/r/FreeCAD/comments/1nkmuax/stl_to_step/

> the prismatic option is locked out unless you pay for the subscription. As I'm new to the hobby... $70/month is a large expense for me.
>
> — Autodesk 论坛，https://forums.autodesk.com/t5/fusion-support-forum/hobbyist-want-to-convert-mesh-to-prismatic-solid-body/m-p/13192898

> it's very computer resource heavy Everytime you try to do a certain action it will load for quite a few minutes
>
> — u/TricepBandito，https://www.reddit.com/r/SolidWorks/comments/1qxp9r8/

> STL文件保存的是表面数据，目前的三维软件都难以较好的实线将其转换为实体。
>
> — 百度经验，https://jingyan.baidu.com/article/aa6a2c140566af4d4d19c46e.html

- 痛点 1：输出全是三角面，没法编辑或倒角
- 痛点 2：免费方案需要多步手动操作，耗时又吃性能
- 痛点 3：好用的方案太贵（Fusion 商业版、Geomagic）
- 痛点 4：面数上限（Fusion 建议少于 1 万面），需要先减面
- App Store 直接竞品评论：评分为 0，没有评论可引用，未查到

## 用户画像与付费意愿

| 画像 | 场景 | 付费意愿（依据） |
|---|---|---|
| 3D 打印玩家 | 下载 Thingiverse 的 STL，想改尺寸 | 低：不愿付 $70/月（Autodesk 论坛帖） |
| 机械工程师/外包 | 客户只给 STL，要进 SW/UG 出图、开模 | 中高：能为特征识别付费；SW 用户占多数（reddit r/SolidWorks） |
| 学生/毕设 | 作业需要 STEP 格式 | 低，¥10 级（闲鱼内部信号） |
| 扫描/逆向用户 | 扫描件转 CAD | 高，但需要的是 Geomagic 级工具，三角面实体不满足 |
| PCB/KiCad 用户 | STL 元件转 STEP 放进板子 | 低到中，三角面就够用（anyconv FAQ） |

知乎、B 站、小红书的原话本次未查到（搜索没有返回可引用的帖子），建议人工补采。

## 商业化建议

- 定价（建议，非数据）：iOS 买断 Pro ¥18–30 / $2.99–4.99，对标免费网站和闲鱼 ¥10，并参考 IGES & STEP Viewer 的 ¥48；暂不做订阅，因为同类没有评分、信任度低。
- 免费：预览、小于 5 万面的 STL→STEP、带水印或限次。付费：IGES、批量、减面/焊接、大文件、无广告。
- 云端按次：特征识别（平面合并、孔/圆柱识别）每次 ¥3–5 或包月，面向工程师。这是最值钱的方向，可以直接对标 Fusion Prismatic。
- 差异化价值排序：平面合并+孔/圆柱识别 > 批量转换 > 更多格式（竞品已覆盖 15+ 种格式）> 完整曲面重建（成本太高，暂缓）。
- 国内获客：闲鱼/淘宝低价桌面版加 App 下载引导；在 B 站发「STL 转 STEP 进 SolidWorks」教程视频；在知乎回答相关问题。
- 海外获客：在 r/3Dprinting、r/FreeCAD、r/SolidWorks 的求助帖里诚实回答；App Store 关键词用 STL to STEP / STP / IGES；去 Product Hunt 发布。
- 前 100 个付费用户：先把闲鱼 7 单的买家转化成种子用户，回访他们用在什么场景；TestFlight 用户提供首发半价；每周发 2 条教程短视频；回答 20 个求助帖。

## QuickSurface 对比

> 结论：QuickSurface 卖的不是「格式转换」，而是把扫描件或 STL 变成能加工、能改尺寸的 CAD，省下的是工程师重新建模的工时。官方价格：Lite 每年 €480（订阅）；for SOLIDWORKS 每年 €1100 或买断 €3300；Pro 每年 €1700 或买断 €5250；买断从第二年起维护费约为授权价的 10%/年（https://www.quicksurface.com/price/ 、https://www.quicksurface.com/maintenance/ ）。参照的锚点 Geomagic Design X Go 每年 $1,900 起（3dwonders.com 经销商页面：https://3dwonders.com/blogs/case-studies/a-complete-guide-to-geomagic-design-x-reverse-engineering-software-from-entry-level-to-pro ）。

| 项目 | 内容 |
|---|---|
| 功能 | 网格修复/减面、对齐到坐标系或 CAD、自动分割、基元拟合、2D/3D 草图、拉伸/放样/扫掠/圆角、自动与手动自由曲面、混合建模、实时偏差色图、导出 IGES/STEP/Parasolid（https://www.quicksurface.com/price/ ）；2026 版改用 Siemens Parasolid 内核（https://shop.gomeasure3d.com/products/quicksurface-maintenance ） |
| 版本 | Personal（非商用；第三方称 €20/月按年付，https://www.itechguides.com/products/quicksurface/ ，官方页面未核实）、Lite、Pro、for SOLIDWORKS 插件；Pro 和 for SOLIDWORKS 有 30 天试用（https://www.quicksurface.com/ ） |
| 平台 | Windows 桌面（https://www.itechguides.com/products/quicksurface/ ） |
| 目标客户 | 汽车、航空、医疗/牙科、机械设计、工业设计（https://us.store.creality.com/products/quicksurface-lite-pro ） |
| 渠道 | 扫描仪厂商商城捆绑销售（Creality 美国商城 $540 起：https://us.store.creality.com/products/quicksurface-lite-pro ）；经销商 GoMeasure3D（Pro 年维护 $590：https://shop.gomeasure3d.com/products/quicksurface-maintenance ） |

| 对比项 | QuickSurface Pro | 3D-STL2STP |
|---|---|---|
| 输出 | 可编辑的 NURBS/解析面、草图、特征实体 | 三角面组成的实体（不做重建） |
| 价格 | €1700/年 或 €5250 买断 | 计划内购（建议 ¥18–69） |
| 平台 | Windows | iOS/iPadOS（已在规划桌面版） |
| 使用门槛 | 高：需要建模经验，还要先清理网格（https://www.reddit.com/r/3DScanning/comments/1iy89m4/ ） | 低：一键转换 |
| 精度控制 | 实时偏差分析 | 无 |
| 适合 | 逆向工程、模具、加工 | 进 CAD 打开、测量、做布尔运算、元件占位 |

> quicksurface is good but it's a monumentally cut down version of what geomagic is, they trade blows in workflow and of course price.
>
> — u/Pizzaholic-，https://www.reddit.com/r/3DScanning/comments/1wsjzsn/

> Quick Surface is much quicker to respond to bug fixes and adding new features.
>
> — u/ArthurNYC3D，同帖

> orienting a scan to start rebuilding the cad file is the worst part
>
> — u/esordol，https://www.reddit.com/r/3DScanning/comments/1vmswi4/

视频评论未查到。

- 为什么贵 1，价格锚点：行业标准 Geomagic Design X 入门版每年 $1,900 起，QuickSurface 定价在它下面，看起来就是「便宜的那个」（3dwonders.com、https://www.reddit.com/r/3DScanning/comments/1vmswi4/ ）。
- 为什么贵 2，客户是企业：官方写明价格是单个商业授权，网络授权要询价；Personal 版明确不能商用（https://www.quicksurface.com/price/ ）。企业按工时算账，不按软件标价算。
- 为什么贵 3，节省工时：Design X 官方宣称比手工重建快 3–10 倍（3dwonders.com 引 Hexagon 数据）；QuickSurface 自己的工时数据未查到。推断：只要每单省 1–2 天工程师时间，一年几千欧元就能回本。
- 为什么贵 4，渠道和捆绑：跟扫描仪一起卖、靠经销商卖，买家本来就在花几千美元买硬件（https://www.reddit.com/r/3DScanning/comments/1vmswi4/ 提到「anything at 2k is a great scanner」）。推断：软件价格是跟硬件预算挂钩的。
- 为什么贵 5，技术难点：难的是在有噪声、不完整的网格上稳定地分割和拟合曲面，还要和 Parasolid 级内核配合做圆角、布尔运算（gomeasure3d 页面；reddit 1iy89m4 也说结果取决于网格质量和完整度）。界面看起来简单，难的部分在算法鲁棒性上。这一条是推断。

1. 要走到 €500+ 价位需要补的能力：平面合并 → 孔/圆柱/圆锥识别 → 对齐到坐标系 → 截面草图 → 偏差色图 → 自由曲面拟合。前三项做完就能对标 Lite 的「feature extraction / fit surface」。
2. 能用的开源或 OCC 基础：OCC 自带 `BRepBuilderAPI_Sewing`、`ShapeUpgrade_UnifySameDomain`（合并共面面）和 `GeomAPI_PointsToBSplineSurface`（B 样条拟合），这是 OCC 文档里有的 API，具体效果需要实测。参考实现有 2STEP-Converter（开源，基于 OCC，做原语和孔识别：https://github.com/yaneony/2STEP-Converter ）和 stl2step（https://next.brep.io/stl2step ）。
3. 推断：手机端很难卖到 €500 级别。更现实的路线是做一个 ¥100–500 的「Lite 平替」，主打机械件的平面和孔识别，目标用户是买不起 Design X、但有加工需求的个人和小厂。

## 数据缺口（未查到）

- Google Play 上其他小开发者的转换 App、CAD Exchanger 订阅价格
- 直接竞品的内购价格和下载量
- CAD Assistant / Shapr3D / eDrawings 的 App Store 数据
- 知乎、B 站、小红书原帖
- 闲鱼/淘宝公开销量
- Geomagic、SolidWorks ScanTo3D 的价格
