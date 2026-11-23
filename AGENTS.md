<!-- BEGIN dual-agent-review -->
## dual-agent-review

Shared workflow for Claude, Grok, and Codex. Existing project instructions remain in force. A project-specific `Roles` section outside this marked block overrides the default role assignments below; substitute those assignments when the loop names a default role and record the effective roles in the feature's `STATE.md`. A role override does not relax the review sandbox or merge guards.

### Default roles

- **Claude**: core planner and implementer. Writes the substantive plan and source changes, self-reviews before every Codex review, and addresses blocker/major findings.
- **Grok** (local `grok` CLI, available through **2026-11-30**): support work — research, reading docs, running tests/builds, summarizing logs, drafting PR descriptions, and maintaining `TASKS.md`, `STATE.md`, and handoff/escalation files from Claude's decisions. Give it focused inputs and request a short summary to save Claude tokens. It does not make design decisions or edit implementation source by default.
- **Codex reviewer**: strictly read-only (`codex exec --sandbox read-only`). Reviews code and test evidence, returns a verdict as output, and does not edit the tree or git state.
- **Support fallback**: if Grok is unavailable, including from **2026-12-01**, Codex takes over support work in a **separate support invocation**, with only the writes needed for tests and process files. This is not the review invocation: the Codex reviewer stays read-only and checks the implementation and evidence independently. Claude retains planning, implementation, and self-review.
- **bobo (human)**: approves the plan before coding (except Fast path), reviews the diff before merge, and decides after 3 unresolved rounds.

Use one writer at a time per worktree. Finish support writes/test execution before review; do not let Claude and a support agent write concurrently. Do not launch helper agents unless the task authorizes them; the implementer handles support work when delegation is unavailable.

### Feature files

Choose a unique lowercase hyphenated feature slug and use **`.agents/work/<slug>/`** for `PLAN.md`, `TASKS.md`, `STATE.md`, `HANDOFF_REQUEST.md`, `HANDOFF_VERDICT.md`, `ESCALATION.md`, and test logs. All short process filenames below refer to that directory, never the repository root. Include the full feature path in each review prompt. `AGENTS.md`, `CLAUDE.md`, and `CODEX_REVIEW.md` stay at the root.

`orca-kit-init <repo> <slug>` appends `/.agents/work/` to the root `.gitignore` in a marked block. Ignoring the whole directory is simpler and safer than remembering to delete process files before every merge. Never force-add it. Before merge, check `git ls-files -- .agents/work`; if anything is already tracked, remove it from the index on the feature branch (keep local copies). Gitignore does not untrack existing files.

Fill in slug/branch fields in newly copied templates. For old installations, move root process files into the chosen feature directory and untrack them on the feature branch; initialization does not migrate or delete old files.

### Two-phase loop

Use at most **3 review rounds per phase**, fewer whenever possible. Never start a 4th round. Only `blocker` or `major` findings require another round; failed or blocked required checks must be recorded at one of these grades. List `minor` findings without a new round.

**Fast path**: about 20 lines or fewer with no design, architecture, public API, schema, protocol, security, dependency, or new-feature impact may skip plan review. Record `mode: fast-path` and a short rationale in `STATE.md`. If design impact appears, stop coding, open the plan phase, and notify bobo through the channel below.

**Plan phase**:

1. Claude drafts `PLAN.md`, self-reviews, and supplies decisions to the support agent, which updates `TASKS.md`, `STATE.md` (`phase: plan`, `round: N/3`), and `HANDOFF_REQUEST.md`.
2. Codex reviews the plan only (`tests: SKIPPED`, `build_or_lint: SKIPPED`). The writable caller saves Codex's output as `HANDOFF_VERDICT.md`; the reviewer does not write this file.
3. `APPROVE`: set state to `waiting-for-bobo`, request plan approval, and wait. Record `plan_approved_by: bobo` and the approval date before coding. Explicit approval already given in the task counts; do not ask again.
4. `CHANGES`: Claude addresses blocker/major findings, self-reviews, and requests the next round. After round 3 remains unresolved, stop and escalate below.

**Code phase** (after plan approval, or Fast path):

1. Before each round, create a rollback checkpoint on the feature branch (local WIP commit of intended paths, or record HEAD if clean). Record its SHA in `STATE.md`.
2. Claude implements and self-reviews. The implementer or support agent actually runs the project test and build/lint commands in a writable environment, then records evidence in `HANDOFF_REQUEST.md` and logs under this feature directory.
3. Codex reviews the diff against the checkpoint, the relevant code, and the recorded evidence read-only. The caller persists its output to `HANDOFF_VERDICT.md` and updates `STATE.md`.
4. Done requires passing tests, passing build or lint (lint if defined, otherwise build), and `APPROVE`. Actual failures are `FAIL` and require fixes. Missing/suspicious evidence is `BLOCKED` and requires evidence, not a sandbox switch. Blocker/major findings lead to another round, up to 3; then escalate.

### Test evidence and review sandbox

For **each** test and build/lint check, the handoff records runner, exact command, working directory, code revision/checkpoint and diff identity, timestamp, exit code, log path, and a short log summary. If the code changes after testing, rerun affected checks and update evidence. Preserve failure output and report nonzero exit codes; do not hide them behind a successful `tee` exit status (use `pipefail` or capture the command's status).

Codex checks that commands match the project, exit codes and summaries match logs, logs belong to the reviewed revision/diff, and coverage is adequate. It does **not** need to compile or run write-producing commands in the read-only review sandbox. Do not block merely because that sandbox cannot build. Mark a check `BLOCKED` only when its evidence is missing or suspicious (for example stale, inconsistent, truncated, or unverifiable); explain exactly what evidence is needed. Valid evidence of a failing check is `FAIL`, not `BLOCKED`.

If a rerun is needed, the implementer/support agent may run it in a disposable, writable copy of the **exact reviewed tree**, including intended uncommitted changes. Record how the copy was made, source revision and diff identity, copy path, environment/dependency setup, commands, exit codes, and log locations in the handoff. Keep the original reviewer sandbox read-only; have Codex read the rerun evidence. Do not silently switch the review invocation to a writable sandbox.

### Codex invocation and handoff

Use the fixed handoff headings and values from the templates. Verdict is exactly `APPROVE` or `CHANGES`; check status is `PASS`, `FAIL`, `SKIPPED` (plan only), or `BLOCKED`. `CHANGES` must explain failed checks, missing evidence, or blocker/major findings.

For example, from the repository root, the **writable caller's shell** saves reviewer output outside the sandbox:

```sh
slug=feature-slug # replace with the initialized feature slug
work_dir=".agents/work/$slug"
codex exec --sandbox read-only --cd "$PWD" \
  "Review $work_dir/HANDOFF_REQUEST.md using CODEX_REVIEW.md; return the $work_dir/HANDOFF_VERDICT.md template as output, without editing files." \
  > "$work_dir/HANDOFF_VERDICT.pending.md"
```

Check the Codex process exit code and verdict format before replacing `HANDOFF_VERDICT.md` with the pending output; capture to a separate path so the template/prior verdict remains readable during review. A failed invocation or malformed output is not approval. Pass the verdict back to the implementer. Update `STATE.md` after each self-review/verdict with phase, round N/3, verdict, open issues, checkpoint, and mode.

### Escalation, approvals, and completion

After 3 unresolved rounds, stop and write `ESCALATION.md` (disagreement, options, Claude's recommendation, Codex's position). Writing a file alone is insufficient: notify the coordinator/human and record the receipt in that file.

Resolve the Orca executable through the `orca-cli` skill, and load the version-matched `orchestration` guide before worker messaging. In the examples `ORCA` means that executable, not a literal command. Use the sender handle and Task/Dispatch IDs from the current dispatch preamble; never invent them.

- **Dispatched worker** (Task/Dispatch IDs in the task): use the dispatch's coordinator channel, not just a workspace unread marker. For plan approval or a decision, run `ORCA orchestration ask --from <sender> --question "Need bobo: <decision>; see .agents/work/<slug>/ESCALATION.md"` (point to `PLAN.md` for plan approval), then end the turn and wait for Orca to wake you. If blocked pending coordinator action, send `ORCA orchestration send --from <sender> --type escalation --subject "Blocked: <reason>" --body "Need bobo: <decision>; see .agents/work/<slug>/ESCALATION.md" --task-id <task-id> --dispatch-id <dispatch-id>` and stop. This surfaces the need for human action on the tracked task. Do not mark unresolved work successful.
- **Worker completion**: use `ORCA orchestration send --from <sender> --type worker_done --subject "<result>" --body "<three sentences: work done, findings, work left>" --task-id <task-id> --dispatch-id <dispatch-id> --outcome succeeded` (or `failed` if unfinished). Add only real `--files-modified`/`--report-path` values. Send exactly once; follow any printed recovery instructions on failure. On confirmation, end the turn without polling or taking other work under those IDs.
- **Not a worker**: use `ORCA worktree set --worktree active --workspace-status in-review --unread --comment "Need bobo: <decision>; see .agents/work/<slug>/ESCALATION.md" --json` (or `PLAN.md` for approval; on completion, comment with the result/diff). If Orca is unavailable, keep the file and tell bobo directly, recording the notification failure. Never substitute this fallback for worker coordinator messaging.

### Guards and checkpoints

- Work on a feature branch (normally `feat/<slug>`) or its Orca worktree. Never commit to, merge into, or push to `main`/`master`.
- Stage only intended paths. Never commit `.env`, secrets, or `.agents/work/`.
- Before each code round: local WIP commit if dirty, otherwise HEAD SHA. Record the SHA; when available also store it in an Orca worktree comment.
- Roll back only on the feature branch with bobo's agreement or explicit authorization to restart a round. Never reset main/master.
- Human reviews the diff before merge. Leave the branch or PR for bobo.
- Use the session's resolved Orca executable. Bare `orca` outside Orca terminals on Linux may be the GNOME screen reader.

### Project test and build commands

Commands for this repo (stl2stp). Paths are relative to the repo root. The Windows MFC project (`stl2stp-code/`) has no tests and needs VS2019, so the maintained build/test line is the cross-platform core in `mobile/` plus the Flutter app. Sources: `mobile/QUICKSTART.md`, `.github/workflows/core-regression.yml`, `codemagic.yaml`.

- Build (core, needs cmake, ninja, g++ with C++17, python3, curl): `cd mobile && bash tools/bootstrap-occt.sh --jobs 4 && bash tools/verify.sh --jobs 4 --cad` (CI equivalent: cmake -S core -B build/core-linux -G Ninja -DSTL2STP_WITH_OCCT=ON -DOCCT_ROOT=... then cmake --build). Mesh-only, no OCCT, no STEP/IGES, not a full verification: `cd mobile && bash tools/verify.sh --mesh-only --jobs 4`.
- Test (core): `cd mobile && ctest --test-dir build/core-linux --output-on-failure` (verify.sh runs ctest itself).
- Test (Python tool checks, runnable without cmake/OCCT): from `mobile/app`, run `python3 tool/ios/core_symbols.py`, `python3 tool/ios/test_core_symbols.py`, `python3 tool/ios/test_build_ios_core.py`, `python3 tool/ios/test_check_ios_binary.py`, `python3 ../tools/tests/test_occt_discovery.py`, `python3 ../tools/tests/test_repro_tools.py`.
- Test + Lint (Flutter, needs Flutter 3.38.7): `cd mobile/app && flutter pub get && flutter analyze && flutter test`. Lint is `flutter analyze`.
- Not for Codex or Linux: iOS builds (`build_ios_core.sh`, `pod install`, `flutter build ios/ipa`, `xcodebuild`) and the Codemagic workflows need macOS + Xcode. Windows MFC build needs VS2019.

Verified on the local WSL Ubuntu 20.04 machine (2026-11-22; g++ 9.4, Python 3.8.10, ninja 1.13; no cmake, no flutter):

| Command | Result |
|---|---|
| `python3 tool/ios/core_symbols.py` | PASS (12 public functions retained) |
| `python3 tool/ios/test_core_symbols.py` | PASS (5 tests) |
| `python3 tool/ios/test_build_ios_core.py` | PASS (1 test) |
| `python3 tool/ios/test_check_ios_binary.py` | PASS (3 tests) |
| `python3 ../tools/tests/test_occt_discovery.py` | PASS (2 tests skipped: no OCCT installed) |
| `python3 ../tools/tests/test_repro_tools.py` | PASS (9 tests, 2 skipped) |
| `bash tools/verify.sh --mesh-only` | Cannot run locally: `cmake` and `ctest` not installed |
| `bootstrap-occt.sh` / `verify.sh --cad` / `ctest` | Cannot run locally: no cmake, and OCCT is not built (large multi-GB build; CI builds it) |
| `flutter pub get/analyze/test` | Cannot run locally: Flutter SDK not installed |
| iOS / Codemagic steps | Cannot run locally: need macOS and Xcode |

Status note: the table above is the earlier measurement. A full product build/verification (WSL, original PR worker, dispatch ctx_49fab07879b5) was still in progress when this layout migration was written; do not treat any C++/Flutter/OCCT check as passing until that worker reports real results.

Until cmake and Flutter are installed here, the checks the implementer/support agent can run here are the Python checks above; the C++ and Flutter checks must be run by CI (`.github/workflows/core-regression.yml`) or on a machine with the tools, and their logs attached to the handoff. Until that evidence exists, do not record them as `PASS`; mark them `BLOCKED` (missing evidence) with the reason, never as passing.
<!-- END dual-agent-review -->
