#!/usr/bin/env python3
"""Second pass: simplification cost (QEM) on top of conversion. Reuses calib.py mesh generators."""
import os, sys, time, subprocess, csv
sys.argv = sys.argv[:3]
CLI, OUT = sys.argv[1], sys.argv[2]
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'calib.py')).read()
ns = {}
exec(src.split('meshes = [')[0].replace('CLI = sys.argv[1]', 'CLI = None').replace('OUT = sys.argv[2]', 'OUT = None').replace('os.makedirs(OUT, exist_ok=True)', ''), ns)
os.makedirs(OUT, exist_ok=True)
runs = [('sphere50k', ns['sphere'](200, 126), [50, 25], [False, True]),
        ('box20k', ns['tess_box'](41), [50], [False, True])]
rows = []
for name, tris, pcts, merges in runs:
    stl = os.path.join(OUT, name + '.stl'); ns['write_stl'](stl, tris)
    for pct in pcts:
        for merge in merges:
            out = os.path.join(OUT, f'{name}_s{pct}{"_m" if merge else ""}.step')
            cmd = [CLI, 'convert', stl, out, '--mode', 'original', '--simplify', str(len(tris) * pct // 100)] + (['--merge-coplanar'] if merge else [])
            t0 = time.perf_counter(); p = subprocess.run(cmd, capture_output=True, text=True); dt = time.perf_counter() - t0
            size = os.path.getsize(out) if os.path.exists(out) else -1
            rows.append(dict(mesh=name, tris=len(tris), pct=pct, merge=int(merge), rc=p.returncode, seconds=round(dt, 3), bytes=size,
                             log=((p.stdout or '') + (p.stderr or ''))[-400:].replace('\n', ' | ')))
            print(rows[-1], flush=True)
            if os.path.exists(out): os.remove(out)
    os.remove(stl)
with open(os.path.join(OUT, 'calib2.csv'), 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
print('CALIB2_DONE')
