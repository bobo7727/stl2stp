import 'dart:math' as math;

import 'models.dart';

/// Time/size estimates (ui-spec §8), calibrated with stl2stp-core itself.
///
/// Calibration (docs/flutter-app.md §估算校准): `stl2stp-cli convert` on this
/// project's dev PC (WSL2 Ubuntu 20.04, gcc-10 -O2, OCCT 8.0.1, output written
/// to an NTFS drive through /mnt/d), 2026-10-21, idle machine. Meshes: closed UV
/// spheres (curved) and finely tessellated boxes (planar) with 5k–100k triangles,
/// all read back as one solid; STEP/IGES, original/sewing, merge on/off,
/// simplify 25/50 %. Residuals of the fit are within about −22 % … +35 %, which
/// [Coefficients.modelLo]/[modelHi] cover. An earlier session on the same PC
/// while other builds ran was 1.1–1.9× slower; that stays inside the range once
/// the device spread ([Estimator.deviceHi]) is included.
///
/// Model (seconds on the reference machine):
///   export  = base + exportPerFace · F_out  (+ mergePerTriangle · f if merging)
///             × sewingFactor in sewing mode
///   verify  = verifyPerMB · output MB       (STEP with “verify” on)
///   simplify= simplifyPerTriangle · n       (QEM, cheap)
/// where n = input triangles, f = triangles after simplification and
/// F_out = CAD faces written: f, or ≈ distinct planes when merging coplanar
/// faces (counted from the mesh by PlaneCounter, see stl_reader.dart).
/// The phone is accounted for by [Estimator.deviceFactor] (configurable).
class Coefficients {
  const Coefficients({
    this.baseSeconds = 0.07,
    this.exportPerFace = 0.33e-3,
    this.mergePerTriangle = 0.34e-3,
    this.sewingFactor = 1.25,
    this.verifyPerMB = 0.18,
    this.simplifyPerTriangle = 6e-6,
    this.headerBytes = 8000,
    this.stepBytesPerFace = 2400,
    this.igesBytesPerFace = 1870,
    this.mergedFaceBytesFactor = 1.3,
    this.sewingBytesFactor = 1.05,
    this.planarFaceBytesFactor = 0.6,
    this.simplifiedLo = 1.0,
    this.modelLo = 0.75,
    this.modelHi = 1.35,
    this.sizeLo = 0.9,
    this.sizeHi = 1.1,
  });

  /// Process start, STL read, document setup.
  final double baseSeconds;

  /// Building + writing one CAD face (STEP and IGES measured within 5 %).
  final double exportPerFace;

  /// Coplanar detection per triangle (box100k → 6 faces still took 28 s).
  final double mergePerTriangle;

  /// Sewing vs original export time; measured 1.0–1.7× (mean ≈ 1.25×).
  final double sewingFactor;

  /// Read-back verification per MB of STEP output (≈ 0.17–0.2 s/MB measured).
  final double verifyPerMB;
  final double simplifyPerTriangle;

  /// Fixed STEP/IGES header and product structure.
  final double headerBytes;

  /// One triangle face (STEP: 2210–2510 B, IGES: 1710–2030 B measured).
  final double stepBytesPerFace;
  final double igesBytesPerFace;

  /// A merged polygon face is larger than a triangle face (more edges):
  /// UV-sphere quads measured 3020–3210 B in STEP ≈ 1.3 × 2400.
  final double mergedFaceBytesFactor;
  final double sewingBytesFactor;

  /// When merging leaves very few faces (< 5 % of triangles: boxes, plates),
  /// faces collapse to clean polygons: box → 6 faces measured ≈ 16 KB total.
  final double planarFaceBytesFactor;

  /// Extra lower spread after simplification (1 = none). Simplified closed
  /// meshes convert at the normal per-face rate (sphere 50k → 25k: 18.4 s vs
  /// 37.1 s unsimplified). The first calibration's “0.67× faster” simplified
  /// sphere was an open test mesh (unwelded pole in the old generator), not a
  /// simplifier effect.
  final double simplifiedLo;

  /// Spread of the time model on the reference machine.
  final double modelLo, modelHi;

  /// Spread of the size model.
  final double sizeLo, sizeHi;

  /// Calibrated defaults (see class doc).
  static const calibrated = Coefficients();
}

/// What we know about the mesh before converting.
class MeshStats {
  const MeshStats(this.triangles, {this.planeGroups});

  final int triangles;

  /// Distinct planes (≈ faces after merging coplanar), null if unknown.
  final int? planeGroups;
}

/// A closed range [lo, hi] with its central value.
class Range {
  const Range(this.lo, this.mid, this.hi) : assert(lo <= mid && mid <= hi);
  final double lo, mid, hi;
  @override
  String toString() => 'Range($lo, $mid, $hi)';
}

class Estimate {
  const Estimate(this.seconds, this.bytes, {required this.outputFaces});
  final Range seconds;
  final Range bytes;

  /// Estimated CAD faces in the output.
  final int outputFaces;
}

/// Turns [MeshStats] + [ConvertSettings] into time/size ranges.
class Estimator {
  const Estimator({
    this.coefficients = Coefficients.calibrated,
    this.deviceFactor = defaultDeviceFactor,
    this.deviceLo = 0.75,
    this.deviceHi = 1.5,
  });

  /// Phone time ÷ reference-PC time. Not measured yet (needs a Mac + iPhone);
  /// 1.0 assumes a recent iPhone ≈ one laptop core, and the reference PC wrote
  /// to a slow /mnt/d mount, which partly offsets phone throttling.
  static const defaultDeviceFactor = 1.0;

  final Coefficients coefficients;
  final double deviceFactor;

  /// Uncertainty of [deviceFactor] (multiplies the range ends).
  final double deviceLo, deviceHi;

  /// CAD faces written for [f] triangles (after simplification of [n]).
  /// Returns (mid, lo, hi).
  (double, double, double) _outputFaces(int n, int f, ConvertSettings s, int? planes) {
    if (!s.mergeCoplanar) return (f.toDouble(), f.toDouble(), f.toDouble());
    if (planes == null) {
      // Unknown planarity: anything from a box (few faces) to a scan (no merging).
      return (0.6 * f, 0.02 * f, f.toDouble());
    }
    // Planar regions survive simplification, curved ones shrink with it;
    // sqrt matched the 50 % simplified sphere (17.8k vs 17.6k measured).
    final p = math.min(planes, n).toDouble();
    final mid = math.min(f.toDouble(), p * math.sqrt(f / math.max(n, 1)));
    return (math.max(mid, 1), math.max(mid * 0.7, 1), math.min(mid * 1.4, f.toDouble()));
  }

  Estimate estimate(MeshStats m, ConvertSettings s) {
    final c = coefficients;
    final n = math.max(m.triangles, 0);
    final f = s.simplify ? s.targetFaces(n) : n;
    final (facesMid, facesLo, facesHi) = _outputFaces(n, f, s, m.planeGroups);
    final iges = s.format == ExportFormat.iges;
    final sewing = s.mode == ConvertMode.sewing;

    double bytesFor(double faces) {
      var perFace = iges ? c.igesBytesPerFace : c.stepBytesPerFace;
      if (s.mergeCoplanar && faces < 0.05 * f) {
        perFace *= c.planarFaceBytesFactor;
      } else if (s.mergeCoplanar && faces < f) {
        // Blend towards the merged-polygon cost as faces get merged.
        perFace *= 1 + (c.mergedFaceBytesFactor - 1) * (1 - faces / math.max(f, 1));
      }
      var b = c.headerBytes + perFace * faces;
      if (sewing) b *= c.sewingBytesFactor;
      return b;
    }

    final bMid = bytesFor(facesMid), bLo = bytesFor(facesLo) * c.sizeLo, bHi = bytesFor(facesHi) * c.sizeHi;

    double secondsFor(double faces, double bytes) {
      var export = c.exportPerFace * faces + (s.mergeCoplanar ? c.mergePerTriangle * f : 0);
      if (sewing) export *= c.sewingFactor;
      final verify = s.verifyEffective ? c.verifyPerMB * bytes / 1e6 : 0;
      final simplify = s.simplify ? c.simplifyPerTriangle * n : 0;
      return c.baseSeconds + export + verify + simplify;
    }

    final tMid = secondsFor(facesMid, bMid) * deviceFactor;
    final tLo = secondsFor(facesLo, bytesFor(facesLo)) * deviceFactor * c.modelLo * deviceLo *
        (s.simplify ? c.simplifiedLo : 1);
    final tHi = secondsFor(facesHi, bytesFor(facesHi)) * deviceFactor * c.modelHi * deviceHi;
    return Estimate(
      Range(math.min(tLo, tMid), tMid, math.max(tHi, tMid)),
      Range(math.min(bLo, bMid), bMid, math.max(bHi, bMid)),
      outputFaces: facesMid.round(),
    );
  }
}

/// Formats a time range with the app strings' wording:
/// `(key, params)` for S.t — e.g. ('est.range_seconds', {lo: 4, hi: 9}).
(String, Map<String, Object>) timeRangeKey(Range r) {
  if (r.hi < 1) return ('estimate.under_1s', const {});
  if (r.hi < 90) {
    final lo = math.max(1, r.lo.floor()), hi = math.max(lo + 1, r.hi.ceil());
    return ('est.range_seconds', {'lo': lo, 'hi': hi});
  }
  final lo = math.max(1, (r.lo / 60).floor()), hi = math.max(lo + 1, (r.hi / 60).ceil());
  if (r.lo < 60) return ('est.range_seconds_minutes', {'lo': math.max(1, r.lo.floor()), 'hi': hi});
  return ('est.range_minutes', {'lo': lo, 'hi': hi});
}

/// “2.3–3.1 MB”, or “900 KB–1.2 MB” across units.
String formatBytesRange(Range r) {
  final lo = formatBytes(r.lo.round()), hi = formatBytes(r.hi.round());
  if (lo == hi) return lo;
  final lu = lo.split(' ').last, hu = hi.split(' ').last;
  return lu == hu ? '${lo.split(' ').first}–$hi' : '$lo–$hi';
}

String formatBytes(int bytes) {
  if (bytes < 1024) return '$bytes B';
  if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(0)} KB';
  if (bytes < 1024 * 1024 * 1024) return '${(bytes / 1024 / 1024).toStringAsFixed(1)} MB';
  return '${(bytes / 1024 / 1024 / 1024).toStringAsFixed(2)} GB';
}

String formatCount(int n) {
  final s = n.toString();
  final b = StringBuffer();
  for (var i = 0; i < s.length; i++) {
    if (i > 0 && (s.length - i) % 3 == 0) b.write(',');
    b.write(s[i]);
  }
  return b.toString();
}

String formatDim(double v) {
  if (v.abs() >= 100) return v.toStringAsFixed(0);
  if (v.abs() >= 10) return v.toStringAsFixed(1).replaceAll(RegExp(r'\.0$'), '');
  return v.toStringAsFixed(2).replaceAll(RegExp(r'\.?0+$'), '');
}
