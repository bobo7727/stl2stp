# App Store Connect：Pro 买断与首发送审（给老板本人）

2026-11-24。依据 PLAN.md（含批量转换）。账号准备（付费 App 协议、税务、银行、隐私、沙盒测试员、内部测试员）仍按 `.agents/work/tip-iap/ASC_STEPS.md` 第一部分做，不重复。

**边界**：上传构建、送审、发布都**没有授权**。下面凡涉及这三件事的步骤，等您另外下令。

## 一、现在就能做

### 1. 确认 ASC 里没有 `com.stl2stp.app.pro`
- 页面：`Apps → STL2STP → Monetization → In-App Purchases`（旧版叫 Features → In-App Purchases）。
- 做什么：在列表里找这个 ID。您说没建过。
- 确认：列表里没有。如果有，截图给我，**先不要改它**（类型选错了不能改，要换 ID）。

### 2. 新建 Pro 商品
点 **+**，按下表填：

| 字段 | 填写 |
| --- | --- |
| Type（类型） | **Non-Consumable（非消耗型）**——创建后不能改 |
| Reference Name | `STL2STP Pro`（只您自己看得到） |
| Product ID | `com.stl2stp.app.pro` ——**保存后不能改，删了也不能再用，一字不差** |
| Availability（可售地区） | 和 App 的发布地区一致 |
| Price Schedule（价格） | 基准地区 **United States**，价位 **USD 4.99**；看一眼国际价预览（人民币等由 Apple 换算） |
| Family Sharing（家庭共享） | **保持关闭**（打开后不能再关） |
| Localization：English (U.S.) | Display Name：`STL2STP Pro`；Description：`Unlock IGES export, batch conversion and large files. One-time purchase.` |
| Localization：简体中文 | 显示名：`STL2STP Pro`；描述：`解锁 IGES 导出、批量转换和大文件。一次购买，不是订阅。` |
| Tax Category | 默认，不确定问税务顾问 |

确认：创建成功，商品详情里 ID、类型、价格都对。状态要等第 3 步截图传上去才会变 **Ready to Submit**。

### 3. 申请 Apple 小企业计划（15% 抽成）
只有 Account Holder 能申请。入口以 Apple 当前页面为准（我没核实具体页面，可在 App Store Connect 的 Business / Agreements 附近或 Apple 开发者网站的 App Store Small Business Program 页面找）。条件和生效时间以官方页面为准，生效有延迟，越早越好。

## 二、等代码好了、批量也做完再做

前提：我通知您“代码评审通过”，您审过 diff，并且批量转换已做完测完。**批量没做完就不能上传和送审**（付费墙和商品描述都写了批量）。

### 4. 审核截图和审核备注（Pro 商品）
- 页面：商品详情 → **App Review Information**。
- **Screenshot**：上传**真实 App 里的付费墙截图**（标题、功能行、价格都在）。我会告诉您怎么截。
- **Review Notes**（英文）：
  > Open Settings → Pro, tap Home → Select multiple files, or choose IGES in the Preview screen and tap Convert. STL2STP Pro is a one-time non-consumable purchase that unlocks IGES export, batch conversion and models over 50,000 faces. STEP conversion of smaller models stays free. Restore Purchases is on the paywall and in Settings. No account or external payment is required.
- 确认：商品状态变成 **Ready to Submit**。

### 5. 打赏商品的审核备注（要改，旧的作废）
打赏分支那份 `ASC_STEPS.md` 里的旧备注写着“STEP/IGES 都免费、没有可恢复的购买”，Pro 加入后不对了。**改成：**
> Open Settings → Support Development. The Small Tip is an optional, repeatable consumable In-App Purchase. It provides no content, feature unlock, credits, or additional conversion limits. A completed tip is consumed immediately and is not restorable. STL to STEP conversion remains available without any purchase; IGES export, batch conversion and models over 50,000 faces are part of the separate STL2STP Pro purchase.

### 6. App 版本的审核备注
版本页 → **App Review Information → Notes**：
> Settings → Support Development: optional consumable tip, unlocks nothing. Paywall (choose IGES in the Preview screen and tap Convert, open a model over 50,000 faces, or tap Home → Select multiple files): STL2STP Pro, a one-time non-consumable purchase that unlocks IGES export, batch conversion and large models. Restore Purchases is on the paywall and in Settings. No account or external payment is required. This submission includes the app version and both in-app purchases.

如果您已经按旧备注填过，**这三处都要在 ASC 里重新核对并改掉**，截图也要重截（含 Pro 入口）。

### 7. 测试（TestFlight 里的内购走沙盒，不真实扣款）
需要您另外授权上传。在 iPhone 16（只有这一台，覆盖不了 iOS 15/16）上用沙盒账号依次测：
1. 设置里能看到“支持开发”“Pro”状态和“恢复购买”。
2. 预览页选 IGES 点转换 → 付费墙，价格显示 $4.99（本地化价格）。
3. 买 Pro → 提示已解锁，IGES 能转，超过 5 万面的模型能转，批量入口可用。
4. 取消购买 → 提示已取消，可再次点。
5. 杀掉 App 重开 → 仍是 Pro。
6. 删掉 App 重装 → 设置里点“恢复购买” → 恢复为 Pro。
7. （沙盒里设了“询问购买”）等待批准的提示。
8. 断网：设置/付费墙显示“暂时无法连接 App Store”，转换照常。
9. 免费用户（没买）：STEP 转 5 万面以内的模型正常；5 万面以上先看到“减到 N% 免费转换”。
10. 打赏：买一次、取消、不影响 Pro 状态。

### 8. 同一次提交（需要您另外授权）
- 打赏商品和 Pro 商品都放进同一次首次提交，和 App 的首发版本一起送审（`App Review → Drafts` 里确认三项都在）。Apple 的规则是每种类型的第一个内购要随一个 App 版本送审，送审前请再核对 Apple 当前规则。
- 发布方式选“手动发布”。
