# FreeCAD (Windows, FreeCADCmd) validity check of STEP/IGES exports. Usage: FreeCADCmd fc_check.py -- <dir> <report>
import sys, os, glob, traceback
import FreeCAD, Part
args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[-2:]
d, rep = args[0], args[1]
out = open(rep, 'w', encoding='utf-8')
def p(*a):
    s = ' '.join(str(x) for x in a); out.write(s + '\n'); out.flush()
p('FreeCAD', '.'.join(FreeCAD.Version()[:3]), 'OCC', Part.OCC_VERSION)
for f in sorted(glob.glob(os.path.join(d, '*.step')) + glob.glob(os.path.join(d, '*.igs'))):
    name = os.path.basename(f)
    try:
        s = Part.read(f)
        info = 'solids=%d shells=%d faces=%d edges=%d vertices=%d' % (len(s.Solids), len(s.Shells), len(s.Faces), len(s.Edges), len(s.Vertexes))
        vols = [round(x.Volume, 6) for x in s.Solids][:4]
        closed = [x.isClosed() for x in s.Shells][:4]
        valid = s.isValid()
        bop = 'ok'
        try:
            s.check(True)
        except Exception as ex:
            bop = 'FAIL: ' + str(ex).replace('\n', ' | ')[:600]
        p('%-38s valid=%s bopcheck=%s %s volumes=%s shells_closed=%s area=%.6f' % (name, valid, bop, info, vols, closed, s.Area))
    except Exception as ex:
        p('%-38s READ ERROR %s' % (name, traceback.format_exc().replace('\n', ' | ')[:500]))
p('done')
out.close()
