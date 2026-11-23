# Handoff Request

- feature_dir: .agents/work/pro-iap
- phase: plan
- round: 3/3
- mode: full
- branch: feat/pro-iap
- self_review: done
- checkpoint_sha: n/a（方案阶段，无代码）
- reviewed_revision: feat/tip-iap 9e7ef85（分支 feat/pro-iap 与其相同，只多了被忽略的过程文件）
- reviewed_diff_identity: PLAN.md（.agents/work/pro-iap/PLAN.md）
- support_runner: none
- request: REVIEW_PLAN

## Summary
为 iOS 上架加入 Pro 买断（美国 4.99 美元，非消耗型，解锁 IGES、批量、大文件），打赏不变。老板已批准“做 Pro”，方案本身待评审后再请老板批准。

## Scope
只评审方案，不评审代码。

## Files or plan sections
PLAN.md 全文。对照代码：mobile/app/lib/purchase/、lib/screens/{preview,paywall,settings,flow}_screen.dart、ios/Runner/StoreCoordinator.swift。深度报告：/home/xai/.orca/bots/bot/docs/reports/monetization-deep-dive-2026-11.md 的 stl2stp 一节。

## What to check
（第 3/3 轮）本轮只复审老板决定后的变化：Pro 第 1 版含批量（不分两步）、商品新建（ASC 里没有 com.stl2stp.app.pro）。重点：§8 批量设计是否完整、可实现、对内存/后台/取消/失败的规则是否站得住，§9/§10/§11 是否同步，文案/发布门槛是否自洽。第 2 轮 APPROVE 的 verdict 在 HANDOFF_VERDICT.round-2.md。原清单：请先逐条确认第 1 轮 verdict（HANDOFF_VERDICT.round-1.md）的 PRO-P1…P7 是否已在 PLAN.md 解决（对照 §14 处理记录和对应章节），再看有没有新引入的问题。以下为原清单：
1. 免费/付费边界是否合理、有没有把“已对公众可用”的功能收费；IGES/大文件今天免费这件事处理是否站得住。
2. “批量转换不存在”的判断是否属实（读 flow.dart、stl_drop.dart、home_screen.dart），M1/M2 拆分和工作量是否可信。
3. 内购技术栈说明（iOS 原生 StoreKit 2；in_app_purchase 仅非 iOS；无第二个监听）是否与代码和锁定的插件源码一致。
4. 原生权益模型（以 currentEntitlements 为唯一权威、无账本、离线缓存取舍）是否有漏洞：退款、Ask to Buy、重装恢复、未验证交易、打赏与 Pro 互斥。
5. 付费墙时机/文案/恢复购买是否满足 Apple 3.1.1 并避免误导（尤其不宣传未实现的批量）；中英文是否完整。
6. ASC 步骤是否缺漏、有没有错的断言（首个内购随首版同提、家庭共享、小企业计划）。
7. 测试方法与工作量。

## Validation evidence
Plan phase: not required（tests: SKIPPED, build_or_lint: SKIPPED）。

### Test
- runner: n/a
- command: n/a
- working_directory: n/a
- revision_and_diff_identity: n/a
- started_at: n/a
- exit_code: n/a
- log_path: n/a
- log_summary: SKIPPED

### Build or lint
- runner: n/a
- command: n/a
- working_directory: n/a
- revision_and_diff_identity: n/a
- started_at: n/a
- exit_code: n/a
- log_path: n/a
- log_summary: SKIPPED

### Disposable-copy rerun (if needed)
- n/a

## Notes
只读评审；不改文件、不动 git、不联网。
