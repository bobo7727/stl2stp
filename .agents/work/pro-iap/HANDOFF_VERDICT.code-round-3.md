# Handoff Verdict

- feature_dir: .agents/work/pro-iap
- phase: code
- round: 3/3 (final)
- mode: full
- verdict: CHANGES
- reviewed_revision: a9f8ea3d8f3256cfd546e3dc26be1b9844148e70..ff93c325a59887bf21db505eb3c265d3936aefb3
- reviewed_diff_identity: git diff a9f8ea3 ff93c32; SHA-256 4dca6b9a9372a81708578a629f94a0c84f7478d7b1d68d94450103f55fa2efac
- tests: BLOCKED_SANDBOX
- build_or_lint: BLOCKED_SANDBOX

## Evidence checked

- test_log_and_result:
  - Independently attempted `flutter test` from `mobile/app`: exit 1 before tests started; SDK cannot write `bin/cache/engine.stamp` (“Read-only file system”).
  - Independently ran all six Python checks with `python3 -B`: `core_symbols.py` PASS (12 functions), `test_core_symbols.py` PASS (5 tests), `test_check_ios_binary.py` PASS (3 tests). The remaining three suites encountered sandbox temporary-directory errors: “No usable temporary directory found.”
  - Supplied `logs/c3_test.log` reports 133 passing Flutter tests and `exit 0`; this is supplied evidence, not an independently reproduced pass.
- build_or_lint_log_and_result:
  - Independently attempted `flutter analyze`: exit 1 with the same SDK-cache restriction.
  - Supplied `logs/c3_analyze.log` reports “No issues found!”
  - `bash tools/verify.sh --jobs 4 --cad`: exit 1 because OCCT headers are absent (`BLOCKED_ENV`). No bootstrap/network operation attempted.
  - Swift/XCTest: `BLOCKED_ENV`; C3 remains deferred by agreement.
  - Application-only `git diff --check`: PASS. Full-range whitespace findings are confined to recorded review logs.
- revision_diff_and_coverage_match: Read AGENTS.md, CODEX_REVIEW.md, current handoff, STATE.md’s final section, both earlier code verdicts, verdict template, relevant PLAN sections, and changed application/test sources. HEAD is d20b0bb; its differences after ff93c32 contain only process state/logs, with no application/core changes. The working handoff was already modified. Translation validation passed for 165 matching keys and placeholders.
- missing_or_suspicious_evidence: The handoff attributes the Flutter logs to ff93c32, but the logs themselves contain no revision identifier. Native StoreKit integration coverage and compilation evidence remain outstanding under the agreed C3 deferral. Local AGENTS.md/CODEX_REVIEW.md explicitly prohibit approval when required reviewer checks are sandbox-blocked.

## Issues

### blocker

none

### major

- id: PRO-C1
  file: mobile/app/lib/core/ffi_engine.dart:355; mobile/app/lib/core/memory_guard.dart:43; mobile/app/test/batch_test.dart:264
  summary: Partially resolved. The actual-face check closes the normal TARGET_NOT_REACHED paid-limit bypass, but the requested actual-count memory check remains absent. STATE.md’s assertion that the 50,000-face cap makes the existing precheck sufficient is incorrect: the budget varies by device/settings, and the precheck still estimates CAD memory using the requested target. For example, 40,000 topology-constrained faces at 5% reduction produce a precheck estimate of 98 MB; if simplification retains 40,000 faces, the existing model estimates 870 MB for CAD construction. A 500 MB budget therefore passes preflight, passes the new free-face check, and reaches export despite exceeding the memory refusal threshold. Pro conversions have the same issue without a face cap. These figures are source-derived, not native runtime measurements. The new DemoEngine test always achieves its requested target and does not exercise TARGET_NOT_REACHED or the FfiEngine export boundary.
  suggested_fix: Enforce the memory budget against the actual post-simplification count before CAD construction, without applying the simplification percentage again. Apply this to free and Pro conversions. Add a regression where simplification retains more faces than requested, verifying export is prevented both above the free face limit and below that limit when actual memory exceeds the budget.

### minor

- id: BATCH-002
  file: mobile/app/lib/screens/batch_screen.dart:85; mobile/app/lib/screens/flow.dart:66
  summary: The authorized content-deduplication policy is acceptable, but its implementation does not span repeated additions. materializePicked creates a fresh seen set on every picker call and gives non-local files new temporary paths. BatchScreen then compares only those paths. Selecting the same non-local file again through Add more adds another queue entry, causing duplicate conversions/outputs and consuming the 50-file allowance. The added test covers identical content only within one materializePicked call.
  suggested_fix: Preserve content identity with each imported entry and deduplicate against the entire setup selection across picker calls, retaining the authorized name/size/content policy. Add a repeated-Add-more regression for identical non-local content and another for distinct content with the same filename.

## Notes

Earlier first-round findings:

| Finding | Final assessment |
| --- | --- |
| C1 | **Partially resolved; major remains.** Actual-face entitlement enforcement is present; actual-memory enforcement and the requested native-boundary regression are missing. |
| C2 | **Resolved in code.** Conversion awaits worker exit without the former timeout before returning. The added test checks the waiting helper; it does not simulate native cleanup exceeding 15 seconds. |
| C3 | **Deferred by agreement; unresolved verification.** Acceptable as a tracked Codemagic follow-up for this review. Running existing RunnerTests alone will not supply the missing StoreKit integration scenarios; retain that coverage requirement and inspect test results despite `ignore_failure`. C3 does not independently trigger another review round. |
| C4 | **Resolved.** ResumeRefresh invokes purchase.refresh on resume; IosStoreService invokes native refresh and applies the snapshot without AppStore.sync. Lifecycle and cache tests were added. |

Earlier batch findings:

| Finding | Final assessment |
| --- | --- |
| BATCH-001 | **Resolved.** Output-path creation and token acquisition are guarded; acquired tokens are disposed in finally. Queue finalization assigns terminal states, and the screen handles an escaped runner error. The new output-path exception test verifies continuation and terminal totals. |
| BATCH-002 | **URI requirement superseded by the authorized content policy.** Policy accepted; cross-addition deduplication remains incomplete as the minor finding above. |
| BATCH-003 | **Resolved.** Add more supports repeated fallback path entry; the widget regression adds a second file. |
| BATCH-004 | **Resolved.** Widget tests now open populated setup, revoke Pro, tap Start, verify the paywall/no conversion, preserve selection on cancellation, and exercise purchase followed by conversion. |

Final result: **CHANGES — 0 blocker, 1 major, 1 minor**, with independent test/lint validation still `BLOCKED_SANDBOX`.

This is round **3/3**. Do not open a fourth round. The writable caller should save this verdict and escalate the remaining major issue and validation gate to bobo. No files or git state were modified; no network access was used.
