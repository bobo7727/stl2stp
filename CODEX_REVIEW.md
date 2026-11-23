# Codex reviewer checklist (read-only)

This checklist applies to the **review invocation**, including when a separate Codex support invocation handles chores because Grok is unavailable. Review does not edit source, process files, config, or git state. Follow `AGENTS.md` and any project `Roles` override while preserving this sandbox boundary.

## Invocation

From the repository root, the writable caller saves the output:

```sh
slug=feature-slug # replace with the initialized feature slug
work_dir=".agents/work/$slug"
codex exec --sandbox read-only --cd "$PWD" \
  "Review $work_dir/HANDOFF_REQUEST.md using CODEX_REVIEW.md; return the $work_dir/HANDOFF_VERDICT.md template as output without editing files." \
  > "$work_dir/HANDOFF_VERDICT.pending.md"
```

The caller checks process exit and verdict format before replacing `HANDOFF_VERDICT.md` with the pending output. This keeps the template/prior verdict readable during review; failed or malformed output is not approval.

Never silently switch the review sandbox to writable. Never run `git commit`, `git add`, `git reset`, or file edits. A disposable-copy rerun is performed by the implementer/support agent, with evidence attached for read-only review.

## What to do

1. Read root `AGENTS.md` and the specified feature's `HANDOFF_REQUEST.md`, `STATE.md`, and (plan phase) `PLAN.md` / `TASKS.md`, or (code phase) the diff against the checkpoint SHA. All process files live in `.agents/work/<slug>/`.
2. Plan phase: review correctness, scope, risks, and test plan. Set `tests: SKIPPED`, `build_or_lint: SKIPPED`.
3. Code phase: inspect the implementer/support agent's test and build/lint evidence. Check runner, exact commands, cwd, revision/checkpoint and diff identity, timestamps, exit codes, log paths, and summaries. Read the logs and compare commands/coverage with the project and changes. Require passing evidence for the reviewed tree; do not run write-producing tests in the read-only sandbox.
4. A known failing check is `FAIL`. Use `BLOCKED` **only** for missing or suspicious evidence (stale, inconsistent, truncated, unverifiable); say what is needed. Read-only build restrictions alone never block approval when valid passing evidence exists. If a rerun is necessary, ask the implementer/support agent for a documented disposable-copy rerun as described in `AGENTS.md`.
5. Grade issues `blocker`, `major`, or `minor`, including failed/blocked required checks as blocker/major findings. Return the exact `HANDOFF_VERDICT.md` template as output; the caller persists it. No blocker/major and passing code-phase checks means `APPROVE`; failed/blocked checks or blocker/major means `CHANGES`.
6. Never request a 4th round. At `3/3`, state unresolved findings clearly so the implementer/support agent can write `ESCALATION.md` and notify the dispatch coordinator (worker) or bobo via the worktree unread marker (non-worker).
