import 'models.dart';

/// Local time/size estimate (ui-spec §8). Placeholder coefficients from the
/// desktop benchmark (≈0.2 ms and ≈2.4 KB per face for STEP); phone ×1.5,
/// sewing ×2, verify ×1.3. Must be calibrated on real devices.
class Estimate {
  const Estimate(this.seconds, this.bytes);
  final double seconds;
  final int bytes;
}

Estimate estimate(int faces, ConvertSettings s, {double deviceFactor = 1.5}) {
  final f = s.simplify ? s.targetFaces(faces) : faces;
  var secs = f * 0.0002 * deviceFactor;
  if (s.mode == ConvertMode.sewing) secs *= 2;
  if (s.verifyEffective) secs *= 1.3;
  var perFace = s.format == ExportFormat.step ? 2400.0 : 1500.0;
  if (s.mergeCoplanar) perFace *= 0.65;
  return Estimate(secs, (f * perFace).round());
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
