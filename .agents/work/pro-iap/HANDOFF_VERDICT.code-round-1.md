# Handoff Verdict

- feature_dir: .agents/work/pro-iap
- phase: code
- round: 1/3
- mode: full
- verdict: CHANGES
- reviewed_revision: 9e7ef85ecb4c44561f21ccbd4183df3ca5e2a028..b0abdd53af3f77298efe5b1d7e1fb635a8dee8b2
- reviewed_diff_identity: git diff 9e7ef85 b0abdd5; SHA-256 e322a2b511f044cb123a9e8cd6a5b290f3f340acbb9341360f4e82328492d9e9
- tests: BLOCKED_SANDBOX
- build_or_lint: BLOCKED_SANDBOX

## Evidence checked

- test_log_and_result:
  - `python3 -B tool/ios/core_symbols.py`: PASS, 12 public functions.
  - `python3 -B tool/ios/test_core_symbols.py`: PASS, 5 tests.
  - `python3 -B tool/ios/test_check_ios_binary.py`: PASS, 3 tests.
  - `test_build_ios_core.py`, `test_occt_discovery.py`, and `test_repro_tools.py`: BLOCKED_SANDBOX. They exited with temporary-directory errors: `No usable temporary directory found`.
  - `flutter test`: BLOCKED_SANDBOX, exit 1 before tests started: `bin/cache/engine.stamp: Read-only file system`.
  - Target-revision translation checks: PASS, 125 matching keys and placeholders; English contains no Chinese except the Chinese-language selector.
- build_or_lint_log_and_result:
  - `flutter analyze`: BLOCKED_SANDBOX, same SDK-cache error.
  - `bash tools/verify.sh --mesh-only --jobs 4`: BLOCKED_SANDBOX; cannot create `mobile/build`.
  - `bash tools/verify.sh --jobs 4 --cad`: BLOCKED_ENV; OCCT headers absent.
  - Swift build/XCTest: BLOCKED_ENV; no Xcode.
  - `git diff --check 9e7ef85 b0abdd5`: PASS.
- revision_diff_and_coverage_match: Reviewed the requested 22-file diff and target-revision sources against AGENTS.md, CODEX_REVIEW.md, HANDOFF_REQUEST.md, PLAN.md, and STATE.md. Current HEAD is a9f8ea3 and includes subsequent batch changes; those are excluded from this verdict. The Python check sources are unchanged after b0abdd5.
- missing_or_suspicious_evidence: Supplied c1 logs report analyze PASS and 106 tests PASS, but identify an earlier uncommitted tree rather than the complete b0abdd5 revision. No build or XCTest evidence covers the new Swift implementation. The earlier Codemagic result covers the base revision.

## Issues

### blocker

none

### major

- id: PRO-C1
  file: mobile/app/lib/purchase/free_tier.dart:25; mobile/app/lib/screens/preview_screen.dart:78
  summary: The newly activated 50,000-face restriction checks the requested simplification target, not the actual simplified mesh. The core explicitly returns a positive TARGET_NOT_REACHED warning when topology prevents sufficient reduction (`mobile/core/src/mesh.cpp:438`). FfiEngine accepts that warning and proceeds to export without checking the actual face count. For example, 60,000 disconnected triangles with 50% reduction pass the free check as 30,000 faces, while the simplifier’s topology rules can retain all 60,000. This bypasses the paid large-file boundary and can also invalidate the memory estimate. This example is source-derived; native execution was unavailable.
  suggested_fix: Enforce the applicable face limit after simplification and before CAD export, using actual mesh statistics. Stop and return an actionable result if the mesh remains over the free limit. Recheck memory against the actual count as well. Add a regression covering TARGET_NOT_REACHED with an actual count above 50,000 and verify export is never called for a free user.

- id: PRO-C2
  file: mobile/app/lib/core/ffi_engine.dart:192
  summary: The 15-second timeout silently completes the cleanup wait even when the worker has not exited. Native destruction occurs after the worker sends its result, so slow cleanup allows convert() to return while the previous native handle and buffers still exist. The next conversion can then overlap that cleanup, violating PLAN §14c P12 and restoring the memory-overlap risk this commit intends to fix.
  suggested_fix: Complete the conversion Future only after confirmed native cleanup or worker exit. Do not treat a cleanup timeout as successful completion or permit another conversion while cleanup remains outstanding. Add a test where cleanup exceeds the timeout and verify the Future remains pending and no subsequent conversion starts.

- id: PRO-C3
  file: mobile/app/ios/RunnerTests/ProEntitlementTests.swift:8; codemagic.yaml:71
  summary: The planned native StoreKit verification is incomplete. The added tests exercise only a Boolean predicate; there is no `.storekit` configuration or StoreKitTest coverage for purchase, refund, pending approval, or restore through StoreCoordinator. These paths implement the actual entitlement delivery and revocation behavior. None of the new Swift code has build/XCTest evidence, and `ignore_failure: true` allows the workflow to report success even when native tests fail. This does not satisfy PLAN §11’s completion gate.
  suggested_fix: Add the planned StoreKit configuration and integration tests, require the native test step to pass, and attach Swift compilation plus XCTest results for the exact reviewed revision. Also provide revision-bound Flutter analyze/test evidence from a writable runner; keep this reviewer read-only.

### minor

- id: PRO-C4
  file: mobile/app/ios/Runner/StoreCoordinator.swift:88; mobile/app/ios/Runner/StoreChannel.swift:20
  summary: Foreground entitlement refresh is implemented and exposed, but nothing invokes it. Dart has no refresh call or purchase lifecycle observer, and applicationDidBecomeActive only reinstalls channels. The planned foreground reconciliation therefore depends entirely on transaction-update delivery.
  suggested_fix: Invoke the entitlement refresh when the app resumes and apply its snapshot through the existing cache-update path, without calling AppStore.sync(). Add a lifecycle regression test.

## Notes

No files or git state were modified. No network access was used.

Batch implementation and P10/P13/P14 acceptance remain outside this range. The three major findings and blocked verification prevent approval of this code round.
