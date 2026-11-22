# dual-agent-review 使用说明

在 Orca 里用 **Claude 写方案/改代码、Codex 只读评审** 的可复用流程。一套规则最多两阶段（方案 → 代码），每阶段 Claude 先自审再交给 Codex，来回最多 3 轮，能少则少；3 轮仍谈不拢就停止并交给 bobo 决断。

技能名：`dual-agent-review`  
源目录：`/home/xai/orca-agent-kit`

## 新仓库怎么接入

在 WSL Ubuntu-20.04 里执行（不会覆盖已有文件）：

```sh
/home/xai/orca-agent-kit/bin/orca-kit-init /path/to/repo
```

然后打开该仓库的 `AGENTS.md`，把「Project test and build commands」改成这个项目真正的构建、测试、lint 命令。

不要在 `main` / `master` 上直接开发。在 Orca 里为功能新建 worktree，分支名用 `feat/<slug>`。

## 每天在 Orca 里怎么走

1. 打开 Orca，把仓库加进侧边栏（若还没加）。在仓库旁点 `+`，新建 worktree，分支用 `feat/<简短英文slug>`，起始引用一般是 `origin/main` 或仓库的 base ref。
2. 在该 worktree 里启动 Claude Code。把要做的功能用一句话说清楚，并提醒它按 `AGENTS.md` / 技能 `dual-agent-review` 走。
3. **方案阶段**（小改走快速通道除外）：Claude 写 `PLAN.md` 和 `TASKS.md`，自审，更新 `STATE.md`（`phase: plan`，`round: N/3`），写出 `HANDOFF_REQUEST.md`。
4. 在同一 worktree **分屏**启动 Codex，或按 Orca 编排把只读评审交给 Codex。Codex 必须用只读沙箱，例如 `codex exec --sandbox read-only`，不要改文件。评审结果写进 `HANDOFF_VERDICT.md`，结论只能是 `APPROVE` 或 `CHANGES`。问题要分成 blocker / major / minor，只有 blocker 或 major 才进入下一轮。
5. 方案被 Codex 批准后，**先等 bobo 点头**，再进入代码阶段。在 `STATE.md` 记下 `plan_approved_by: bobo`。
6. **代码阶段**：每一轮开始前，Claude 只在功能分支上做检查点（WIP 提交，或记下当前 HEAD），写入 `STATE.md`，以便回滚。然后实现、自审，再交给 Codex。Codex 自己跑测试和构建/lint，并做只读评审。
7. 代码轮次的完成标准：测试通过、构建或 lint 通过，并且 Codex 给出 `APPROVE`。最多 3 轮；小问题（minor）不单独开新一轮。
8. 若某阶段 3 轮仍未达成：停止。Claude 写 `ESCALATION.md`（分歧、选项、Claude 建议、Codex 立场）。若在 Orca 终端里，用下面命令给工作区打未读标记（CLI 没有单独的“发通知”命令）：

```sh
orca worktree set --worktree active --workspace-status in-review --unread \
  --comment "ESCALATION: <phase> after 3 rounds; see ESCALATION.md" --json
```

9. 不要合并。把 diff 留给 bobo 看过再合并。不要提交 `.env` 或密钥，不要向 `main` / `master` 直接提交。

## 快速通道（省额度）

预计改动大约不超过 20 行，且不影响设计、架构、对外 API、协议、安全或依赖时，可以跳过方案评审，直接进入代码阶段。在 `STATE.md` 写上 `mode: fast-path` 和一句话理由。若后来发现有设计影响，立刻停手，改走完整方案阶段，并告诉 bobo。

## 需要你在 Orca 界面里确认的事项

这些不能只靠命令可靠完成，请在 Orca 里检查：

1. **加入仓库**：侧边栏点 Add Repo，选本地仓库目录。
2. **Agent Dashboard**：Settings → Experimental → Agent Dashboard，打开后可在左侧看板里看到各 agent 是在工作、等你，还是已完成。
3. **完成通知**：Settings → Notifications，按需要打开 agent 完成时的系统通知、声音或 worktree 芯片提示。
4. **Orca CLI**：在 Settings 里注册 Orca CLI（文档有的版本写在 General → Orca CLI，有的写在 Experimental → CLI）。在 Orca 自己的终端里用 `orca`，不要在普通系统终端里裸跑 `orca`。
5. **编排（可选）**：编排是实验功能。文档写要在 Settings → Experimental 打开；编排面板和开关目前可能暂不可用。若要用 CLI 做 `worker-start` / `dispatch`，先在 **Orca 终端**里运行 `orca status --json`，再 `orca skills get orchestration --full`，不要凭记忆造参数。

## 文件清单

| 文件 | 用途 |
|---|---|
| `AGENTS.md` | 双方共用规则 |
| `CLAUDE.md` | 引用 AGENTS.md，并写明 Claude 是实现者 |
| `CODEX_REVIEW.md` | Codex 只读评审清单 |
| `PLAN.md` / `TASKS.md` / `STATE.md` | 方案、任务、阶段与轮次日志 |
| `HANDOFF_REQUEST.md` / `HANDOFF_VERDICT.md` | 固定格式交接，便于解析 |
| `ESCALATION.md` | 3 轮未决时升级给 bobo |