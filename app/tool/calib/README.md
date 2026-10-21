# Estimate calibration

`calib.py` / `calib2.py` generate test meshes (UV spheres = curved, tessellated
boxes = planar), run `stl2stp-cli convert` and record wall time and output size.
Results from the dev PC (WSL2 Ubuntu 20.04, gcc-10, OCCT 8.0.1, output on /mnt/d),
2026-10-21: `calib.csv`, `calib2.csv`. Coefficients fitted from them live in
`lib/core/estimate.dart` (`Coefficients.calibrated`); `test/core_test.dart`
checks every measurement falls inside the predicted range.

    python3 calib.py  <path/to/stl2stp-cli> <out-dir>   # ~30 min, one conversion at a time
    python3 calib2.py <path/to/stl2stp-cli> <out-dir>   # simplification, ~2 min

Notes: `convert` already includes the STEP read-back verification
(`readback_ms` in its log); the `readback` column in calib.csv is an extra
separate `stl2stp-cli readback` run. Meshes and outputs are deleted after each run.
To calibrate a phone, time a few conversions in the app and set
`Estimator(deviceFactor: phone/PC)`.
