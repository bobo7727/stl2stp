# AGENTS.md — dual-agent-review

This file is the single source of workflow rules for Claude Code and Codex. Claude also loads it through `@AGENTS.md` in `CLAUDE.md`.

## Roles

- **Claude**: planner and implementer. Writes the plan or the code, **self-reviews first**, then hands the work to Codex. Updates `STATE.md` and the handoff files. Never skips self-review.
- **Codex**: strictly read-only reviewer. Does not edit source, config, or git state. In the **code** phase, Codex also **runs the project's tests and build/lint itself**. Writes a parseable verdict: `APPROVE` or `CHANGES`.
- **bobo (human)**: approves the plan before any code phase (except Fast path); reviews the diff before merge; decides when a phase is still unresolved after 3 rounds.

Only Claude writes the tree. Never let two agents write the same worktree.

## Two-phase loop

Do not start the code phase until bobo has explicitly approved the plan, unless Fast path applies.

Use fewer than 3 review rounds when appropriate. A first-round `APPROVE` is success. Never start a 4th review round in the same phase.

### Fast path (quota saver)

Skip the **plan-review** phase only when **all** of these hold:

- Expected change is about 20 lines or fewer.
- No design, architecture, public API, schema, protocol, security, or dependency impact.
- No new feature surface. Typo, comment, tiny bugfix, or config nit is OK.

Claude records `mode: fast-path` and a 1–3 line rationale in `STATE.md`, then goes to the code phase. If either agent later finds design impact, stop coding, open the plan phase from the current work, and tell bobo.

### Plan phase

1. Claude writes or updates `PLAN.md` and `TASKS.md`, self-reviews the plan, sets `STATE.md` to `phase: plan` and `round: N/3`, writes `HANDOFF_REQUEST.md`, then asks Codex to review.
2. Codex reviews the plan only. No code edits. Tests are not required in this phase (`tests: SKIPPED`).
3. If verdict is `APPROVE`: stop the plan loop, set `STATE.md` to `waiting-for-bobo`, and **wait for bobo to approve the plan**. Do not start coding yet.
4. If verdict is `CHANGES` and there is at least one **blocker** or **major** issue: Claude addresses those issues, self-reviews again, increments the round, and sends another Codex review. **Minor** issues do not start a new round; list them in `STATE.md` for bobo.
5. If still unresolved after 3 rounds (blocker or major disagreement remains): **stop**. Write `ESCALATION.md` and notify (see Escalation).

### Code phase

Only after bobo approves the plan, or on Fast path.

1. Before **each** code round (including round 1), create a rollback checkpoint on the feature branch (see Checkpoints) and record the SHA in `STATE.md`.
2. Claude implements, self-reviews, runs tests if able, updates `STATE.md`, and writes `HANDOFF_REQUEST.md`.
3. Codex reviews read-only and **runs the tests and build/lint commands itself**. Writes `HANDOFF_VERDICT.md` with severity-graded issues.
4. **Definition of done** for a code round — all of the following must hold:
   - tests pass
   - build or lint passes (use lint if the project defines one; otherwise the build is the check)
   - Codex verdict is `APPROVE`
   Minor issues may remain; they do not block done, but must be listed.
5. `CHANGES` with blocker or major → Claude fixes, self-reviews, next round (max 3). Minors do not trigger another round.
6. After 3 unresolved rounds: stop, write `ESCALATION.md`, notify bobo.

## Codex invocation

Always review in a read-only sandbox, for example:

```sh
codex exec --sandbox read-only --cd <repo> "<review prompt>"
```

Codex must not modify source, `.env`, secrets, or git history. If the read-only sandbox blocks writing build artifacts, do **not** silently switch to a writable sandbox. Record `tests: BLOCKED_SANDBOX` (and the error) and do not `APPROVE`. Claude may then run the same commands in a normal terminal, attach the log to the next `HANDOFF_REQUEST.md`, and Codex re-checks that log still under `--sandbox read-only`.

## Handoff format

Use the repo-root templates `HANDOFF_REQUEST.md` and `HANDOFF_VERDICT.md`. Do not invent a different heading structure. The verdict value must be exactly `APPROVE` or `CHANGES`.

Issue grades: `blocker`, `major`, `minor`. Only `blocker` or `major` starts another review round.

## STATE.md

Claude updates `STATE.md` after every self-review and after every Codex verdict. Each log entry must include: phase, round N/3, verdict, open issues, checkpoint SHA (code phase), and mode (`full` or `fast-path`). Record `plan_approved_by: bobo` and the date when the plan is approved.

## Checkpoints (code phase)

- Work only on `feat/<slug>` or an Orca worktree of that branch. Never commit to `main` or `master`.
- Before each code round: if there are changes, make a **local WIP commit on the feature branch only**, staging only intended paths. Never stage `.env`, credentials, keys, or secrets. Message: `checkpoint: code round N before implement`.
- If the tree is already clean, record the current HEAD SHA as the checkpoint.
- Rollback (feature branch only, after bobo agrees or when restarting a round): `git reset --hard <checkpoint-sha>`. Never reset `main` or `master`.
- When the Orca CLI is available inside an Orca terminal, also store the SHA in the worktree comment.

## Guards

- Never commit secrets or `.env` files.
- No direct commits to `main` or `master`.
- Work only on feature branches or Orca worktrees.
- Branch name: `feat/<slug>` (lowercase, hyphenated).
- A human (bobo) reviews the diff before merge. Agents must not merge or push to `main`/`master`.
- Do not run bare `orca` outside Orca terminals (on Linux it may start the GNOME screen reader). Inside Orca use `orca`. Outside Orca, use `orca-ide` only for help or skills; never run `orca-ide open` unless bobo asks.

## Escalation

If a phase is unresolved after 3 rounds, write `ESCALATION.md` from the template (disagreement, options, Claude's recommendation, Codex's position). Then, only if running inside an Orca-managed terminal and the CLI is available, flag bobo:

```sh
# Resolve ORCA as in the orca-cli skill (ORCA_CLI_COMMAND, else orca inside Orca terminals).
ORCA worktree set --worktree active --workspace-status in-review --unread \
  --comment "ESCALATION: <phase> after 3 rounds; see ESCALATION.md" --json
```

The Orca CLI has no separate "send notification" command. `--unread` plus a worktree comment puts the unread marker on the workspace (the same cue as an agent finishing). If the CLI cannot reach a running Orca runtime, leave `ESCALATION.md` and tell bobo in the terminal.

## Project test and build commands

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

Until cmake and Flutter are installed here, the code-phase test/build gate for Codex is the Python checks above; the C++ and Flutter checks must be run by CI (`.github/workflows/core-regression.yml`) or on a machine with the tools. Record `tests: BLOCKED_ENV` (not `PASS`) when they cannot run.

## Safety before merge

Do not merge. Leave the feature branch or open a PR for bobo. The human reviews the diff, then merges.