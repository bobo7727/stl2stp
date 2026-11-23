# Handoff Verdict

- feature_dir: .agents/work/pro-iap
- phase: plan
- round: 1/3
- mode: full
- verdict: CHANGES
- reviewed_revision: 交接声明的 feat/pro-iap，基于 9e7ef85；本轮未执行 git 命令核验
- reviewed_diff_identity: .agents/work/pro-iap/PLAN.md（2026-11-23，方案第 1/3 轮）
- tests: SKIPPED
- build_or_lint: SKIPPED

## Evidence checked

- test_log_and_result: SKIPPED；方案阶段未运行测试。
- build_or_lint_log_and_result: SKIPPED；方案阶段未运行构建或 lint。
- revision_diff_and_coverage_match: 已阅读 HANDOFF_REQUEST.md、PLAN.md、STATE.md、TASKS.md、指定 verdict 模板；对照 purchase/ 全部实现、preview/paywall/settings/flow/home 页面、stl_drop.dart、AppState、main.dart、StoreCoordinator.swift、StoreChannel.swift、AppDelegate.swift、相关测试、codemagic.yaml、内存校准文档及深度报告 stl2stp 一节。另核对本机锁定的 in_app_purchase_storekit 0.4.13、file_picker 13.1.0 源码及被方案引用的 ASC_STEPS.md。
- missing_or_suspicious_evidence: 未联网核验 Apple 当前规则、ASC 商品状态、公开上架状态及 Codemagic 结果；这些仍是待核实前提。本轮无文件修改、无 git 操作、无网络访问。

## Issues

### blocker

none

### major

- id: PRO-P1
  file: .agents/work/pro-iap/PLAN.md:90、121、184、191
  summary: M1“不宣传未实现批量”的要求尚未落实到可直接使用的文案。商品中英文描述和 pro.title 仍包含批量，只有 pro.feature.batch 标明 M2 前隐藏；§9 又要求老板照 §4 填 ASC。按当前步骤执行，M1 仍会销售或宣传尚不存在的功能。
  suggested_fix: 明确列出 M1、M2 两套标题和商品描述；M1 所有付费墙、ASC 本地化、截图及备注均只写 IGES 和大文件。增加 M1 中英文组件断言，确认标题、正文和功能行均不出现批量承诺。

- id: PRO-P2
  file: .agents/work/pro-iap/PLAN.md:111–112；mobile/app/lib/purchase/free_tier.dart；mobile/app/lib/screens/preview_screen.dart:68、394
  summary: “减面后免费转换”没有定义完整的放行条件和设置回传方式。现有检查先判面数，再判 IGES：例如 100,000 面且选择 IGES，首先触发大文件墙，减到 50% 后仍需 Pro。若直接继续会绕过 IGES 收费；若再次拦截，则“免费转换”承诺不成立。另外当前比例滑块仅支持 5%–95%、每档 5%；超过 1,000,000 面时，最低 5% 仍超过免费上限。现有 ensureAllowed() 只返回 bool，也无法表达“采用修改后的设置继续”。
  suggested_fix: 明确购买、取消、采用免费设置三种结果及设置回传契约；继续前重新检查全部限制并执行内存检查。对 IGES 与大文件同时命中，明确提供经用户选择的“减面并导出 STEP”，或不显示免费转换按钮；比例必须满足支持范围，无法达到上限时不要承诺免费。补充组合限制、比例边界及实际 ConvertRequest 使用新设置的测试。

- id: PRO-P3
  file: .agents/work/pro-iap/PLAN.md:188；.agents/work/tip-iap/ASC_STEPS.md:139、159、170
  summary: 沿用的打赏送审材料与 Pro 首发直接冲突。旧 Review Notes 写着 STEP/IGES 均免费、没有非消耗型购买、没有恢复内容；旧验收步骤要求设置页不存在 Pro 和恢复入口，App 版本备注还要求复制该段旧文本。方案目前只新增 Pro 商品备注，没有明确替换这些内容。
  suggested_fix: 在 Pro 方案中提供统一的首发送审清单和新版备注，明确覆盖旧 B2、B4、B5 中失效内容，并同步更新已填入 ASC 的打赏商品及 App 版本备注。保留“打赏不授予权益”，同时准确说明 Pro 的收费边界和恢复入口。

### minor

- id: PRO-P4
  file: .agents/work/pro-iap/PLAN.md:158、165–170
  summary: §6“没找到不影响任何已有状态”与 §7“空权益结果权威地写回 false”存在冲突；此外购买结果计划区分 unverified，但现有 PurchaseOutcome 没有该值，“PurchaseService 抽象不变”需要说明映射方式。
  suggested_fix: 明确同步成功且权益为空时撤销缓存，登录取消或同步失败时如何处理缓存；明确 unverified、busy 的结果映射和双语反馈。补充已有缓存为 true 时的恢复空结果、恢复失败、离线重启测试。

- id: PRO-P5
  file: .agents/work/pro-iap/PLAN.md:180–184、215–219
  summary: M2 的三个工程日应视为暂估。锁定的 file_picker 13.1.0 的 pickFiles() 没有 allowMultiple 参数；现有拖放在 iOS 是空包装，“拖放同理”若包含 iPad，就需要额外原生实现。测试表也未覆盖队列本身的失败继续、取消、内存压力及后台到期行为。
  suggested_fix: 使用锁定版本的实际多选 API；明确 iPad 原生拖放是否排除。补充共享设置入口、队列停止规则及上述测试，再确认 M2 工期；尤其区分单文件失败可继续与系统内存压力、后台到期应停止队列。

- id: PRO-P6
  file: .agents/work/pro-iap/PLAN.md:218、235；codemagic.yaml:31–67
  summary: 当前 ios-simulator 工作流运行 Flutter 测试并构建 App，没有执行 XCTest/StoreKitTest；RunnerTests 目前也只有空示例。仅启动现有工作流不能满足方案写出的 Swift 测试完成条件。
  suggested_fix: 将测试目标接线、StoreKit 配置和模拟器 xcodebuild test 步骤明确纳入实施任务，并保存测试结果及对应提交身份；区分“Swift 编译通过”和“Swift 测试执行通过”。

- id: PRO-P7
  file: .agents/work/pro-iap/PLAN.md:97、193、228
  summary: “首个内购随新 App 版本提交”和“必须随 App 首个版本提交”被混用。当前两个商品随首发一起提交是合理发布安排，但不应将它写成只能随 App 首版提交的普遍要求。
  suggested_fix: 改为本项目的提交安排，并保留送审前核验 Apple 当前规则的步骤。ASC 第 2 步也不宜在尚未上传第 3 步审核截图前就要求确认 Ready to Submit。

## Notes

免费保留单文件 STEP、元数据、减面且不设每日次数，作为首发边界合理；IGES 和大文件目前免费属实，但“未公开上架、无正式买家”仍需老板确认。

批量尚不存在、iOS 使用原生 StoreKit 2 的判断与代码一致。锁定插件的交易监听需显式启动，本轮未发现第二个活动的 StoreKit 2 监听。以 verified currentEntitlements 为权威、不建账本或服务器，对当前单一非消耗型商品是可行方案；离线取舍应通过已有购买离线重启、退款及恢复测试验证。

先修正上述 major，再进入方案下一轮；本轮结论不授权开始代码阶段。