# Conversion regression fixtures

`gen_fixtures.py` is the deterministic generator. It extends the original ten shapes (20 mm cube binary and ASCII, cylinder d20×h40, torus, sphere r25, two boxes, open cube, 0.1 mm cube, 200k-triangle sphere, non-STL text) with inverted normals, duplicate vertices, zero-area triangles, a 1e5 offset, a metre-number cube, a thin plate, a through-hole, an empty binary STL, a truncated binary STL, and CRLF ASCII.

Committed files live in this directory, plus `manifest.json`. The 200700-triangle sphere is not committed. CTest `stl2stp.regression_fixtures` runs:

```sh
python3 gen_fixtures.py --check --large-dir build/core-linux/tests/regression-large
```

`--check` fails if a committed STL or the geometry fields in the manifest drift from the script, and it writes the large sphere into `--large-dir`. `stl2stp.regression` then calls the public C API (`stl2stp_load_stl`, `stl2stp_export_step`, `stl2stp_export_iges`) and re-reads both outputs with `STEPControl_Reader` and `IGESControl_Reader`.

`manifest.json` holds one object per file:

| Field | Meaning |
| --- | --- |
| `load_status` | Expected `stl2stp_status` from `stl2stp_load_stl` |
| `triangles` / `vertices` | Mesh stats after the loader's exact weld and zero-area drop |
| `bbox_min` / `bbox_max` | Loader AABB, and the re-read shape AABB |
| `volume` | Expected closed-solid volume (mm³). Null when the mesh is not a solid |
| `volume_abs_tol` / `volume_rel_tol` | Passes when `abs(actual-expected) <= max(abs, rel*|expected|)` |
| `closed` | Output must be closed solid(s) with that volume |
| `step` / `iges` | `status`, `solids`, `faces`, `shell_closed` after an OCCT re-read. IGESControl_Reader does not rebuild shells when `write.brep.mode` is the OCCT default 0, so IGES expectations are loose faces (`solids=0`, `shell_closed=false`) with the same face count and, for closed inputs, the same GProp volume. STEP is the solid/shell check |
| `assert_mesh` | When false, only the status codes are checked (used for the leading-whitespace bug) |
| `known_bug` | Current behaviour that is wrong. Set for `21_ascii_leading_whitespace.stl` only |
| `generate` | True for the sphere written at test time |
| `time_budget_s` | Per-export and per-read ceiling for the large file |

STL has no unit field. STEP and IGES are written in millimetres, so `15_meter_scale_0.02.stl` stays 0.02 mm on a side. The open cube drops triangles 2 and 3 (the +X face); the historical filename says "missing top".

`21_ascii_leading_whitespace.stl` is a known loader bug: `binary_format()` accepts an ASCII file only when the first five bytes are `solid` (or the length is an exact binary STL). Leading whitespace is parsed as binary records, load returns `STL2STP_WARNING_PARTIAL`, and export returns `STL2STP_ERROR_OCCT`. `20_ascii_crlf_whitespace.stl` is the same cube with CRLF, tabs, and blank lines, but it starts with `solid` and converts.

Regenerate after editing the script:

```sh
python3 mobile/core/tests/fixtures/gen_fixtures.py --large-dir /tmp/stl2stp-large
```

That also refreshes the four copies under `mobile/app/assets/regression/` used by `integration_test/fixture_regression_test.dart`. The iOS run of that test is the manual Codemagic workflow `ios-fixture-regression`. Linux ctest on every pull request is `.github/workflows/core-regression.yml`.
