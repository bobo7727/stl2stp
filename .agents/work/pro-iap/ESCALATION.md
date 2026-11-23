# Escalation — code phase unresolved after 3 rounds

- feature_dir: .agents/work/pro-iap
- phase: code
- rounds: 3/3（第 1 轮 e08b915+b0abdd5；批量轮 701431b+a9f8ea3；第 3 轮修复 ff93c32）
- verdicts: HANDOFF_VERDICT.code-round-1.md / -round-2.md / -round-3.md（均 CHANGES）

## Disagreement / what is open

1. **PRO-C1（major，部分解决）**：5 万面限制已改为按减面后的真实面数检查（FfiEngine、DemoEngine、批量都传入 maxFaces）。Codex 还要求：减面后的真实面数也要重查**内存预算**（免费和 Pro 都适用），因为预检用的是“请求的目标”，TARGET_NOT_REACHED 时真实面数更大。Claude 在 STATE 里曾判断现有预检足够，Codex 举例（4 万面、500 MB 预算）说明不够——**Claude 认可 Codex 的例子成立**。
2. **BATCH-002（minor）**：内容去重在一次选择内有效，但“继续添加”再选同一个非本地文件会重复入队（每次调用的 seen 集合是新的）。
3. **PRO-C3（已约定推迟）**：StoreKitTest/.storekit 覆盖需要 Xcode，放到 Codemagic；现有 RunnerTests 步骤本身不能提供这些场景，需另写集成测试，并且不能靠 ignore_failure 放过。
4. **验证**：Codex 沙箱无法写 Flutter SDK 缓存，三轮均为 tests: BLOCKED_SANDBOX；Flutter 133 个测试通过只有 Claude 侧日志（logs/c3_test.log）。Swift 未编译（BLOCKED_ENV）。

## Options

A. 老板批准一个**范围很小的修复**（不开新的 Codex 评审轮）：在 worker 里减面后用真实面数做内存检查（C1），并让“继续添加”跨次去重（BATCH-002），Claude 自测；然后老板直接看 diff。
B. 允许再开一轮 Codex 评审（需要老板明确授权，规则默认不开第 4 轮）。
C. 现在就交 diff：C1 的内存部分与 BATCH-002 作为已知问题记录，C3 在 Codemagic 上验证。

## Claude 的建议

选 A，再配合老板亲自看 diff；两处修复都小。C1 内存检查：在 `_convertWorker` 的 maxFaces 检查旁边，用 `MemoryGuard` 同一模型按真实面数估算（不再乘减面比例），超预算则返回 errorMemory，不写文件。BATCH-002：BatchScreen 记住已导入的内容身份。未经 Codex 复审的改动需标明。

## Codex 的立场

CHANGES（0 blocker / 1 major / 1 minor）；C3 推迟可接受且不单独触发新轮；明确写“不要开第 4 轮”。
