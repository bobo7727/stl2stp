import 'dart:convert';
import 'dart:io';
import 'dart:math' as math;
import 'dart:typed_data';

/// Triangles for the 3D preview (9 floats per triangle) plus format info.
///
/// This is a light Dart-side reader used only for the preview and the
/// ASCII/binary label (the C API does not expose the format yet, core TODO 2).
/// The conversion itself always goes through stl2stp-core.
class StlMesh {
  StlMesh(this.positions, this.isAscii, this.totalTriangles, {this.planeGroups});

  /// x0,y0,z0,x1,y1,z1,x2,y2,z2 per triangle (possibly decimated for display).
  final Float32List positions;
  final bool isAscii;

  /// Triangle count in the file (before display decimation).
  final int totalTriangles;

  /// Approximate number of CAD faces after “merge coplanar” = distinct planes
  /// over all triangles (see [PlaneCounter]); null if not computed.
  final int? planeGroups;

  int get triangleCount => positions.length ~/ 9;

  ({List<double> min, List<double> max}) bounds() {
    final mn = [double.infinity, double.infinity, double.infinity];
    final mx = [-double.infinity, -double.infinity, -double.infinity];
    for (var i = 0; i < positions.length; i++) {
      final a = i % 3;
      final v = positions[i];
      if (v < mn[a]) mn[a] = v;
      if (v > mx[a]) mx[a] = v;
    }
    if (positions.isEmpty) return (min: [0, 0, 0], max: [0, 0, 0]);
    return (min: mn, max: mx);
  }

  /// Unique vertex count (exact float match), used by the mock engine.
  int uniqueVertexCount() {
    final seen = <String>{};
    for (var i = 0; i < positions.length; i += 3) {
      seen.add('${positions[i]},${positions[i + 1]},${positions[i + 2]}');
    }
    return seen.length;
  }
}

class StlFormatException implements Exception {
  StlFormatException(this.message);
  final String message;
  @override
  String toString() => 'StlFormatException: $message';
}

/// True when the header looks like ASCII STL (starts with "solid" and the size
/// does not match the binary layout 84 + 50*N).
bool looksAscii(Uint8List bytes) {
  if (bytes.length >= 84) {
    final n = ByteData.sublistView(bytes, 80, 84).getUint32(0, Endian.little);
    if (84 + 50 * n == bytes.length) return false;
  }
  final head = ascii.decode(bytes.sublist(0, math.min(5, bytes.length)), allowInvalid: true).toLowerCase();
  return head == 'solid';
}

StlMesh parseStlBytes(Uint8List bytes, {int maxDisplayTriangles = 60000}) {
  if (bytes.length < 15) throw StlFormatException('file too small');
  if (looksAscii(bytes)) return _parseAscii(bytes, maxDisplayTriangles);
  if (bytes.length < 84) throw StlFormatException('truncated binary header');
  final bd = ByteData.sublistView(bytes);
  final n = bd.getUint32(80, Endian.little);
  final available = (bytes.length - 84) ~/ 50;
  if (n == 0 || available == 0) throw StlFormatException('no triangles');
  final count = math.min(n, available);
  final stride = math.max(1, (count / maxDisplayTriangles).ceil());
  final out = Float32List(((count + stride - 1) ~/ stride) * 9);
  var o = 0;
  final planes = PlaneCounter();
  final tri = Float64List(9);
  for (var t = 0; t < count; t++) {
    final base = 84 + t * 50 + 12;
    for (var k = 0; k < 9; k++) {
      tri[k] = bd.getFloat32(base + k * 4, Endian.little);
    }
    planes.add(tri);
    if (t % stride == 0) {
      for (var k = 0; k < 9; k++) {
        out[o++] = tri[k];
      }
    }
  }
  return StlMesh(o == out.length ? out : Float32List.sublistView(out, 0, o), false, count,
      planeGroups: planes.count);
}

StlMesh _parseAscii(Uint8List bytes, int maxDisplay) {
  final text = latin1.decode(bytes);
  final verts = <double>[];
  final re = RegExp(r'vertex\s+(\S+)\s+(\S+)\s+(\S+)', caseSensitive: false);
  for (final m in re.allMatches(text)) {
    final x = double.tryParse(m.group(1)!), y = double.tryParse(m.group(2)!), z = double.tryParse(m.group(3)!);
    if (x == null || y == null || z == null) throw StlFormatException('bad vertex');
    verts..add(x)..add(y)..add(z);
  }
  final count = verts.length ~/ 9;
  if (count == 0) throw StlFormatException('no facets');
  final stride = math.max(1, (count / maxDisplay).ceil());
  final out = <double>[];
  final planes = PlaneCounter();
  final tri = Float64List(9);
  for (var t = 0; t < count; t++) {
    for (var k = 0; k < 9; k++) {
      tri[k] = verts[t * 9 + k];
    }
    planes.add(tri);
    if (t % stride == 0) out.addAll(tri);
  }
  return StlMesh(Float32List.fromList(out), true, count, planeGroups: planes.count);
}

/// Reads and parses a file (call inside an isolate for large files).
StlMesh readStlFile(String path, {int maxDisplayTriangles = 60000}) =>
    parseStlBytes(File(path).readAsBytesSync(), maxDisplayTriangles: maxDisplayTriangles);

/// Counts distinct planes among triangles: the normal is quantized to ~0.06°
/// and the plane offset to ~1 µm-scale buckets. Coplanar triangles end up in
/// one bucket, so for a connected part the count approximates the faces left
/// after the core's “merge coplanar” (separate coplanar islands are counted
/// once, so this slightly underestimates). Degenerate triangles are skipped
/// and counted as their own face.
class PlaneCounter {
  PlaneCounter({this.normalStep = 1e-3, this.offsetStep = 1e-3});
  final double normalStep;
  final double offsetStep;
  final _keys = <int>{};
  int _degenerate = 0;

  int get count => _keys.length + _degenerate;

  void add(List<double> t) {
    final ux = t[3] - t[0], uy = t[4] - t[1], uz = t[5] - t[2];
    final vx = t[6] - t[0], vy = t[7] - t[1], vz = t[8] - t[2];
    var nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    final len = math.sqrt(nx * nx + ny * ny + nz * nz);
    if (len < 1e-12) {
      _degenerate++;
      return;
    }
    nx /= len;
    ny /= len;
    nz /= len;
    final d = nx * t[0] + ny * t[1] + nz * t[2];
    _keys.add(Object.hash((nx / normalStep).round(), (ny / normalStep).round(), (nz / normalStep).round(),
        (d / offsetStep).round()));
  }
}
