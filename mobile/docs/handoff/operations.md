# 运营相关：内购、估算/内存系数、iOS 原生部分、发版

[← 返回总入口](../HANDOFF.md)

## 目录

1. [内购（模拟商店 / StoreKit）](#1-内购模拟商店--storekit)
2. [估算与内存模型的系数及重新标定](#2-估算与内存模型的系数及重新标定)
3. [iOS 原生部分（后台、内存）](#3-ios-原生部分后台内存)
4. [版本号与发版流程](#4-版本号与发版流程)

---

## 1. 内购（模拟商店 / StoreKit）

详细设计见 [flutter-app.md §8](../flutter-app.md) 和 [mac-setup-guide.md §9.1](../mac-setup-guide.md)。要点：

| 项 | 当前值 | 位置 |
|---|---|---|
| 商业模式 | 一次性买断“完整版”（Non-Consumable），只有一个商品 | `purchase/purchase_service.dart` |
| 商品 ID | `com.stl2stp.app.pro`（必须与 App Store Connect 一致） | `PurchaseConfig.proProductId` |
| 价格 | 代码里**没有价格**，显示商店返回的本地化价格；未返回时显示“购买（价格待定）” | `ProductInfo.priceLabel` |
| 免费额度 | `FreeTierLimits.none`：**不限制任何东西** | `purchase/free_tier.dart`、`AppState(limits:)` |
| 可配置的限制 | `maxTriangles`（按减面后的面数算）、`dailyConversions`、`igesRequiresPro`、`metaRequiresPro` | 同上 |
| 实现选择 | iOS/macOS/Android → `StorePurchaseService`（`in_app_purchase` / StoreKit）；其它 → `MockPurchaseService`；`STL2STP_MOCK_STORE=1` 强制模拟 | `purchase/purchase.dart` |
| 购买状态存储 | 本地 `SharedPreferences` 标记（**没有服务端/收据校验**） | |
| 恢复购买 | 12 秒内商店没回应 → “没有可恢复的购买” | `store_purchase_service.dart` |
| 测试 | 模拟商店：7 个单元/组件测试 + 集成测试；StoreKit：**未测试**（需要 Mac + 沙盒账号） | |

触发点：点“转换”时 `ensureAllowed()` → `AppState.limitFor(faces, settings)`；超限 → 购买页（顶部说明原因）→ 买完返回 true 继续转换。设置页“当前版本”也能进购买页。

启用免费额度的示例（改 `main.dart` 的 `createAppState` 里构造 `AppState` 的参数）：见 [recipes.md R7](recipes.md#r7-修改商品-id价格档位或免费额度)。

上架前必做：App Store Connect 建同 ID 商品、填价格档位；StoreKit 配置文件做模拟器测试；沙盒账号真机测试；决定是否需要收据校验（单机 App 一般接受本地标记的风险）；把购买页的黄色占位横幅和“待定”标签去掉（`paywall_screen.dart`）。

## 2. 估算与内存模型的系数及重新标定

### 2.1 时间与大小估算（`lib/core/estimate.dart`）

```text
F（写出的 CAD 面数）= f（减面后的三角形数）；合并共面时 ≈ 平面数 × √(f/n)（平面数由 Dart 端 PlaneCounter 统计）
用时 = base + exportPerFace·F (+ mergePerTriangle·f 若合并) ；缝合 ×sewingFactor
      + verifyPerMB·输出MB（STEP 且校验）+ simplifyPerTriangle·n（减面）；整体 ×deviceFactor
大小 = headerBytes + 每面字节·F；缝合 ×1.05；合并后的多边形面 ×1.3；合并后面数 < 5% 三角形数（平板/方块）时 ×0.6
范围：用时 ×[modelLo·deviceLo, modelHi·deviceHi]；大小 ×[0.9, 1.1]；合并时面数取 0.7–1.4 倍；平面数未知时 2%–100%
```

| 系数（`Coefficients.calibrated`） | 值 | 来源 |
|---|---|---|
| `baseSeconds` | 0.07 s | calib.csv 拟合 |
| `exportPerFace` | 0.33 ms/面 | 同上 |
| `mergePerTriangle` | 0.34 ms/三角形 | 同上 |
| `sewingFactor` | 1.25 | 同上（实测慢 20–25%） |
| `verifyPerMB` | 0.18 s/MB | 回读耗时 |
| `simplifyPerTriangle` | 6e-6 s | calib2.csv |
| `headerBytes` | 8000 B | |
| `stepBytesPerFace` / `igesBytesPerFace` | 2400 / 1870 B | |
| `mergedFaceBytesFactor` / `sewingBytesFactor` / `planarFaceBytesFactor` | 1.3 / 1.05 / 0.6 | |
| `modelLo` / `modelHi` | 0.75 / 1.35 | 覆盖空闲机测量的散布 |
| `sizeLo` / `sizeHi` | 0.9 / 1.1 | |
| `Estimator.deviceFactor` | 1.0（默认） | 手机未标定 |
| `deviceLo` / `deviceHi` | 0.75 / 1.5 | 覆盖“机器忙”时测得的慢 1.1–1.9 倍 |

标定数据和结论表见 [flutter-app.md §7](../flutter-app.md)。测试 `core_test.dart` 的 `estimate (calibrated)` 组把每个测量点都放进去：实测必须落在预测范围内，中心值误差 ≤ 35%（时间）/ 20%（大小）。

### 2.2 时间文案

`timeRangeKey()`：上限 < 1 s → `estimate.under_1s`；上限 < 90 s → `est.range_seconds`（约 a–b 秒，下限取整到 ≥1）；否则下限 < 60 s → `est.range_seconds_minutes`（a 秒–b 分钟）；下限 ≥ 60 s → `est.range_minutes`（a–b 分钟）。

### 2.3 内存模型（`lib/core/memory_guard.dart`）

```text
峰值 = 50 MB + 20.5 kB × F；缝合 ×1.06；合并 ×1.0；减面时取 max(上式, 1.2 kB × 原始三角形数)
预算 = 设置覆盖值 memoryBudgetMB
     → iOS MethodChannel 'stl2stp/memory'.available（os_proc_available_memory）× 0.8
     → Linux /proc/meminfo MemAvailable × 0.8
     → 默认 1.5 GB
峰值 > 70% 预算 → 警告；> 预算 → 拒绝；建议比例 = 能回到 70% 以下的最大 5% 档
```

来源：`tool/calib/memtime.csv`（`stl2stp-cli` 的最大 RSS，WSL，Release）：ORIGINAL STEP 5k 面 140 MB、20k 437 MB、50k 1016 MB、102k 2017 MB；缝合约多 5%；合并和 IGES 更少。测试断言模型在这些点上都不低于实测。**手机上的实际占用没有测过。**

另有两个硬阈值在 `AppState`：`largeFaces = 200000`（预览页黄框建议减面）、`limitFaces = 2000000`（直接进“模型太大”页）。

### 2.4 重新标定

步骤见 [recipes.md R4](recipes.md#r4-重新标定估算和内存系数)。原则：
- PC 上的数据决定“形状”（每面耗时、每面字节）；手机只需要一个 `deviceFactor`（实测 ÷ 本表）和内存模型的核对。
- 手机输出写本机闪存，比 WSL 写 `/mnt/d` 快，`deviceFactor` 可能 < 1。

## 3. iOS 原生部分（后台、内存）

Dart 与 iOS 原生通道均已集成；原生实现在 app/ios/Runner/AppDelegate.swift，仍需在 Mac/iPhone 上验收。验证步骤见 [mac-setup-guide.md §9.3](../mac-setup-guide.md#93-后台转换与内存警告)。

| 通道 | 方法 | 原生实现 | Dart 端 |
|---|---|---|---|
| `stl2stp/background` | `begin({name})` → 返回 int id；`end({id})`；原生调用 Dart 的 `expired(id)` | `UIApplication.beginBackgroundTask(expirationHandler:)` / `endBackgroundTask` | `platform/background_task.dart`；只在 iOS 启用；到期 → 自动取消 → 错误页说明 |
| `stl2stp/memory` | `available` → 字节数 | `os_proc_available_memory()` | `MemoryBudget.detect()`；原生没装时（`MissingPluginException`）退回 1.5 GB |
| 系统内存警告 | — | Flutter 自动把 `didReceiveMemoryWarning` 转成 `didHaveMemoryPressure` | `converting_screen.dart` 自动取消 |

iOS 后台时间一般约 30 秒，大模型应提示保持前台（文案已写）。

## 4. 版本号与发版流程

现状：`pubspec.yaml` 是 `version: 1.0.0+1`，设置页显示的是 `settings_screen.dart` 里写死的 `appVersion = '0.1.0'`——**两处不一致**（已知问题）。仓库里没有 CHANGELOG。

建议的约定（接手后执行）：
1. 版本号唯一来源 = `pubspec.yaml` 的 `version: X.Y.Z+N`（X.Y.Z 给用户看，N 每次上传 App Store 递增）。设置页改为读取它（例如加 `package_info_plus`，或构建时 `--dart-define=APP_VERSION=...`）。
2. 语义：Z = 修 bug；Y = 新功能/新选项；X = 不兼容变化（如核心库 options 结构变化导致旧设置不可用）。
3. 新建 `CHANGELOG.md`（根目录），每版一节：日期、用户可见的变化（中文）、核心库变化、已知问题。
4. 发版步骤见 [recipes.md R8](recipes.md#r8-发布新版本)。
