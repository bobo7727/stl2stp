#!/bin/bash
# Generate representative exports for external CAD validation (cadcheck2)
cd /mnt/d/stl2stp-ios
CLI=build/cc2-release/stl2stp-cli; FX=build/cadcheck2/fx; O=build/cadcheck2/out
rm -rf $O; mkdir -p $O
python3 core/tools/cadcheck/gen.py $FX
TS=2026-10-21T10:30:00+08:00
c() { local in=$1 outf=$2; shift 2; echo "== $outf $*"; $CLI convert $FX/$in $O/$outf --timestamp $TS "$@" 2>&1 | tr '\n' ' '; echo; }
c cube-binary.stl cube-original.step --mode original
c cube-binary.stl cube-sewing.step --mode sewing
c cube-binary.stl cube-original.igs --mode original
c rot-cube-4.stl rot4-original-merge.step --mode original --merge-coplanar
c rot-cube-4.stl rot4-sewing-merge.step --mode sewing --merge-coplanar
c rot-cube-10.stl rot10-original-merge.step --mode original --merge-coplanar
c rot-cube-10.stl rot10-sewing-merge.step --mode sewing --merge-coplanar
c rot-cube-10.stl rot10-original-nomerge.step --mode original
c rot-cube-10.stl rot10-original-merge.igs --mode original --merge-coplanar
c two-shells.stl two-shells-original.step --mode original
c two-shells.stl two-shells-sewing.step --mode sewing
c rot-two-bodies.stl rot-two-bodies-original-merge.step --mode original --merge-coplanar
c open-cube-4.stl open-cube-original.step --mode original
c open-cube-4.stl open-cube-sewing-merge.step --mode sewing --merge-coplanar
c sphere.stl sphere-original.step --mode original
c sphere.stl sphere-sewing-merge.step --mode sewing --merge-coplanar
c cube-binary.stl meta-cn.step --mode original --author "致知屋" --org "测试公司（深圳）" --product "零件 A-01" --description "中文描述 °±"
c cube-binary.stl meta-cn.igs --mode original --author "致知屋" --org "测试公司（深圳）" --product "零件 A-01" --description "中文描述 °±"
for f in $O/*.step; do echo "## readback $(basename $f): $($CLI readback $f 2>&1 | tr '\n' ' ')"; done
du -sh $O; ls -la $O
