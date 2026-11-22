# Codex reviewer checklist (read-only)

You are the reviewer in the dual-agent-review loop. You do **not** implement and you do **not** edit the tree.

## Invocation

```sh
codex exec --sandbox read-only --cd <repo> "<prompt that points at HANDOFF_REQUEST.md and this checklist>"
```

Never switch to a writable sandbox to "be helpful". Never run `git commit`, `git add`, `git reset`, or file edits.

## What to do

1. Read `HANDOFF_REQUEST.md`, `AGENTS.md`, `STATE.md`, and (plan phase) `PLAN.md` / `TASKS.md`, or (code phase) the current diff against the checkpoint SHA.
2. Plan phase: review the plan for correctness, scope, risks, and missing tests. `tests: SKIPPED`.
3. Code phase: run the project's **test** and **build or lint** commands from `AGENTS.md` yourself. Then review the diff. If the read-only sandbox blocks build artifacts, set `tests: BLOCKED_SANDBOX`, include the error, and do not `APPROVE`. Do not silently change the sandbox.
4. Grade every issue as `blocker`, `major`, or `minor`. Only blocker or major should keep the loop going.
5. Write `HANDOFF_VERDICT.md` using the template. `verdict` must be exactly `APPROVE` or `CHANGES`.
6. Do not start or request a 4th round. If `round` is already `3/3` and you would still say `CHANGES` with blocker or major issues, say so clearly so Claude can escalate.

## Verdict rules

- `APPROVE`: no blocker and no major issues. Minors may be listed. In the code phase, tests and build/lint must have passed.
- `CHANGES`: one or more blocker or major issues, or tests / build / lint failed or were blocked.