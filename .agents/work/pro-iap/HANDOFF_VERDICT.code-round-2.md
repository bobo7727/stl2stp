# Handoff Verdict

- feature_dir: .agents/work/pro-iap
- phase: code
- round: 2/3 (batch)
- mode: full
- verdict: CHANGES
- reviewed_revision: b0abdd53af3f77298efe5b1d7e1fb635a8dee8b2..a9f8ea3d8f3256cfd546e3dc26be1b9844148e70
- reviewed_diff_identity: git diff b0abdd5 a9f8ea3; SHA-256 5cc5ce2de9957f1d520d66d3865bbce01631a6d388090618b4479d67852a47da
- tests: BLOCKED_SANDBOX
- build_or_lint: BLOCKED_SANDBOX

## Evidence checked

- test_log_and_result: Independently ran the six Python commands from `mobile/app`, with `PYTHONDONTWRITEBYTECODE=1`. `core_symbols.py` passed (12 functions); `test_core_symbols.py` passed (5 tests); `test_check_ios_binary.py` passed (3 tests). The other three suites were blocked by unavailable writable temporary directories. `flutter test` exited 1 before executing tests because the SDK could not write `bin/cache/engine.stamp` (“Read-only file system”). Supplied `logs/c2_test.log` reports 121 passing Flutter tests and exit 0.
- build_or_lint_log_and_result: `flutter analyze` independently attempted; same SDK write restriction, exit 1. Supplied `logs/c2_analyze.log` reports no issues. `bash tools/verify.sh --cad --jobs 4` exited 1 because OCCT headers are absent (`BLOCKED_ENV`).
- revision_diff_and_coverage_match: HEAD matches a9f8ea3 and the working tree is clean. Reviewed only the requested batch diff, with surrounding dependencies read for context. Read AGENTS.md, CODEX_REVIEW.md, handoff, STATE.md, PLAN §8/§14c, and verdict template. Independently checked all 37 batch translation keys for matching languages/placeholders and absence of Chinese in English strings: PASS.
- missing_or_suspicious_evidence: Flutter results are supplied evidence, not independently reproduced passes. Runtime findings below follow from code inspection. `git diff --check` reports trailing whitespace only in the committed analyzer log.

## Issues

### blocker

none

### major

- id: BATCH-001
  file: mobile/app/lib/batch/batch_runner.dart:207
  summary: Output-path creation occurs before the conversion’s try/finally. `AppState.outputPathFor` performs synchronous filesystem operations, including creating `Outputs`; a filesystem exception escapes `_runOne` without disposing its token or assigning a terminal state. `run()` then exits before setting `_finished`, and `BatchRunningScreen._go()` has no recovery/finally. The user remains on a non-dismissible running screen; Stop cannot finish the abandoned queue, and the background task is not ended through the normal completion path. This violates the promised IO-failure continuation and terminal-state guarantees.
  suggested_fix: Include output-path creation and token acquisition in guarded per-file execution, release any acquired token in finally, and convert setup failures into a failed item before continuing. Ensure queue/screen cleanup also runs on unexpected exceptions. Add a regression where `outputPathFor` throws for one input, verifying subsequent processing, terminal totals, token disposal, and an accessible completion screen.

### minor

- id: BATCH-002
  file: mobile/app/lib/screens/flow.dart:75
  summary: Non-local inputs are deduplicated by name, length, and content hash rather than source identity. Two distinct URIs containing identical bytes under the same filename silently become one selected file, contrary to §14c P13. `PlatformFile.uri` already exposes the source URI. The fake picker tests also assign identical URIs to cases described as different sources, so they do not validate source identity.
  suggested_fix: Deduplicate using the source URI, preserve separate temporary directories, and give test inputs explicit URIs. Cover different URIs with identical name/content and repeated selections of the same URI.

- id: BATCH-003
  file: mobile/app/lib/screens/flow.dart:99
  summary: The unavailable-picker fallback accepts one path and immediately opens batch setup. There is no way to append another file, despite §8.1 explicitly promising repeated additions. The added `batch.add_more` and `batch.picker_note` strings are unused.
  suggested_fix: Provide a repeated path-entry flow or an Add more action that preserves the existing selection and applies deduplication and the 50-file cap.

- id: BATCH-004
  file: mobile/app/test/batch_test.dart:259
  summary: The test named “entitlement lost after the setup page opened is seen at start” never opens setup or taps Start; it only checks `batchLimit()` before and after changing preferences. It would still pass if the final entitlement check were removed from `_start`.
  suggested_fix: Add a widget test that opens populated setup while Pro is active, revokes entitlement, taps Start, and verifies the paywall appears, no conversion begins, and selection/settings survive cancellation. Exercise successful purchase or restore followed by rechecking as well.

## Notes

The batch startup and between-file entitlement checks are present. Sequential execution, isolated temporary directories, and cancellation terminal-state handling substantially implement §8 and §14c.

Verdict: CHANGES — 1 major and 3 minor findings. Independent Flutter validation remains BLOCKED_SANDBOX. No files or git state were modified; the earlier range’s verdict is outside this review.
