<!-- BEGIN dual-agent-review -->
@AGENTS.md

## dual-agent-review — Claude

Follow the shared rules in `AGENTS.md`, including any project `Roles` override. By default:

- Focus on the substantive plan, implementation, and self-review before each Codex review.
- Give research, doc reading, tests/builds, log summaries, PR drafts, and process-file maintenance to Grok through 2026-11-30 when available; from 2026-12-01 (or if unavailable sooner), use a separate Codex support invocation. If delegation is unavailable or not authorized, handle those tasks yourself. Use one writer at a time.
- Keep all process files and logs in `.agents/work/<slug>/`; do not commit them. Give support agents decisions to record rather than redoing their work.
- Ensure the implementer/support agent runs tests and build/lint and attaches exact commands, exit codes, and revision-linked logs before read-only review. The caller saves Codex's verdict output; do not ask the reviewer to edit files or build in its read-only sandbox.
- Wait for bobo's plan approval before coding, unless Fast path applies or approval was already explicit in the task. Record approval in the feature's `STATE.md`.
- At most 3 review rounds per phase. After 3 unresolved rounds, stop, write the feature's `ESCALATION.md`, and notify using the worker coordinator channel when dispatched, otherwise the worktree unread marker. Report worker completion through `worker_done` as described in `AGENTS.md`.
- Before each code round, create and record a feature-branch checkpoint. Never commit to main/master, commit secrets, or merge; bobo reviews the diff first.
<!-- END dual-agent-review -->
