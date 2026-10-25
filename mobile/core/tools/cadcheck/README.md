# CAD export validation

From the repository root, build the full core and run the independent checks:

```sh
bash mobile/tools/bootstrap-occt.sh
bash mobile/tools/verify.sh --cad
```

To use an existing full OCCT build:

```sh
bash mobile/core/tools/cadcheck/run.sh \
  --build-dir "mobile/build/core-shared" \
  --output-dir "mobile/build/cadcheck" --check
```

The defaults are resolved from the script location, so it can be called from any
working directory. Explicit relative paths use the calling directory. Paths may
contain spaces. `--timestamp` overrides the fixed default export timestamp
`2026-10-21T02:30:00Z`.

`run.sh` requires the CLI and `tests/stl2stp-fixture-generator` from a full build.
It generates all input models and writes each run into a new
`mobile/build/cadcheck/run.XXXXXXXX/` directory: `fx/` contains the inputs and
`out/` the STEP/IGES exports. Existing runs are kept. A failed conversion or
readback stops the run; `--check` also returns nonzero if either independent
checker fails. The STEP JSON report is saved in that run directory.

The checks cover planar STEP geometry and do not replace testing in the target
CAD application. FreeCAD and rendering are optional separate steps:

- `gen.py DIR`: creates the directory and writes additional STL fixtures
  (rotated cubes, an open cube, and two rotated bodies). Basic cube, sphere, and
  two-shell fixtures come from the compiled fixture generator.
- `step_check.py FILE.step ...`: pure Python B-rep checks for references, loop
  closure, vertex-on-line/plane, edge sense, face normals, closed-shell edge use,
  Euler characteristic, signed volume, timestamps, and `\X2\` decoding.
  Writes `step_check_report.json` in the current directory; errors or an empty
  input model return nonzero.
- `pcurve_check.py FILE.step ...`: checks that supported PCURVEs on planes agree
  with their 3D lines. Errors and inputs with no supported PCURVE return nonzero.
- `render.py out.png a.step b.step`: renders faces and normals; requires matplotlib.
- `fc_check.py`: FreeCAD validity and BOP checks, run with FreeCAD's own Python:
  `freecadcmd fc_check.py -- DIR REPORT.txt`. Read the resulting report; it is not
  part of the automated `--check` gate.

The generator and independent checkers need Python 3.8+ and its standard library.
