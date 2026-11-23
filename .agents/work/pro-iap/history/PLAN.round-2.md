# Plan

- feature_dir: .agents/work/pro-iap
- title: STL2STP Pro 买断（美国 4.99 美元，非消耗型）
- status: draft → 自审后交独立只读评审
- mode: full
- branch: feat/pro-iap（从 feat/tip-iap 的 9e7ef85 切出，理由见 §1）
- phase: plan
- round: 2/3（第 1 轮 Codex 评审 CHANGES：3 major、4 minor，本版逐条处理，见 §14）
- author: Claude（Sonnet 5.5）
- last_updated: 2026-11-23

## Goal

### 给老板的摘要

在 iOS 上架时同时带两个内购：已有的打赏（消耗型，不变）和新的 **Pro 买断**（非消耗型，美国 4.99 美元，一次购买、不是订阅、可恢复），解锁三样：**IGES 导出、批量转换、大文件**。元数据（作者/公司等文件信息）**建议继续免费**。

要先说清三件事：

1. **批量转换现在不存在。** 现有代码只能一次选一个 STL（`FilePicker.pickFile`，拖放也是单文件）。“批量”要新写一个多选加队列功能，约 3–4 个工程日（暂估）。深度报告写的“2–3 人日”是把批量当成已有功能算的，所以偏低。本方案总工作量是 **约 5–6 个工程日（不含批量）/ 8–10 个工程日（含批量）**，见 §10。
2. **IGES 和大文件今天在代码里是免费的**（`FreeTierLimits.none`，什么都不限），但 App 还没上架，没有公开用户，只有 TestFlight 里的测试版。把它们放进 Pro，对公众来说是“从一开始就是付费功能”，不是“收回已有功能”。仍需要您确认这点（§12 第 2 条）。
3. **付费墙只能宣传已经存在的功能。** 批量没做完就上架的话，付费墙文案和商品描述不能提批量，否则可能被审核拒绝。所以我建议分两个里程碑：M1（Pro 购买 + IGES + 大文件）先可上架，M2（批量）做完再加进文案。要不要等批量一起上，由您决定。

### 现在的内购到底用什么（您问的核实）

| 平台 | 打赏 | Pro |
| --- | --- | --- |
| iOS（feat/tip-iap，提交 9e7ef85） | **原生 StoreKit 2**（`ios/Runner/StoreCoordinator.swift`，唯一的交易监听） | **目前不卖也不恢复**（上一轮已去掉），原生只 `finish` 残留的旧 Pro 交易 |
| macOS / Android | 不提供 | Flutter `in_app_purchase` 插件（`StorePurchaseService`），未测试，也不是上架目标 |
| Linux / Windows / 测试 | 不提供 | 模拟商店 |

深度报告说“购买代码是 `in_app_purchase` 包、没找到 StoreCoordinator.swift”，**对 master 成立，对 feat/tip-iap 不成立**：`git ls-tree master` 里确实没有 `StoreCoordinator.swift`；它是打赏分支新加的。`pubspec.yaml` 仍声明 `in_app_purchase ^3.3.1`，但 iOS 路径不再构造 `InAppPurchase.instance`。我读了锁定版本 `in_app_purchase_storekit 0.4.13` 的源码：它的 `Transaction.updates` 监听只在 Dart 侧调用 `startListeningToTransactions` 时才启动，所以 iOS 上只有我们自己的一个监听，没有第二个。本方案把 Pro 也并入原生 StoreKit 2，iOS 完全不再用 `in_app_purchase`。

## Context

### 1. 分支选择

从 `feat/tip-iap` 切出 `feat/pro-iap`：Pro 要复用打赏分支刚写好的原生 `StoreCoordinator`、通道和 `IosStoreService`，直接基于 master 会重复实现；另开分支而不是继续在 feat/tip-iap 上改，是因为打赏分支已经推送并在跑 Codemagic 构建（6b0401da），不想在构建验证期间动它，也让老板可以分别审两部分改动。代价是 Pro 分支依赖打赏分支先合并（或一起合并）。本阶段不推送、不开 PR、不启动构建。

关于 Codemagic 构建 6b0401da：结果还没收到。方案阶段不依赖它，但**代码阶段开始前必须先确认它通过**；如果失败，先修构建（那是 Swift 第一次被真正编译，失败概率不低）。

### 2. 现状（已核实，行号为 9e7ef85）

| 事实 | 位置 |
| --- | --- |
| `FreeTierLimits` 支持 4 个维度：`maxTriangles`、`dailyConversions`、`igesRequiresPro`、`metaRequiresPro`；默认 `none`；缺少“批量”维度 | `lib/purchase/free_tier.dart` |
| 限制检查点只有一个：预览页点“转换”时 `ensureAllowed()`，命中则打开 `PaywallScreen` | `lib/screens/preview_screen.dart:70`、`paywall_screen.dart` 末尾 |
| `PaywallScreen` 还是设计占位：黄色“占位”横幅、“价格待定”、功能行带“占位”标签 | `paywall_screen.dart` |
| 选文件只支持单个 STL | `lib/screens/flow.dart:36`、`lib/widgets/stl_drop.dart` |
| 设置页在 iOS 上不显示 Pro 行和恢复购买（上一轮 D2/简化） | `settings_screen.dart` |
| `AppState.limits` 默认 `FreeTierLimits.none`，`main.dart` 没有传入真实限额 | `lib/state/app_state.dart`、`lib/main.dart` |
| 原生只有打赏；Pro 交易只 finish | `ios/Runner/StoreCoordinator.swift` |
| 估算：约 50k 面峰值内存约 1 GB、102k 面约 2 GB（本机已测的校准数据，不同机型不同） | `mobile/docs/core-validation.md` |

## Approach

### 3. 免费与付费的边界

**原则：已经对公众可用的功能不收费。** 目前没有任何公开版本，所以边界可以一次定好。

| 功能 | 免费 | Pro |
| --- | --- | --- |
| 打开 STL、3D 预览、旋转缩放 | 是 | 是 |
| 转换为 STEP（单文件） | 是 | 是 |
| 转换预设、焊接/缝合选项、减面 | 是 | 是 |
| 文件信息（作者/公司/产品/描述） | **是（建议继续免费）** | 是 |
| 历史记录、分享、存到“文件” | 是 | 是 |
| 转换为 **IGES** | 否 | 是 |
| 一次转换 **多个文件（批量）** | 否（一次一个） | 是 |
| **大文件**：面数超过 **50,000**（减面之后的面数，沿用现有 `check()` 逻辑） | 否 | 无上限（仍受已有内存保护） |
| 每日次数限制 | **不设** | — |
| 打赏 | 任何人 | 任何人，不解锁任何东西 |

要点和理由：

- **元数据继续免费**：深度报告把它放进 Pro，我建议不放。它只是 STEP 头部几个文本字段，转化价值低；放进去会给免费用户多一个“看不见的墙”，还要在设置页的默认元数据里处理；付费墙只讲三件事（IGES、批量、大文件）也更清楚。`FreeTierLimits.metaRequiresPro` 保持 false，代码不用动。想以后收，改一个布尔值即可，但那时 App 已上线，就变成“收回已有功能”，所以**现在定下来就不要再改**。
- **大文件上限 50,000 面**：来自仓库 `docs/market-research.md` 的建议值，也接近本机实测的内存拐点。**免费用户仍可以先减面再转换**（现有 `check()` 按减面后的面数算），付费墙会给“减到约 X% 免费转换”的出路，避免变成死胡同。这个数是产品决定，需要老板确认（§12）。
- **不设每日次数**：深度报告也认为对工程师是负体验。
- **IGES 为什么放 Pro**：竞品里同类导出解锁卖 6.99–9.99 美元，工程用户为“进 SolidWorks/UG”付费意愿最高。免费保留 STEP（主流格式），所以免费版不是残废演示。

### 4. 商品 ID 与价格

| 项 | 值 |
| --- | --- |
| Product ID | `com.stl2stp.app.pro`（沿用代码里已有的 ID，下面解释） |
| 类型 | Non-Consumable（非消耗型） |
| Reference Name | `STL2STP Pro` |
| 美国基准价 | **USD 4.99**；其他地区用 ASC 自动换算，老板预览确认；界面只显示商店返回的 `displayPrice`，不硬编码、不拼货币符号 |
| 展示名/描述（英文，**M1**） | `STL2STP Pro` / `Unlock IGES export and large files. One-time purchase.`（上架前核对字符数限制） |
| 展示名/描述（简中，**M1**） | `STL2STP Pro` / `解锁 IGES 导出和大文件。一次购买，不是订阅。` |
| M2 批量上线后才改成 | EN `Unlock IGES export, batch conversion and large files. One-time purchase.` / 中 `解锁 IGES 导出、批量转换和大文件。一次购买，不是订阅。`（随含批量的版本修改并送审） |
| 家庭共享 | **关闭**（打开后不能再关；买断工具类没有必要） |
| 订阅 | 无 |

沿用 `com.stl2stp.app.pro` 的理由：代码、测试和文档里一直是这个 ID，老板说没人买过。风险：ASC 里可能已经有一个按旧设想建的同 ID 商品（类型/价格/本地化不对）。ASC 的 Product ID 保存后不能改、删除后不能复用，所以**第一步先看 ASC 里有没有、状态如何**（§9）；如果类型不是非消耗型或状态异常，就换新 ID（建议 `com.stl2stp.app.pro.v2`）并更新代码常量，不去“修”旧商品。

不要混淆：打赏商品 `com.stl2stp.app.tip.small` 是另一个商品，两者**本项目安排在同一次首次提交里一起送审**（Apple 的规则是每种类型的第一个内购要随一个 App 版本送审，这里两个商品都随首发版本；送审前重查 Apple 当前规则）。

### 5. 付费墙：什么时候出现、出现什么

**触发时机（只有三个，全部发生在用户主动操作之后，启动时、打开 App 时、转换完成后都不弹）：**

1. 在预览页选了 **IGES** 并点“转换”。
2. 模型超过 50,000 面（减面后仍超）并点“转换”。
3. （M2 之后）选择了**多于一个**文件，或点“批量转换”入口。

深度报告建议“转换成功之后再弹”。但 IGES 和大文件在转换**之前**就知道，转换成功后才拦就得白算一遍还要丢掉结果，体验更差。折中：用户在**预览页**已经看到自己的模型和估算时间，才碰到墙，这已经是“确认产品有用之后”。如果老板更想“先让他免费转 STEP，成功后在完成页提示‘也想要 IGES？’”，那是另一个入口（完成页的次要按钮），可以后加，本方案不做。

**行为：**

- `PaywallScreen` 保留，但结果改为下面的三种结果（`purchased` / `cancelled` / `useFree`）；购买和恢复成功后同样要经过最终权益与限制重检，不再用 bool。
- 按触发原因在顶部显示一行说明（见下表）。大文件触发时，在**可达成**的前提下增加一个免费出路按钮（契约见下）。
- 价格用 `displayPrice`。商品还没加载出来时按钮**禁用并显示“正在加载价格…”，并提供重试**；绝不显示占位或写死的价格。
- **付费墙的三种结果**（取代现在只返回 bool 的 `ensureAllowed`）：`purchased`（继续原设置，并重新检查）、`cancelled`（回到预览，什么都不改）、`useFree(ConvertSettings)`（采用改过的免费设置）。预览页拿到结果后才继续，并且**继续前对最终设置重新跑全部限制检查（`limitFor`）和内存检查（`ensureMemory`），再用这份设置构造 `ConvertRequest`**；任何一项仍命中就不放行，不能靠“已点过免费出路”绕过。
- **免费出路的规则**：
  1. 只在命中“大文件”时出现。所需比例 = 向下取整到 5 的倍数的 `floor(50000 × 100 / 面数)`；现有滑块范围 5%–95%、步长 5%，所以**面数超过 1,000,000（5% 仍超限）或所需比例低于 5% 时不显示该按钮**，不承诺做不到的事。
  2. 如果同时选了 IGES：按钮文案改为“Reduce to {percent}% and export STEP free / 减到 {percent}% 并导出 STEP（免费）”，点击把格式改成 STEP **和**比例一起写回；绝不在 IGES 下放行免费减面。想要 IGES 只能购买。
  3. 只命中 IGES（面数没超）时没有减面按钮，只有“购买”“改用 STEP（免费）”（把格式改为 STEP 并继续）、“以后再说”。
- “以后再说”只关闭付费墙，不改变用户的任何设置，也不记录“已拒绝”。
- 已购用户永远看不到付费墙。

**M1/M2 两套文案由一个编译期常量 `kBatchAvailable`（M1 为 false）选择**：false 时标题、功能行、商品描述、截图、审核备注一律只写 IGES 和大文件；true 才出现批量。M1 的组件测试断言：付费墙任何位置（标题、副标题、功能行、说明）都不含“batch / 批量”，且批量入口不存在。

**文案（英文优先；中文并列，沿用 `app_strings.py` 机制，新键用 `pro.*`，不改设计表里的 `paywall.*`）：**

| 键 | English | 简体中文 |
| --- | --- | --- |
| `pro.title`（**M1**） | Unlock IGES export and large files | 解锁 IGES 导出和大文件 |
| `pro.title.batch`（**M2 起替换上一行**） | Unlock IGES, batch and large files | 解锁 IGES、批量转换和大文件 |
| `pro.subtitle` | One-time purchase. No subscription. Your files never leave your phone. | 一次购买，不是订阅。文件全程在本机处理，不上传。 |
| `pro.feature.iges` | IGES export for SolidWorks, UG and more | 导出 IGES，可用于 SolidWorks、UG 等 |
| `pro.feature.batch`（**M2 起才加入功能行**） | Convert many files at once | 一次转换多个文件 |
| `pro.feature.large` | Models over {limit} faces | 超过 {limit} 个面的大模型 |
| `pro.free_note` | Free: STEP export for models up to {limit} faces. | 免费：{limit} 个面以内的模型可导出 STEP。 |
| `pro.buy` | Unlock Pro — {price} | 解锁 Pro — {price} |
| `pro.price_loading` | Loading price… | 正在加载价格… |
| `pro.price_retry` | Retry | 重试 |
| `pro.restore` | Restore Purchases | 恢复购买 |
| `pro.later` | Not now | 以后再说 |
| `pro.reason.iges` | IGES export is part of Pro. | IGES 导出属于 Pro。 |
| `pro.reason.triangles` | This model has {faces} faces. Free conversion covers up to {limit}. | 这个模型有 {faces} 个面。免费版最多转换 {limit} 个面。 |
| `pro.reason.batch` | Converting several files at once is part of Pro. | 一次转换多个文件属于 Pro。 |
| `pro.reduce` | Reduce to {percent}% and convert free | 减到 {percent}% 免费转换 |
| `pro.reduce.step` | Reduce to {percent}% and export STEP free | 减到 {percent}% 并导出 STEP（免费） |
| `pro.use_step` | Use STEP instead (free) | 改用 STEP（免费） |
| `pro.status.pro` | Pro (unlocked) | Pro（已解锁） |
| `pro.status.free` | Free | 免费版 |
| `pro.success` | Pro unlocked. Thank you! | 已解锁 Pro，感谢支持！ |
| `pro.restored` | Purchase restored | 已恢复购买 |
| `pro.nothing_to_restore` | No purchases to restore | 没有找到可恢复的购买 |
| `pro.pending` | Purchase pending approval. It will unlock automatically once approved. | 购买正在等待批准，批准后会自动解锁。 |
| `pro.unverified` | The purchase could not be verified yet. Please try again later; there is no need to pay again. | 购买暂时无法验证，请稍后重试，无需再次付款。 |
| `pro.cancelled` | Purchase cancelled | 已取消购买 |
| `pro.failed` | Purchase did not complete: {msg} | 购买未完成：{msg} |
| `pro.unavailable` | The App Store is not available right now. Conversion is not affected. | 暂时无法连接 App Store，转换功能不受影响。 |

说明：

- 中文也统一叫 **Pro**（现有中文叫“完整版”，会一并改成 Pro，和英文、商品名一致）。旧的 `purchase.*`/`paywall.*` 键里被替换的删除，Mock/非 iOS 仍用的保留，**所有键中英文都要有，沿用 `i18n_test` 的约束**（英文无中文、购买类键不得与中文相同）。
- 感叹号：沿用老板已批准的 `support.thanks` 例外；`pro.success` 也带感叹号，需要在 `core_test` 的窄豁免里再加这一个键（需老板确认，§12）。
- 设置页（iOS）新增：购买分组里显示“Pro（已解锁）/免费版”状态行，点开进入付费墙/状态；**“恢复购买”恢复回来**（上一轮因为 Pro 不卖而去掉）；打赏入口不变。
- 打赏页文案不动，但在打赏页说明里保留“不解锁任何功能”那句，和 Pro 彻底分开，付费墙里不放打赏入口。

### 6. 恢复购买

- **入口**：付费墙底部固定“Restore Purchases”；设置页同名行。苹果 3.1.1 要求有恢复机制。
- **行为**：显式点击才调用 `AppStore.sync()`（可能弹 Apple ID 登录），然后重新计算权益。启动和回到前台时**不调用 sync**，只读本机已有的 `Transaction.currentEntitlements`。
- **结果反馈与缓存规则（定死）**：
  - `AppStore.sync()` 成功且重算后**有**有效权益 → “已恢复”，缓存 true。
  - sync 成功且重算后权益为空 → “没有找到可恢复的购买”，**缓存写 false**（Apple 为权威，与 §7.4 一致）。
  - sync 失败/用户取消登录/网络错误 → “恢复失败，请重试”，**缓存不动**。
  - 结果类型：现有 `PurchaseOutcome` 没有“未验证/忙碌”。**给枚举新增 `unverified` 和 `busy`**（只增不改，Mock 与 StorePurchaseService 不受影响），`showPurchaseResult` 补两个分支：unverified 显示 `pro.unverified`，busy 不弹提示。其余沿用：成功→`success`，已恢复→`restored`，没找到→`nothingToRestore`，取消→`cancelled`，等待→`pending`，错误→`failed`，商店不可用→`unavailable`。
- 打赏是消耗型，不恢复；恢复文案里不再提打赏。

### 7. 技术方案（原生 StoreKit 2，iOS）

在上一轮已有的 `StoreCoordinator` 上扩展，**不引入账本/迁移/历史对账**（上一轮因“旧 Pro 有买家”设计的那套 ProLedger 已删除；现在 Pro 是新商品、没有历史包袱，用 Apple 推荐的最简方式）：

1. **权益的唯一来源**：`Transaction.currentEntitlements` 里 `.verified`、`productID == pro`、`productType == .nonConsumable`、`appBundleID` 匹配、`revocationDate == nil` 的交易。有则 Pro。未验证的交易一律不授予、不 finish。
2. **何时重算**：冷启动（`start`）、`Transaction.updates` 收到 Pro 交易（含退款/撤销，App 运行中即时生效）、购买成功后、显式恢复后。App 在关闭期间发生的退款，下次冷启动重算时 `currentEntitlements` 已不含该交易，权益自动撤销。
3. **购买**：`Product.purchase()` → `.verified` 先重算权益再 `finish()`；`.pending`（询问购买）返回等待，之后由 `updates` 完成；`.userCancelled`、`.unverified`、异常分别映射到不同结果。同一时间只允许一个购买请求（沿用打赏的互斥，打赏和 Pro 共用一把锁）。
4. **离线缓存**：Dart 侧用 `SharedPreferences` 的 `store.proActive` 缓存最近一次原生算出的结果，让冷启动时到原生首次重算完成前，已购用户不会闪现付费墙。原生重算完成后以原生结果为准（可真可假）。空结果在这里被视为权威：对 4.99 美元的买断，不为“离线时误撤销”再造一套对账（上一轮为老买家设计的复杂度），代价是极少数“新设备/重装 + 离线”用户要先联网点一次恢复购买；这个取舍请评审质疑。
5. **事件**：原生通过已有 EventChannel 发 `{type:'pro', pro:bool}`；Dart `IosStoreService` 更新缓存并 `notifyListeners()`。方法通道新增 `buyPro`、`restore`，`loadProducts` 同时返回 Pro 商品；`start` 返回当前权益。
6. **Dart**：`IosStoreService` 的 `proForSale` 改回 true，`product`（价格）由 `loadProducts` 填充，`buyPro/restore` 走原生；`PurchaseService` 的方法不变，仅 `PurchaseOutcome` 枚举加两个值（§6），Mock 和非 iOS 不受影响。
7. **限额配置**：`FreeTierLimits` 新增 `batchRequiresPro`（默认 false）；新增常量 `FreeTierLimits.release = FreeTierLimits(maxTriangles: 50000, igesRequiresPro: true, batchRequiresPro: true)`。`createAppState` 只在 **iOS** 且非 Mock 时传入 `release`；其他平台和测试保持 `none`。限额常量集中在一处，便于调整。
8. **Pro 标识不影响打赏**：打赏路径不读不写 Pro 状态（沿用，有测试）。
9. **批量（M2，见 §8）。**
10. **安全边界**：不建服务器、不做收据验证服务；验证由 StoreKit 2 的 `VerificationResult` 在设备上完成。本地缓存可被越狱设备篡改，这是离线工具 App 可接受的风险，评审时请确认。

### 8. 批量转换（M2）

现状没有。设计要点（保持最小）：

- **入口**：首页新增“选择多个文件”（锁定版本 file_picker 13.1.0 的 `FilePicker.pickFiles()` 本身返回列表，**没有** `allowMultiple` 参数，已读源码确认）；非 Pro 点它或选出 >1 个文件时触发付费墙（`pro.reason.batch`）；Pro 用户进入**批量队列页**。**拖放批量不在范围内**（iOS 上现有拖放是空包装，iPad 原生拖放需另做，M2 不含）。
- **共享设置**：队列页顶部一份设置（格式/预设/元数据）应用到整批。
- **队列规则**：单个文件失败（读取/转换错误/校验未过）只标记失败并继续；**系统内存压力或后台时间到期则停止整个队列**（已完成的保留）；取消=取消当前并停止后续。
- **队列页**：依次（不并发，避免内存峰值叠加）用同一组设置转换每个文件，每个文件前跑现有 `ensureMemory` 检查，单个失败不中断整批；显示每个文件的状态（等待/转换中/完成/失败）和总进度；可取消（取消当前并停止后续）；后台任务沿用现有 `BackgroundTasks`。
- **输出**：每个文件一条历史记录；完成后“分享全部/存到文件”。
- **范围外**：文件夹递归、并行转换、不同文件不同设置。
- 工作量**暂估** 3–4 个工程日（file_picker 用法、iPad 拖放排除、队列失败/取消/内存压力/后台到期测试都计入；M2 开工前再确认）。**M1 不含批量**；M1 上架时付费墙不展示批量一行。

### 9. 需要老板在 App Store Connect 做的操作（分步）

完整的账号准备（付费 App 协议、税务、银行、隐私、沙盒测试员、内部测试员）已经在 `.agents/work/tip-iap/ASC_STEPS.md`，Pro 沿用，不重复。Pro 额外要做的：

1. **查旧商品**：`Apps → STL2STP → Monetization → In-App Purchases`，看有没有 `com.stl2stp.app.pro`。有的话记下：类型是不是 Non-Consumable、状态、价格、有没有本地化。确认（Sales and Trends）销量为零。做完确认：能说出“有/没有；类型；状态”。
2. **创建或核对 Pro 商品**：类型 Non-Consumable；Reference Name `STL2STP Pro`；Product ID `com.stl2stp.app.pro`（保存后不能改，照抄）；可售地区与 App 一致；价格：基准地区 United States，选 **USD 4.99**，看一眼国际价预览；Family Sharing 保持**关闭**；简中和英语的显示名/描述按 §4 填。做完确认：商品详情页字段齐全，必填字段齐全；**第 3 步审核截图上传后**状态才会变 Ready to Submit，所以状态确认放在第 3 步之后。
3. **审核截图和备注**：Pro 商品的 App Review Information 上传**真实的付费墙截图**（不是设计稿；我在代码阶段完成后告诉怎么截）；备注写：`Open Settings → Pro, or choose IGES in the Preview screen and tap Convert. Pro is a one-time non-consumable purchase that unlocks IGES export and large models (over 50,000 faces). Restore Purchases is on the paywall and in Settings.`（M1 版本；M2 完成后才追加 batch）。做完确认：该商品状态仍为 Ready to Submit。
4. **同一次提交**：打赏商品和 Pro 商品都是各自类型的第一个内购，安排和 App 的**首发版本**一起送审（App Review → Drafts 里确认三项都在）。**本方案没有送审授权**，上传/送审/发布仍需老板另外决定。
5. **Apple 小企业计划（15% 抽成）**：只有 Account Holder 能申请，在 ASC 的 Business / Agreements 附近或 Apple 官网小企业计划页面申请（入口以 Apple 当前页面为准）。越早越好，生效有延迟。我没有核实现在的具体页面。
6. **测试**：沙盒测试员已在 ASC_STEPS A8。TestFlight 里的内购走沙盒，不真实扣款。

### 9b. 统一首发送审清单（覆盖 tip-iap/ASC_STEPS.md 里已失效的内容）

打赏分支的 `ASC_STEPS.md` 是按“没有 Pro”写的，Pro 首发后下面几处**作废，以本节为准**（代码阶段会把 tip-iap 的那份同步改掉，并在 Pro 分支的 ASC_STEPS 里给出完整版）：

| 原位置 | 原内容 | 现在 |
| --- | --- | --- |
| B2 打赏商品审核备注 | “… Existing STL to STEP/IGES conversion remains free … There are no non-consumable purchases …” | 改为：`Open Settings → Support Development. The Small Tip is an optional, repeatable consumable In-App Purchase. It provides no content, feature unlock, credits, or additional conversion limits. A completed tip is consumed immediately and is not restorable. STL to STEP conversion remains available without any purchase; IGES export and models over 50,000 faces are part of the separate STL2STP Pro purchase.` |
| B4 第 8 条 | “设置里没有恢复购买和 Pro 入口” | 作废：设置里**有** Pro 状态行和“恢复购买”；测试步骤换成 §11 的 Pro 测试表 |
| B5 第 4 条 App 版本备注 | 复制 B2 的旧文本 | 改为同时说明两个商品：`Settings → Support Development: optional consumable tip, unlocks nothing. Paywall (choose IGES in the Preview screen and tap Convert, or open a model over 50,000 faces): STL2STP Pro, a one-time non-consumable purchase that unlocks IGES export and large models. Restore Purchases is on the paywall and in Settings. No account or external payment is required. This submission includes the app version and both in-app purchases.`（M2 完成后追加批量） |
| 已经填进 ASC 的内容 | 如果老板已按旧备注填过 | 以上三处都要在 ASC 里**重新核对并改掉**，截图也要重截（含 Pro 入口） |

### 10. 工作量（工程日）

| 项 | M1 | M2 |
| --- | --- | --- |
| 原生 Pro（权益重算、购买、恢复、updates）及 Swift 测试 | 1.5 |  |
| Dart：IosStoreService Pro、限额配置、`ensureAllowed` 触发、减面出路 | 1 |  |
| 付费墙重做、设置页状态/恢复、双语文案、i18n 测试 | 1.5 |  |
| StoreKit 测试配置（`.storekit`）、Dart/Swift 测试 | 1 |  |
| 自审 + 独立评审修正 + 材料 | 1 |  |
| 批量：多选、共享设置、队列页、内存/后台/取消、测试 |  | 3–4（暂估） |
| 合计 | **约 5–6** | **约 3–4（暂估）** |

不含：Codemagic 等待、真机沙盒验收（只有 iPhone 16 一台）、ASC 配置和 Apple 审核时间。深度报告估的 2–3 人日只覆盖了 M1 里“打开已有代码”的部分。

### 11. 测试方法

| 层 | 内容 | 能在哪跑 |
| --- | --- | --- |
| Dart 单元 | `FreeTierLimits`：50k 面边界（减面后）、IGES、批量、元数据不受限、已购绕过；触发原因映射；`IosStoreService` 用假通道测购买/取消/等待/未验证/恢复/事件更新缓存 | 本机 `flutter test` |
| Dart 组件 | 付费墙价格加载中/就绪/失败、按触发原因的说明、减面出路、恢复三种结果；设置页状态行；中英文截屏式断言（无中文漏翻，现有 `i18n_test` 扩展到 `pro.*`） | 本机 |
| 付费墙契约 | 三种结果；继续前重检（100,000 面+IGES：不放行免费减面，只给“减面并导出 STEP”；仅 IGES：给“改用 STEP”；>1,000,000 面：无减面按钮；减面比例的实际 `ConvertRequest` 用的是新设置）；M1 文案不含批量；缓存 true + 恢复空结果→false，恢复失败→缓存不动，离线冷启动用缓存、重算后以原生为准 | 本机 |
| 防回归 | 免费用户：STEP 单文件、预设、元数据、历史、分享全部不受限；打赏不改 Pro 状态；非 iOS 限额仍为 none | 本机 |
| Swift | **（任务：现有 `ios-simulator` 工作流只跑 Flutter 测试和构建，RunnerTests 目前只有空示例，不会执行 XCTest。代码阶段要新增：接线 RunnerTests、提交 `.storekit` 测试配置、在 Codemagic 增加一步模拟器 `xcodebuild test`——这要改 `codemagic.yaml`，需老板同意；结果记录对应提交号，并区分“Swift 编译通过”和“XCTest 执行通过”。）** 权益重算逻辑（撤销、非消耗型类型校验、bundleID）抽成纯函数做 XCTest；`.storekit` 配置做 StoreKitTest（购买、退款、询问购买） | Codemagic `ios-simulator` / Mac；**本机无 Swift 编译器，BLOCKED_ENV** |
| 真机沙盒 | iPhone 16 + 沙盒账号 + TestFlight：买、取消、询问购买、重装后恢复、两次购买互斥、断网 | 需老板授权上传（目前没有） |

覆盖不了的：iOS 15/16（只有一台最新系统的 iPhone 16）；App 关闭期间退款只能用 StoreKitTest 模拟，真机沙盒无法精确触发。会在验收记录里如实写明。

## Risks

1. **批量不存在**（最大范围风险）：见 §1/§8。M1 和 M2 分开，M1 不宣传批量。
2. **付费改动会改变所有 iOS 用户的体验**：免费版 50k 面以上会撞墙。风险是首批 0 评分时转化和评价都可能受影响；缓解：减面出路、不设每日限制、元数据免费。上限数值需老板确认。
3. **Swift 没被编译过**：构建 6b0401da 是第一次。代码阶段前必须先确认它通过。
4. **审核**：本项目安排两个商品随首发版本同提（送审前核验 Apple 当前规则）；付费墙必须只宣传已存在的功能；功能解锁必须走 IAP（做到了，没有任何外链/激活码）；3.1.1 要求恢复入口（有）。我没有联网重查 Apple 条款原文，只依赖深度报告和已有知识，**送审前须由老板或我重查**。
5. **简化权益模型的取舍**：以 `currentEntitlements` 为唯一权威，离线新设备需恢复一次（§7.4）。
6. **旧 Pro 商品状态未知**（ASC 里是否已存在、配置是否对）：§9.1。
7. **内购对象“打赏”和“Pro”共用购买锁**：确保不会同时弹两张购买单。

## Test plan

见 §11。完成定义（代码阶段）：Dart 测试与 `flutter analyze` 通过；Swift 在 Codemagic `ios-simulator` 编译并跑过 XCTest（需要老板手动启动）；独立评审 `APPROVE`；真机沙盒验收在老板另行授权上传之后进行，没做就明确写“未验证”。

## Out of scope

订阅、消耗型次数包、服务器收据验证、跨 App 套装、每日次数限制、元数据收费、特征识别功能、Windows 桌面版企业授权、国区单独定价、广告、中国大陆以外的本地化商店页文案、上传/送审/发布（都需要老板另外授权）。本阶段不写任何代码、不推送、不开 PR、不启动构建、不动 master。

### 14. 第 1 轮评审（Codex，CHANGES）处理记录

| 编号 | 级别 | 处理 |
| --- | --- | --- |
| PRO-P1 | major | 采纳：§4/§5 分出 M1/M2 两套标题与商品描述，`kBatchAvailable` 控制，M1 组件测试断言不含批量 |
| PRO-P2 | major | 采纳：§5 定义三种结果、继续前重检、IGES+大文件时只给“减面并导出 STEP”、比例边界（>1,000,000 面不显示）、补测试（§11） |
| PRO-P3 | major | 采纳：新增 §9b，列出作废的旧备注/步骤和新备注 |
| PRO-P4 | minor | 采纳：§6 定死缓存规则，枚举新增 `unverified`/`busy`，§11 补离线重启/恢复空结果测试 |
| PRO-P5 | minor | 采纳：§8 改用 `pickFiles()`、排除拖放批量、补队列规则，工期改为 3–4 天暂估 |
| PRO-P6 | minor | 采纳：§11 Swift 行加“接线 RunnerTests、`.storekit`、Codemagic 增加 xcodebuild test”任务，需老板同意改 codemagic.yaml |
| PRO-P7 | minor | 采纳：§4/§9 改为“本项目安排”，Ready to Submit 放在上传截图之后 |

## Notes for bobo

### 12. 需要老板决定的事

1. **要不要等批量一起上架？** 推荐 M1 先上（IGES + 大文件），批量 M2 再加；或者全部做完再上（多约 3 个工程日）。
2. **IGES 和大文件放进 Pro**：代码里今天是免费的，TestFlight 测试员看到的版本里是免费的。App 没公开上架，所以我认为不算“收回已有功能”，请您确认。
3. **大文件上限 50,000 面**（来自市场调研建议，可调）。更高（如 100,000）转化更低但评价风险更小。
4. **文件信息（元数据）继续免费**，我的建议，和深度报告不同。
5. **中文也叫 Pro**（取代“完整版”）。
6. **感叹号**：`pro.success` “已解锁 Pro，感谢支持！”是否同样豁免（需在 `core_test` 里再加一个键）。
7. **商品 ID**：先核对 ASC 里有没有 `com.stl2stp.app.pro`；不行就用 `com.stl2stp.app.pro.v2`。
8. **家庭共享关闭**，同意吗？
9. **小企业计划**由 Account Holder 尽快申请。
10. 代码阶段前：Codemagic 6b0401da 必须先通过；上传、送审、发布仍然没有授权。
