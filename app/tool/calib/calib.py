#!/usr/bin/env python3
"""Calibrate time/size estimates: generate meshes, run stl2stp-cli, write CSV."""
import math, os, re, struct, subprocess, sys, time, csv

CLI = sys.argv[1]
OUT = sys.argv[2]
os.makedirs(OUT, exist_ok=True)

def write_stl(path, tris):
    with open(path, 'wb') as f:
        f.write(b'calib'.ljust(80, b' '))
        f.write(struct.pack('<I', len(tris)))
        for t in tris:
            f.write(struct.pack('<3f', 0, 0, 0))
            for v in t:
                f.write(struct.pack('<3f', *v))
            f.write(b'\0\0')

def sphere(seg, rings, r=25.0):
    def at(i, j):
        # exact single poles: sin(pi) != 0 would leave seg distinct south-pole points (an open mesh)
        if j == 0: return (0.0, 0.0, r)
        if j == rings: return (0.0, 0.0, -r)
        th = math.pi * j / rings; ph = 2 * math.pi * (i % seg) / seg
        return (r*math.sin(th)*math.cos(ph), r*math.sin(th)*math.sin(ph), r*math.cos(th))
    tris = []
    for j in range(rings):
        for i in range(seg):
            a, b, c, d = at(i, j), at(i+1, j), at(i+1, j+1), at(i, j+1)
            if j != 0: tris.append((a, d, b))
            if j != rings-1: tris.append((b, d, c))
    return tris

def tess_box(n, s=40.0):
    """Axis-aligned box, each face an n x n grid (planar-heavy, like machined parts)."""
    tris = []
    g = [s * k / n for k in range(n+1)]
    def face(fn):
        for i in range(n):
            for j in range(n):
                p00, p10, p11, p01 = fn(g[i], g[j]), fn(g[i+1], g[j]), fn(g[i+1], g[j+1]), fn(g[i], g[j+1])
                tris.append((p00, p10, p11)); tris.append((p00, p11, p01))
    face(lambda u, v: (u, v, s));    face(lambda u, v: (v, u, 0))
    face(lambda u, v: (u, 0, v));    face(lambda u, v: (v, s, u))
    face(lambda u, v: (0, u, v));    face(lambda u, v: (s, v, u))
    return tris

meshes = [
    ('cube', tess_box(1)),
    ('sphere5k', sphere(64, 40)),
    ('sphere20k', sphere(128, 80)),
    ('box20k', tess_box(41)),
    ('sphere50k', sphere(200, 126)),
    ('sphere100k', sphere(320, 160)),
    ('box100k', tess_box(91)),
]
configs = [
    ('step', 'original', False), ('step', 'original', True), ('step', 'sewing', False), ('iges', 'original', False),
]
ONLY = sys.argv[3].split(',') if len(sys.argv) > 3 else None  # optional mesh-name filter
rows = []
for name, tris in meshes:
    if ONLY and name not in ONLY: continue
    stl = os.path.join(OUT, name + '.stl'); write_stl(stl, tris)
    for fmt, mode, merge in configs:
        if len(tris) >= 100000 and mode == 'sewing': continue
        out = os.path.join(OUT, f'{name}_{mode}{"_m" if merge else ""}.{"step" if fmt=="step" else "igs"}')
        cmd = [CLI, 'convert', stl, out, '--mode', mode] + (['--merge-coplanar'] if merge else [])
        t0 = time.perf_counter(); p = subprocess.run(cmd, capture_output=True, text=True); dt = time.perf_counter() - t0
        size = os.path.getsize(out) if os.path.exists(out) else -1
        rb = ''
        if fmt == 'step' and mode == 'original' and not merge and p.returncode == 0:
            t1 = time.perf_counter(); subprocess.run([CLI, 'readback', out], capture_output=True); rb = round(time.perf_counter() - t1, 3)
        rows.append(dict(mesh=name, tris=len(tris), fmt=fmt, mode=mode, merge=int(merge), rc=p.returncode,
                         seconds=round(dt, 3), bytes=size, readback=rb,
                         solids=(re.findall(r'export solids=(\d+)', p.stdout + p.stderr) or [''])[0]))
        print(rows[-1], flush=True)
        if os.path.exists(out): os.remove(out)
    os.remove(stl)
with open(os.path.join(OUT, 'calib.csv'), 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
print('CALIB_DONE')
