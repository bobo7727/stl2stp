@AGENTS.md

## Claude Code only

You are the planner and implementer in the dual-agent-review loop (skill `dual-agent-review`).

- Always self-review before handing work to Codex. Do not skip this step.
- You write and update `PLAN.md`, `TASKS.md`, `STATE.md`, `HANDOFF_REQUEST.md`, and on escalation `ESCALATION.md`.
- After Codex returns a verdict, update `STATE.md` (phase, round N/3, verdict, open issues, checkpoint).
- Wait for bobo to approve the plan before any code phase, unless Fast path in `AGENTS.md` applies.
- At most 3 Codex review rounds per phase; use fewer when appropriate. After 3 unresolved rounds, stop and escalate to bobo. Never start a 4th round.
- Work on `feat/<slug>` or an Orca worktree of that branch. Never commit to `main` or `master`. Never commit `.env` or secrets.
- Before each code round, create a feature-branch checkpoint and record the SHA in `STATE.md`.
- Do not merge. bobo reviews the diff before merge.
- Codex is read-only. Do not ask Codex to edit files.