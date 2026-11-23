# Tasks

- feature_dir: .agents/work/pro-iap
- phase: plan
- round: 3/3
- branch: feat/pro-iap

## Plan
- [x] 读深度报告 stl2stp 一节、现有代码（free_tier、paywall、preview、flow、StoreCoordinator）
- [x] 核实内购实际技术栈（iOS 原生 StoreKit 2；in_app_purchase 仅 macOS/Android；master 无 StoreCoordinator）
- [x] 写 PLAN.md 并自审
- [x] 独立只读评审（Codex）：round 2/3 APPROVE
- [ ] 评审通过后停下，回报协调方，等老板批准

## Implementation（老板批准后才开始；先确认 Codemagic 6b0401da 通过）
- [ ] M1 原生 Pro（currentEntitlements、purchase、restore、updates）+ Swift 测试
- [ ] M1 Dart：IosStoreService Pro、FreeTierLimits.release、触发与减面出路
- [ ] M1 付费墙重做、设置页、pro.* 双语文案、i18n 测试
- [ ] M1 StoreKit 测试配置与测试
- [ ] M2 批量转换
- [ ] 每轮前检查点；Codemagic/上传/送审/发布均需老板另行授权
