#!/usr/bin/env python3
"""Independent (non-OCCT) B-rep validity checker for planar STEP AP203/AP214/AP242 files.

Pure Python 3.8+, no dependencies. Parses ISO 10303-21 directly and checks:
 refs, loop closure, vertex-on-line / vertex-on-plane, edge sense, face-loop vs surface
 normal, closed-shell edge use (each edge exactly twice, opposite directions = orientable),
 Euler-Poincare, signed volume (outward orientation), header timestamp and \\X2\\ strings.
"""
import re, sys, math, json
from collections import defaultdict, Counter

def split_entities(data):
    out, buf, instr, i, n = [], [], False, 0, len(data)
    start = 0
    while i < n:
        ch = data[i]
        if ch == "'":
            if instr and i + 1 < n and data[i + 1] == "'":
                i += 2; continue
            instr = not instr
        elif ch == ';' and not instr:
            out.append(data[start:i].strip()); start = i + 1
        i += 1
    return [e for e in out if e]

TOK = re.compile(r"\s*(?:(?P<str>'(?:[^']|'')*')|(?P<ref>#\d+)|(?P<enum>\.[A-Z0-9_]+\.)|(?P<num>[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)|(?P<kw>[A-Z_][A-Z0-9_]*)|(?P<p>[(),$*]))")

def parse_params(s):
    pos, stack, cur = 0, [], []
    root = cur
    while pos < len(s):
        m = TOK.match(s, pos)
        if not m:
            if s[pos:].strip() == '': break
            raise ValueError('tokenize error at %r' % s[pos:pos + 40])
        pos = m.end()
        k = m.lastgroup; v = m.group(k)
        if k == 'p':
            if v == '(':
                new = []; cur.append(new); stack.append(cur); cur = new
            elif v == ')':
                cur = stack.pop()
            elif v in '$*':
                cur.append(None)
        elif k == 'num': cur.append(float(v))
        elif k == 'str': cur.append(v[1:-1].replace("''", "'"))
        elif k == 'ref': cur.append(int(v[1:]))
        elif k == 'enum': cur.append(v)
        elif k == 'kw': cur.append(('KW', v))
    return root

def decode_x2(s):
    def rep(m):
        h = m.group(1); return ''.join(chr(int(h[i:i + 4], 16)) for i in range(0, len(h), 4))
    s = re.sub(r'\\X2\\([0-9A-F]+)\\X0\\', rep, s)
    s = re.sub(r'\\X\\([0-9A-F]{2})', lambda m: chr(int(m.group(1), 16)), s)
    return s

def load(path):
    txt = open(path, 'r', encoding='latin-1').read()
    hdr = txt[txt.index('HEADER;'):txt.index('ENDSEC;')]
    d0 = txt.index('DATA;') + 5; d1 = txt.index('ENDSEC;', d0)
    ents = {}
    for e in split_entities(txt[d0:d1]):
        m = re.match(r'#(\d+)\s*=\s*(.*)$', e, re.S)
        if not m: continue
        eid, body = int(m.group(1)), m.group(2).strip()
        if body.startswith('('):  # complex entity
            parts = re.findall(r'([A-Z_][A-Z0-9_]*)\s*(\((?:[^()]|\((?:[^()]|\([^()]*\))*\))*\))', body)
            ents[eid] = ('COMPLEX', {n: parse_params(p)[0] for n, p in parts})
        else:
            m2 = re.match(r'([A-Z_][A-Z0-9_]*)\s*(\(.*\))$', body, re.S)
            ents[eid] = (m2.group(1), parse_params(m2.group(2))[0])
    return hdr, ents

def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def norm(a): return math.sqrt(dot(a, a))
def unit(a):
    l = norm(a); return (a[0] / l, a[1] / l, a[2] / l)

def check(path):
    hdr, E = load(path)
    R = {'file': path.split('/')[-1], 'errors': [], 'warnings': []}
    err = lambda s: R['errors'].append(s) if len(R['errors']) < 40 else None
    warn = lambda s: R['warnings'].append(s) if len(R['warnings']) < 20 else None
    if not any(t == 'ADVANCED_FACE' for t, _ in E.values()):
        err('no supported ADVANCED_FACE entities found')
    # header
    m = re.search(r"FILE_NAME\s*\((.*?)\)\s*;", hdr, re.S)
    if m:
        fn = parse_params('(' + m.group(1) + ')')[0]
        R['header'] = {'name': decode_x2(fn[0]), 'time_stamp': fn[1], 'author': [decode_x2(x) for x in fn[2]],
                       'organization': [decode_x2(x) for x in fn[3]]}
        ts = fn[1]
        R['header']['timestamp_utc_ok'] = bool(re.match(r'^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(Z|\+00:00)?$', ts))
    m = re.search(r"FILE_SCHEMA\s*\(\s*\(\s*'([^']*)'", hdr)
    R['schema'] = m.group(1) if m else None
    nonascii = [c for c in open(path, 'rb').read() if c > 127]
    R['non_ascii_bytes'] = len(nonascii)
    if nonascii: err('file contains %d non-ASCII bytes (Part 21 requires ASCII; use \\X2\\)' % len(nonascii))
    prods = [decode_x2(p[0] or '') for t, p in E.values() if t == 'PRODUCT']
    R['products'] = prods
    # refs
    def walk(x):
        if isinstance(x, int): yield x
        elif isinstance(x, list):
            for y in x: yield from walk(y)
        elif isinstance(x, dict):
            for y in x.values(): yield from walk(y)
    dangling = [(i, r) for i, (t, p) in E.items() for r in walk(p) if r not in E]
    if dangling: err('dangling references: %s' % dangling[:5])
    def g(i, typ=None):
        t, p = E[i]
        if typ and t != typ and not (t == 'COMPLEX' and typ in p):
            raise KeyError('#%d is %s, expected %s' % (i, t, typ))
        return p if t != 'COMPLEX' else p.get(typ, p)
    pt = lambda i: tuple(g(i, 'CARTESIAN_POINT')[1])
    dr = lambda i: unit(tuple(g(i, 'DIRECTION')[1]))
    allpts = [tuple(p[1]) for t, p in E.values() if t == 'CARTESIAN_POINT' and len(p[1]) == 3]
    lo = [min(p[k] for p in allpts) for k in range(3)]; hi = [max(p[k] for p in allpts) for k in range(3)]
    diag = norm(sub(hi, lo)) or 1.0
    tol = 1e-6 * diag + 1e-7
    R['tol'] = tol
    vpt = lambda v: pt(g(v, 'VERTEX_POINT')[1])
    def curve_geom(c):
        t, p = E[c]
        if t in ('SURFACE_CURVE', 'SEAM_CURVE'): return curve_geom(p[1])
        if t == 'LINE':
            o = pt(p[1]); vec = g(p[2], 'VECTOR'); return ('LINE', o, dr(vec[1]))
        return (t, None, None)
    def surf_geom(s):
        t, p = E[s]
        if t == 'PLANE':
            ax = g(p[1], 'AXIS2_PLACEMENT_3D')
            return ('PLANE', pt(ax[1]), dr(ax[2]) if ax[2] is not None else (0, 0, 1))
        return (t, None, None)
    maxdev = {'vertex_line': 0.0, 'vertex_plane': 0.0}
    edge_info = {}
    for i, (t, p) in E.items():
        if t != 'EDGE_CURVE': continue
        v1, v2 = vpt(p[1]), vpt(p[2]); kind, o, d = curve_geom(p[3]); same = p[4] == '.T.'
        if kind != 'LINE':
            warn('edge #%d curve type %s not checked' % (i, kind)); edge_info[i] = (p[1], p[2]); continue
        for v in (v1, v2):
            dev = norm(cross(sub(v, o), d)); maxdev['vertex_line'] = max(maxdev['vertex_line'], dev)
            if dev > tol * 10: err('edge #%d vertex off its line by %.3g' % (i, dev))
        s = dot(sub(v2, v1), d) * (1 if same else -1)
        if norm(sub(v2, v1)) > tol and s <= 0: err('edge #%d same_sense flag contradicts line direction' % i)
        if norm(sub(v2, v1)) <= tol: err('edge #%d is degenerate (zero length)' % i)
        edge_info[i] = (p[1], p[2])
    # faces
    face_data = {}
    for i, (t, p) in E.items():
        if t not in ('ADVANCED_FACE', 'FACE_SURFACE'): continue
        kind, o, n = surf_geom(p[2]); fsame = p[3] == '.T.'
        if kind != 'PLANE': warn('face #%d surface %s not checked' % (i, kind)); continue
        fn = n if fsame else (-n[0], -n[1], -n[2])
        uses, area_vec, nouter, vol = [], (0.0, 0.0, 0.0), 0, 0.0
        for b in p[1]:
            bt, bp = E[b]; borient = bp[2] == '.T.'
            if bt == 'FACE_OUTER_BOUND': nouter += 1
            loop = g(bp[1], 'EDGE_LOOP')[1]
            seq = []
            for oe in loop:
                op = g(oe, 'ORIENTED_EDGE'); e = op[3]; fwd = (op[4] == '.T.') == borient
                a, bb = edge_info[e]
                seq.append((a, bb) if fwd else (bb, a)); uses.append((e, fwd))
            if not borient: seq = seq[::-1]
            for k in range(len(seq)):
                if seq[k][1] != seq[(k + 1) % len(seq)][0]:
                    err('face #%d loop #%d not closed at position %d' % (i, bp[1], k)); break
            P = [vpt(a) for a, _ in seq]
            for q in P:
                dev = abs(dot(sub(q, o), n)); maxdev['vertex_plane'] = max(maxdev['vertex_plane'], dev)
                if dev > tol * 10: err('face #%d vertex off plane by %.3g' % (i, dev))
            nv = [0.0, 0.0, 0.0]
            for k in range(len(P)):
                c = cross(P[k], P[(k + 1) % len(P)]); nv = [nv[j] + c[j] for j in range(3)]
            nv = tuple(x / 2 for x in nv)
            if bt == 'FACE_OUTER_BOUND' or len(p[1]) == 1:
                if dot(nv, fn) <= 0: err('face #%d outer loop runs clockwise w.r.t. face normal (face orientation inconsistent)' % i)
            else:
                if dot(nv, fn) >= 0: err('face #%d inner loop not clockwise w.r.t. face normal' % i)
            area_vec = tuple(area_vec[j] + nv[j] for j in range(3))
            vol += dot(P[0], nv) / 3.0
        if len(p[1]) > 1 and nouter != 1: warn('face #%d has %d FACE_OUTER_BOUND with %d bounds' % (i, nouter, len(p[1])))
        face_data[i] = {'uses': uses, 'area': dot(area_vec, fn), 'vol': vol,
                        'nloops': len(p[1])}
    # shells
    shells = []
    solids_of = {}
    for i, (t, p) in E.items():
        if t == 'MANIFOLD_SOLID_BREP': solids_of[p[1]] = i
    for i, (t, p) in E.items():
        if t not in ('CLOSED_SHELL', 'OPEN_SHELL'): continue
        faces = p[1]; use = defaultdict(list); verts = set(); loops = 0; vol = 0.0; area = 0.0
        for f in faces:
            fd = face_data.get(f)
            if not fd: continue
            loops += fd['nloops']; vol += fd['vol']; area += fd['area']
            for e, fwd in fd['uses']:
                use[e].append(fwd); verts.update(edge_info[e])
        V, Ed, F = len(verts), len(use), len(faces)
        bad_count = {e: len(u) for e, u in use.items() if len(u) != 2}
        same_dir = [e for e, u in use.items() if len(u) == 2 and u[0] == u[1]]
        sh = {'id': i, 'type': t, 'in_solid': i in solids_of, 'faces': F, 'edges': Ed, 'vertices': V,
              'free_edges': sum(1 for c in bad_count.values() if c == 1),
              'nonmanifold_edges': sum(1 for c in bad_count.values() if c > 2),
              'same_direction_edge_pairs': len(same_dir), 'area': area, 'signed_volume': vol,
              'euler_V-E+F-(L-F)': V - Ed + F - (loops - F)}
        if t == 'CLOSED_SHELL':
            if bad_count: err('closed shell #%d: %d edges not used exactly twice' % (i, len(bad_count)))
            if same_dir: err('closed shell #%d: %d edges used twice in the SAME direction -> shell not consistently oriented (unorientable), e.g. edges %s' % (i, len(same_dir), same_dir[:5]))
            if sh['euler_V-E+F-(L-F)'] % 2: err('closed shell #%d odd Euler characteristic' % i)
            if vol <= 0: err('closed shell #%d signed volume %.6g <= 0 (faces point inward)' % (i, vol))
        else:
            if same_dir: err('open shell #%d: %d interior edges used in the SAME direction (inconsistent orientation)' % (i, len(same_dir)))
        shells.append(sh)
    R['counts'] = dict(Counter(t for t, _ in E.values() if t in (
        'MANIFOLD_SOLID_BREP', 'CLOSED_SHELL', 'OPEN_SHELL', 'ADVANCED_FACE', 'EDGE_CURVE', 'VERTEX_POINT',
        'SHELL_BASED_SURFACE_MODEL', 'MANIFOLD_SURFACE_SHAPE_REPRESENTATION', 'ADVANCED_BREP_SHAPE_REPRESENTATION', 'PCURVE', 'SURFACE_CURVE')))
    R['shells'] = shells
    R['max_dev'] = maxdev
    R['result'] = 'PASS' if not R['errors'] else 'FAIL'
    return R

if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit('Usage: step_check.py FILE.step [FILE.step ...]')
    allr = []
    for f in sys.argv[1:]:
        try:
            r = check(f)
        except Exception as ex:
            r = {'file': f.split('/')[-1], 'result': 'ERROR', 'errors': [repr(ex)]}
        allr.append(r)
        sh = r.get('shells', [])
        print('%-40s %s  shells=%s vol=%s area=%s maxdev=%s' % (
            r['file'], r['result'], [(s['type'][:4], s['faces'], s['in_solid']) for s in sh][:4],
            [round(s['signed_volume'], 5) for s in sh][:4], [round(s['area'], 5) for s in sh][:4],
            {k: '%.2g' % v for k, v in r.get('max_dev', {}).items()}))
        if r.get('header'): print('    header:', json.dumps(r['header'], ensure_ascii=False), 'products:', r.get('products'), 'schema:', r.get('schema'))
        for e in r['errors'][:8]: print('    ERROR:', e)
        for w in r.get('warnings', [])[:3]: print('    warn:', w)
    with open('step_check_report.json', 'w', encoding='utf-8') as report:
        json.dump(allr, report, ensure_ascii=False, indent=1)
    sys.exit(0 if all(r['result'] == 'PASS' for r in allr) else 1)
