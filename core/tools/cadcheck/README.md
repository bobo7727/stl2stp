# cadcheck: external validation of stl2stp exports (no SolidWorks needed)

- `gen.py <dir>`: writes extra STL fixtures (rotated cube n=4/10, open cube, two rotated bodies).
- `run.sh`: builds the representative export set into build/cadcheck2/out with build/cc2-release/stl2stp-cli.
- `step_check.py *.step`: independent pure-Python (non-OCCT) B-rep checker for planar STEP:
  references, loop closure, vertex-on-line/plane, edge sense, loop-vs-face-normal, closed-shell edge use
  (orientability), Euler, signed volume, header timestamp and \X2\ decoding. Writes step_check_report.json.
- `pcurve_check.py *.step`: checks every PCURVE on a PLANE agrees with its 3D LINE.
- `render.py out.png a.step b.step`: matplotlib picture of faces with face normals.
- `fc_check.py`: FreeCADCmd script (isValid + BOP check). `freecadcmd fc_check.py -- <dir> <report.txt>`
