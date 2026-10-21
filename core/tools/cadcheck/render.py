import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import step_check as s
files = sys.argv[2:]; outp = sys.argv[1]
fig = plt.figure(figsize=(5 * len(files), 5.4))
for k, f in enumerate(files):
    hdr, E = s.load(f); r = s.check(f)
    g = lambda i: E[i][1]
    vpt = lambda v: tuple(g(g(v)[1])[1])
    polys, arrows = [], []
    for i, (t, p) in E.items():
        if t != 'ADVANCED_FACE': continue
        for b in p[1]:
            bp = g(b); loop = g(bp[1])[1]; pts = []
            for oe in loop:
                op = g(oe); ec = g(op[3]); fwd = (op[4] == '.T.') == (bp[2] == '.T.')
                pts.append(vpt(ec[1] if fwd else ec[2]))
            if bp[2] != '.T.': pts = pts[::-1]
            polys.append(pts)
            ax = g(g(p[2])[1]); n = s.unit(tuple(g(ax[2])[1]))
            if p[3] != '.T.': n = tuple(-x for x in n)
            c = tuple(sum(q[j] for q in pts) / len(pts) for j in range(3))
            arrows.append((c, n))
    a = fig.add_subplot(1, len(files), k + 1, projection='3d')
    a.add_collection3d(Poly3DCollection(polys, facecolor=(0.55, 0.7, 0.95, 0.45), edgecolor='k', linewidth=0.6))
    allp = [q for pl in polys for q in pl]
    lo = [min(q[j] for q in allp) for j in range(3)]; hi = [max(q[j] for q in allp) for j in range(3)]
    L = max(hi[j] - lo[j] for j in range(3))
    if len(arrows) <= 60:
        for c, n in arrows:
            a.quiver(c[0], c[1], c[2], n[0], n[1], n[2], length=0.35 * L, color='r', linewidth=1.2)
    for j, fn in enumerate((a.set_xlim, a.set_ylim, a.set_zlim)):
        m = (lo[j] + hi[j]) / 2; fn(m - 0.75 * L, m + 0.75 * L)
    sh = r.get('shells', [])
    a.set_title('%s\n%s | %s | vol=%s\nfaces=%d, red = face normal (should point out)' % (
        os.path.basename(f), r['result'], ','.join(x['type'] for x in sh),
        ','.join('%.4f' % x['signed_volume'] for x in sh), len(polys)), fontsize=8)
plt.tight_layout(); plt.savefig(outp, dpi=110)
print('saved', outp)
