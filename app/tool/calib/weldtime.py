#!/usr/bin/env python3
"""Weld timing impact: stl2stp-cli convert (STEP, original) with default weld vs --no-weld."""
import os, re, sys, time, subprocess, csv, math
CLI, OUT, CALIB = sys.argv[1], sys.argv[2], sys.argv[3]
src = open(CALIB).read()
ns = {}
exec(src.split('meshes = [')[0].replace('CLI = sys.argv[1]', 'CLI = None').replace('OUT = sys.argv[2]', 'OUT = None').replace('os.makedirs(OUT, exist_ok=True)', ''), ns)
os.makedirs(OUT, exist_ok=True)
def open_sphere(seg, rings, r=25.0):  # old generator: sin(pi) south pole
    def at(i, j):
        th = math.pi * j / rings; ph = 2 * math.pi * (i % seg) / seg
        return (r*math.sin(th)*math.cos(ph), r*math.sin(th)*math.sin(ph), r*math.cos(th))
    tris = []
    for j in range(rings):
        for i in range(seg):
            a, b, c, d = at(i, j), at(i+1, j), at(i+1, j+1), at(i, j+1)
            if j != 0: tris.append((a, d, b))
            if j != rings-1: tris.append((b, d, c))
    return tris
meshes = [('sphere5k', lambda: ns['sphere'](64, 40), None), ('sphere20k', lambda: ns['sphere'](128, 80), None),
          ('box20k', lambda: ns['tess_box'](41), None), ('sphere50k', lambda: ns['sphere'](200, 126), None),
          ('sphere50k_s50', lambda: ns['sphere'](200, 126), 25000), ('open_sphere50k', lambda: open_sphere(200, 126), None),
          ('sphere100k', lambda: ns['sphere'](320, 160), None), ('box100k', lambda: ns['tess_box'](91), None)]
rows = []
def num(pat, text):
    m = re.search(pat, text); return m.group(1) if m else ''
for name, gen, simplify in meshes:
    tris = gen(); stl = os.path.join(OUT, name + '.stl'); ns['write_stl'](stl, tris); del tris
    for weld in (True, False):
        out = os.path.join(OUT, name + '.step')
        cmd = [CLI, 'convert', stl, out] + (['--simplify', str(simplify)] if simplify else []) + ([] if weld else ['--no-weld'])
        t0 = time.perf_counter(); p = subprocess.run(cmd, capture_output=True, text=True); dt = time.perf_counter() - t0
        log = p.stdout + p.stderr
        rows.append(dict(mesh=name, weld=int(weld), rc=p.returncode, seconds=round(dt, 3), export_ms=num(r'export_ms=([\d.]+)', log),
                         weld_ms=num(r'weld_ms=([\d.]+)', log), solids=num(r'export solids=(\d+)', log),
                         welded=num(r'(\d+) near-coincident', log) or num(r'welded_vertices=(\d+)', log), bytes=os.path.getsize(out) if os.path.exists(out) else -1))
        print(rows[-1], flush=True)
        if os.path.exists(out): os.remove(out)
    os.remove(stl)
with open(os.path.join(OUT, 'weldtime.csv'), 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys()), lineterminator='\n'); w.writeheader(); w.writerows(rows)
print('WELDTIME_DONE')
