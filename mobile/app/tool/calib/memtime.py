#!/usr/bin/env python3
"""Peak memory (max RSS) of stl2stp-cli convert per configuration, for the app's memory pre-check."""
import os, sys, time, subprocess, csv
CLI, OUT, CALIB = sys.argv[1], sys.argv[2], sys.argv[3]
src = open(CALIB).read(); ns = {}
exec(src.split('meshes = [')[0].replace('CLI = sys.argv[1]', 'CLI = None').replace('OUT = sys.argv[2]', 'OUT = None').replace('os.makedirs(OUT, exist_ok=True)', ''), ns)
os.makedirs(OUT, exist_ok=True)
meshes = [('sphere5k', lambda: ns['sphere'](64, 40)), ('sphere20k', lambda: ns['sphere'](128, 80)), ('sphere50k', lambda: ns['sphere'](200, 126)),
          ('sphere100k', lambda: ns['sphere'](320, 160)), ('box100k', lambda: ns['tess_box'](91))]
configs = [('original', []), ('merge', ['--merge-coplanar']), ('sewing', ['--mode', 'sewing']), ('simplify50', ['--simplify', 'HALF']),
           ('iges', [])]
rows = []
for name, gen in meshes:
    tris = gen(); n = len(tris); stl = os.path.join(OUT, name + '.stl'); ns['write_stl'](stl, tris); del tris
    for cfg, extra in configs:
        if cfg == 'sewing' and n > 60000: continue
        if name == 'box100k' and cfg not in ('original', 'merge'): continue
        out = os.path.join(OUT, name + ('.igs' if cfg == 'iges' else '.step'))
        cmd = [CLI, 'convert', stl, out] + [str(n // 2) if a == 'HALF' else a for a in extra]
        t0 = time.perf_counter(); pr = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        _, status, ru = os.wait4(pr.pid, 0); dt = time.perf_counter() - t0
        rows.append(dict(mesh=name, tris=n, config=cfg, rc=(status >> 8) if (status & 0x7f) == 0 else -(status & 0x7f), seconds=round(dt, 2),
                         max_rss_mb=round(ru.ru_maxrss / 1024, 1), rss_kb_per_tri=round(ru.ru_maxrss / n, 2)))
        print(rows[-1], flush=True)
        if os.path.exists(out): os.remove(out)
    os.remove(stl)
with open(os.path.join(OUT, 'memtime.csv'), 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys()), lineterminator='\n'); w.writeheader(); w.writerows(rows)
print('MEMTIME_DONE')
