# dual-agent-review 使用说明

在 Orca 里默认由 **Claude 写方案和代码、Grok 做辅助工作、Codex 只读评审**。两阶段（方案 → 代码）每阶段先自审、再评审，最多 3 轮；3 轮仍未解决就停止，写升级记录并通知协调方或 bobo，不开第 4 轮。

技能名：`dual-agent-review`  
源目录：`/home/xai/orca-agent-kit`

## 新仓库怎么接入

在 Ubuntu 中，从技能源目录运行：

```sh
/home/xai/orca-agent-kit/bin/orca-kit-init /path/to/repo feature-slug
```

`feature-slug` 只用小写字母、数字和单个连字符，例如 `export-mesh`。省略时默认 `feature`；并行功能请各用一个唯一名字。脚本的行为：

- `AGENTS.md`、`CLAUDE.md` 不存在时创建；存在时在末尾追加带 `BEGIN/END dual-agent-review` 标记的规则块，保留原文。重复运行不重复追加，也不自动升级已标记的块。
- `CODEX_REVIEW.md` 和 `DUAL_AGENT_README.md` 放根目录，已有则跳过；不改项目的 `README.md`。
- 六份过程文件放在 `.agents/work/<slug>/`，已有则跳过。再次传另一个 slug 只会增加那个功能的过程文件。
- 给根目录 `.gitignore` 追加一次 `/.agents/work/` 规则，原有内容不变；发现残缺或重复的标记块则报错，请先修复。

然后在项目 `AGENTS.md` 的「Project test and build commands」填好真实构建、测试、lint 命令；已有项目命令可直接沿用。补全新过程模板里的 slug 和分支字段。原有项目规则继续生效，项目可在标记块外用 `## Roles` 覆盖默认角色，流程里的角色名随之替换，实际分工记入功能的 `STATE.md`；角色覆盖不放宽评审只读和合并限制。

不要在 main/master 开发。在 Orca 里新建功能 worktree，通常用 `feat/<slug>` 分支。初始化不创建提交；根目录规则是否提交按项目约定决定，过程文件不提交。

## 分工和省 Claude 额度

| 角色 | 默认负责 |
|---|---|
| Claude | 核心方案、写代码、每次交评审前自审、处理 blocker/major |
| Grok（本机 `grok` CLI，可用至 2026-11-30） | 调研、读文档、实际跑测试和构建、整理日志、PR 描述草稿、按 Claude 的决定更新 TASKS/STATE 和交接文件 |
| Codex 评审调用 | 只读核对方案、代码和测试证据，输出结论，由调用方保存 |
| Grok 不可用时（含 2026-12-01 起） | Codex 用**独立的辅助调用**接杂活，只开放测试和过程文件所需写入；评审调用仍用只读沙箱 |
| bobo | 方案批准、合并前看 diff、3 轮未决时作决定 |

交给辅助方范围明确的小任务，拿简短结论给 Claude，不要让 Claude 重读同一批文档或重整日志。同一 worktree 同时只让一个角色写入，测试和辅助更新结束再开始评审。任务未授权派助手、或环境不能委派时，由实现方完成辅助工作；不要自行扩派。

## 每天在 Orca 里怎么走

下文所有短文件名都指 **`.agents/work/<slug>/`** 中的文件。

1. 在功能 worktree 启动 Claude，说明功能、slug，并要求遵守项目 `AGENTS.md`。
2. **方案阶段**：Claude 写 `PLAN.md` 并自审，辅助方更新 `TASKS.md`、`STATE.md`（`phase: plan`，`round: N/3`）和 `HANDOFF_REQUEST.md`。Codex 只读评审方案，测试和构建状态为 `SKIPPED`。
3. Codex 输出固定格式结论，由可写的调用方保存为 `HANDOFF_VERDICT.md` 并更新 STATE。结论仅 `APPROVE` / `CHANGES`，问题分 blocker / major / minor；只有 blocker 或 major 才需要下一轮，必需检查失败或证据阻塞也按这两级记录。示例见根目录 `CODEX_REVIEW.md`。不要让只读 Codex 自己写结论文件。
4. 方案获 Codex 批准后，向 bobo 申请批准并等待；在 STATE 记录 `plan_approved_by: bobo` 和日期。任务里已明确批准的不用再问；worker 通过下节的 `ask` 向协调方申请。
5. **代码阶段**：每轮开始前在功能分支创建检查点（有改动则仅提交预期文件的本地 WIP，否则记 HEAD SHA），写入 STATE。Claude 实现、自审，**实现方或辅助方实际跑测试及构建/lint**，把证据写入交接文件，再交 Codex。
6. Codex 只读审 diff 和日志，不要求在只读沙箱里编译。每项检查的交接记录包括：执行者、完整命令、工作目录、代码 revision/检查点和 diff 标识、时间、退出码、日志路径、摘要。日志也放本功能目录；测试后改了代码，要重跑受影响的检查。通过 `tee` 记日志时不要丢失真实退出码。
7. 代码轮完成须测试通过、lint（项目有则用它，否则用 build）通过、Codex `APPROVE`。真实失败标 `FAIL`；**只有证据缺失或可疑（过期、矛盾、截断、无法对应本次代码等）才标 `BLOCKED`**，说明需补什么。只读沙箱不能写构建产物本身不构成阻塞。
8. 必要时由实现方/辅助方在临时可写副本复跑，副本必须包含本次评审的准确代码（含预期未提交修改）。在交接里写清复制方法、来源 revision/diff 标识、副本路径、依赖与环境准备、命令、退出码和日志；Codex 仍只读核对证据，不悄悄切换评审沙箱。
9. 小问题记入 STATE，不单独加轮次。最多 3 轮，仍有 blocker/major 就按下节升级。不要合并，把分支/diff 留给 bobo。

## 卡住、批准和完成怎么通知

先写本功能的 `ESCALATION.md`（分歧、选项、Claude 建议、Codex 立场），再通知；只写文件不算通知。下面 `ORCA` 是按 `orca-cli` 技能解析的可执行命令占位符，不要照字面运行；worker 先读该版本的 `orchestration` 指南，并使用当前派发前缀里的 sender、Task/Dispatch ID。

**如果是 Orca 派出的 worker**：必须通知派它的协调方，让跟踪项显示“需要你”。要老板决定时：

```sh
ORCA orchestration ask --from <sender> \
  --question "需要 bobo 决定：<选项>；见 .agents/work/<slug>/ESCALATION.md"
```

申请方案批准时同样走 `ask`，改为指向 `PLAN.md`。发出问题后结束当前回合，等 Orca 带回回复，不本地提问或轮询。若已经阻塞、必须协调方介入：

```sh
ORCA orchestration send --from <sender> --type escalation \
  --subject "Blocked: <原因>" --body "需要 bobo：<决定>；见 .agents/work/<slug>/ESCALATION.md" \
  --task-id <task-id> --dispatch-id <dispatch-id>
```

worker 完成也走协调渠道，按当前派发要求**只发一次**：

```sh
ORCA orchestration send --from <sender> --type worker_done \
  --subject "<结果>" --body "<三句话：做了什么、发现什么、剩下什么>" \
  --task-id <task-id> --dispatch-id <dispatch-id> --outcome succeeded
```

工作未完成用 `--outcome failed`；仅在确有文件时加 `--files-modified` 或 `--report-path`。发送失败按 CLI 打印的恢复步骤处理；确认成功后结束回合，不轮询、不沿用旧 ID 开始新工作。未决升级不能报成成功完成。

**不是 worker** 时，才退回工作区未读提示：

```sh
ORCA worktree set --worktree active --workspace-status in-review --unread \
  --comment "需要 bobo：<决定>；见 .agents/work/<slug>/ESCALATION.md" --json
```

申请方案批准改指向 PLAN；完成时注释写结果和 diff。CLI 无法连接时保留文件、直接告诉 bobo，并记录通知失败；worker 不得用工作区标记代替协调方消息。在 ESCALATION 记下实际渠道和消息回执。

## 快速通道

大约不超过 20 行，且不影响设计、架构、对外 API、schema、协议、安全、依赖或新增功能面时，可跳过方案评审，直接进入代码阶段。在 STATE 写 `mode: fast-path` 和理由。后续发现设计影响，停止编码，转完整方案阶段，并按上节渠道通知。

## 文件清单与合并前检查

| 位置 | 文件 |
|---|---|
| 仓库根目录 | `AGENTS.md`、`CLAUDE.md`、`CODEX_REVIEW.md`（以及日常说明 `DUAL_AGENT_README.md`） |
| `.agents/work/<slug>/` | `PLAN.md`、`TASKS.md`、`STATE.md`、`HANDOFF_REQUEST.md`、`HANDOFF_VERDICT.md`、`ESCALATION.md`、测试日志 |

选择整目录 gitignore，比每次合并前手动删除可靠，也保留本地历史。不要 force-add 这些文件；合并前查 `git ls-files -- .agents/work`，若有已跟踪文件，在功能分支取消其跟踪并保留本地副本（gitignore 不会自动取消跟踪）。旧安装根目录的六份过程文件不会自动迁移或删除，请按功能移入新目录，已跟踪的旧文件也须在功能分支取消跟踪。不要提交 .env 或密钥，不直接提交/推送 main/master；只在 bobo 看过 diff 后由人合并。

维护者可在技能源码目录运行 `python3 tests/test_init.py` 做隔离自测；只在系统临时目录建测试仓库，不向真实项目安装。
