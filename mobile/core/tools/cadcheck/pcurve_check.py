#!/usr/bin/env python3
"""Check that every PCURVE (2D line on a PLANE) of a SURFACE_CURVE agrees with its 3D LINE:
same direction (parametrisation sense) and lies on it. Independent of OCCT."""
import sys, math
sys.path.insert(0, __import__('os').path.dirname(__file__))
import step_check as s
def run(path):
    hdr, E = s.load(path)
    def g(i): return E[i][1]
    bad, n, maxoff, mindot = [], 0, 0.0, 1.0
    for i, (t, p) in E.items():
        if t != 'SURFACE_CURVE': continue
        c3 = g(p[1]); o3 = tuple(g(c3[1])[1]); d3 = s.unit(tuple(g(g(c3[2])[1])[1])); m3 = g(c3[2])[2]
        for pc in p[2]:
            if E[pc][0] != 'PCURVE': continue
            pp = g(pc); pl = g(pp[1]); ax = g(pl[1])
            O = tuple(g(ax[1])[1]); Z = s.unit(tuple(g(ax[2])[1])); X = s.unit(tuple(g(ax[3])[1])); Y = s.cross(Z, X)
            dr = g(pp[2]); l2 = E[dr[1][0]]
            if l2[0] != 'LINE': continue
            l2 = l2[1]; o2 = g(l2[1])[1]; v2 = g(l2[2]); d2 = g(v2[1])[1]; m2 = v2[2]
            P = tuple(O[k] + o2[0] * X[k] + o2[1] * Y[k] for k in range(3))
            D = tuple(d2[0] * X[k] + d2[1] * Y[k] for k in range(3))
            D = s.unit(D)
            off = s.norm(s.cross(s.sub(P, o3), d3)); dd = s.dot(D, d3)
            # parameter origin shift: point at t=0 on both
            shift = s.norm(s.sub(P, o3))
            n += 1; maxoff = max(maxoff, off); mindot = min(mindot, dd)
            if dd < 0.999999 or off > 1e-4 or shift > 1e-4 or abs(m2 - m3) > 1e-9 * max(1, abs(m3)):
                bad.append((i, pc, round(dd, 6), '%.2g' % off, '%.2g' % shift, m2, m3))
    print('%-36s pcurves=%d maxoff=%.2g min_dir_dot=%.8f bad=%d %s' % (path.split('/')[-1], n, maxoff, mindot, len(bad), bad[:4]))
for f in sys.argv[1:]: run(f)
