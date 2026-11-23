# Handoff Verdict

- feature_dir: .agents/work/pro-iap
- phase: plan
- round: 3/3
- mode: full
- verdict: CHANGES
- reviewed_revision: 交接声明的 feat/pro-iap，基于 9e7ef85；遵照要求未执行 git 命令核验
- reviewed_diff_identity: PLAN.md SHA-256 ae1675283c901bb599c94a1eeb68bff1e797ff972984e3e77f88ed86510b78c3
- tests: SKIPPED
- build_or_lint: SKIPPED

## Evidence checked

- test_log_and_result: SKIPPED；方案阶段未运行测试。
- build_or_lint_log_and_result: SKIPPED；方案阶段未运行构建或 lint。
- revision_diff_and_coverage_match: 已读完整 PLAN.md、HANDOFF_REQUEST.md、前两轮 verdict、指定模板、STATE.md、TASKS.md、AGENTS.md 和 CODEX_REVIEW.md；对照指定页面（实际文件名为 flow.dart）、engine/memory_guard/models、AppState、BackgroundTasks，并补查 FfiEngine、HistoryStore、FreeTierLimits 和本机锁定的 file_picker 源码。
- missing_or_suspicious_evidence: 未联网，未独立核验 ASC、Apple 当前规则或 Codemagic 结果。无文件修改、无 git 操作。STATE.md 的下一步指令与交接中的人工审批要求冲突，见 PRO-P11。

## Issues

### blocker

none

### major

- id: PRO-P10
  file: .agents/work/pro-iap/PLAN.md:196、203、208；mobile/app/lib/state/app_state.dart:181
  summary: 批量只明确在首页入口检查权益，没有定义选择文件、检查面数之后，点击“开始转换”时的最终放行检查。用户进入批量设置页后，原生首次权益重算或退款事件可能把缓存 true 改成 false；按当前队列规则仍可启动批量并导出 IGES、大文件。§5 的最终检查明确落在预览页，现有 limitFor() 也没有批量参数，不能自然覆盖新路径。这与“原生结果权威、撤销即时生效”的规则存在缺口。
  suggested_fix: 明确批量开始前按当前权益和 batchRequiresPro 重新检查，购买或恢复后同样重检；未获授权不得启动队列，保留已选文件和设置。定义运行期间权益撤销的处理策略，例如允许当前批次完成，或当前文件结束后停止。§11 增加“进入设置页后权益由 true 变 false”的测试，以及非 iOS none 限额不受影响的测试。

- id: PRO-P11
  file: .agents/work/pro-iap/STATE.md:38；.agents/work/pro-iap/HANDOFF_REQUEST.md:16；.agents/work/pro-iap/TASKS.md:13
  summary: STATE.md 写“通过后：记 plan_approved_by: bobo……进入代码阶段”，但交接明确说方案本身仍待评审后请老板批准，STATE 顶部也确认尚未审批。老板决定首版包含哪些功能，不能自动作为对本轮新增批量设计及规则的完整审批。该指令会让后续执行者跳过人工方案审批。
  suggested_fix: 删除评审通过后自动填写审批人并进入代码阶段的指令。只有收到 bobo 对最终方案的明确批准后才能记录审批；APPROVE 后保持 waiting-for-bobo。若本轮存在未解决的 major，则按第 3/3 轮规则交由 bobo 决定。

### minor

- id: PRO-P12
  file: .agents/work/pro-iap/PLAN.md:208、268；mobile/app/lib/core/ffi_engine.dart:153、185、290、373
  summary: “顺序 await convert”尚不能保证前一个文件的原生资源已经释放。FfiEngine 在收到 done 消息时完成 Future，而 worker 发送消息后才进入 finally 销毁 handle、释放分配；下一次转换可能与前一次清理重叠。仅用假引擎验证调用顺序，无法证明方案所述的资源串行边界。
  suggested_fix: 将“转换完成包含原生清理完成”纳入引擎契约及实施范围，例如清理后再发送最终结果，或等待明确的清理完成信号。检查阶段也应明确完成后才能启动转换，离开设置页后不再继续启动后续 inspect。

- id: PRO-P13
  file: .agents/work/pro-iap/PLAN.md:197、202；mobile/app/lib/screens/flow.dart:40
  summary: 非本地文件若照现有 pickStl 的方式复制到 temp/文件名，两个来源不同但都叫 part.stl 的文件会覆盖彼此，随后按临时路径去重还可能丢掉一个输入。输出名加 (2) 无法解决输入已被覆盖的问题。当前 iOS 插件的文档选择路径已使用 UUID 子目录，因此该问题主要在方案承诺的非本地复制分支。
  suggested_fix: 为每个导入文件使用独立临时目录，保留原始文件名及来源身份；按来源身份去重，不按复制后的重名路径去重。补充“不同来源、相同文件名、不同内容”的导入测试。

- id: PRO-P14
  file: .agents/work/pro-iap/PLAN.md:208、211、218、268
  summary: 停止整批时只定义了剩余文件为 notRun，没有定义当前被取消文件的最终状态。状态枚举没有 cancelled，而摘要只有成功、失败、跳过、未运行四类；因此当前文件如何计数、显示什么原因尚不确定，尤其是取消与成功结果同时到达时。
  suggested_fix: 明确当前文件取消后的归类及停止原因字段，并规定成功结果先完成时保留 done；其余文件必须进入唯一终态。增加取消发生在文件运行中、文件间隙和完成边界的测试，保证摘要总数始终等于所选文件数。

## Notes

前两轮意见的文本复核结果：

| 编号 | 本轮结论 |
| --- | --- |
| PRO-P1 | 首版含批量后，一套文案与发布门槛一致；原 M1/M2 处理记录可作为历史保留。 |
| PRO-P2 | 三结果契约、组合限制及免费设置回传仍完整；旧 bool 句已删除。 |
| PRO-P3 | §9b 仍明确覆盖旧送审材料，批量承诺已同步。 |
| PRO-P4 | 恢复缓存及 unverified/busy 规则保持一致。 |
| PRO-P5 | 多选 API、拖放排除、共享设置、失败继续和停止规则已补全；新增边界见本轮问题。 |
| PRO-P6 | XCTest 接线、StoreKit 配置及 Codemagic 执行任务仍在方案内。 |
| PRO-P7–P9 | 上轮指出的普遍性断言、bool 旧句及顶部轮次问题已修正。 |

批量整体架构可实现：串行引擎调用、共享设置、逐文件历史、单次后台任务，以及单文件失败继续、系统压力停止的区分均合理。§9 的首发商品描述和 §11 的主要批量验收项已同步；§10 各项实际合计为约 **9–10 个工程日**，仍应保留暂估性质。

本轮为最后一轮，结论为 **CHANGES（0 blocker、2 major、3 minor）**。不得开启第 4 轮；由可写调用方保存 verdict、更新状态并准备 ESCALATION.md，交 bobo 决定。此次评审未授权进入代码阶段。