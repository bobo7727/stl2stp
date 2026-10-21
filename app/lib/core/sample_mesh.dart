import 'dart:io';
import 'dart:math' as math;
import 'dart:typed_data';

/// Builds a closed L-shaped bracket (extruded hexagon) as binary STL bytes.
/// Used by tests, the integration test and the demo engine.
Uint8List buildBracketStl({double w = 60, double h = 50, double t = 10, double depth = 40}) {
  // Outline in XZ, counter-clockwise, extruded along +Y.
  final outline = <List<double>>[
    [0, 0], [t, 0], [w, 0], [w, t], [t, t], [t, h], [0, h], [0, t],
  ];
  final tris = <List<List<double>>>[];
  List<double> p(List<double> xz, double y) => [xz[0], y, xz[1]];
  // Side walls.
  for (var i = 0; i < outline.length; i++) {
    final a = outline[i], b = outline[(i + 1) % outline.length];
    tris.add([p(a, 0), p(b, depth), p(b, 0)]);
    tris.add([p(a, 0), p(a, depth), p(b, depth)]);
  }
  // Caps: split the L into three quads that share full edges (no T-junctions).
  final rects = [
    [[0.0, 0.0], [t, 0.0], [t, t], [0.0, t]],
    [[t, 0.0], [w, 0.0], [w, t], [t, t]],
    [[0.0, t], [t, t], [t, h], [0.0, h]],
  ];
  for (final r in rects) {
    // y = 0 cap (normal -Y) and y = depth cap (normal +Y).
    tris.add([p(r[0], 0), p(r[1], 0), p(r[2], 0)]);
    tris.add([p(r[0], 0), p(r[2], 0), p(r[3], 0)]);
    tris.add([p(r[0], depth), p(r[2], depth), p(r[1], depth)]);
    tris.add([p(r[0], depth), p(r[3], depth), p(r[2], depth)]);
  }
  final bd = ByteData(84 + 50 * tris.length);
  final header = 'stl2stp sample bracket'.codeUnits;
  for (var i = 0; i < header.length; i++) {
    bd.setUint8(i, header[i]);
  }
  bd.setUint32(80, tris.length, Endian.little);
  var off = 84;
  for (final tri in tris) {
    off += 12; // normal left as zero; readers recompute it
    for (final v in tri) {
      for (final c in v) {
        bd.setFloat32(off, c, Endian.little);
        off += 4;
      }
    }
    off += 2;
  }
  return bd.buffer.asUint8List();
}

Future<String> writeSampleStl(String path) async {
  await File(path).writeAsBytes(buildBracketStl(), flush: true);
  return path;
}

/// Closed UV sphere (radius [r]) as binary STL; 2*seg*(rings-1) triangles.
Uint8List buildSphereStl({int seg = 64, int rings = 32, double r = 25}) {
  List<double> at(int i, int j) {
    final th = 3.141592653589793 * j / rings;
    final ph = 2 * 3.141592653589793 * (i % seg) / seg;
    return [r * _sin(th) * _cos(ph), r * _sin(th) * _sin(ph), r * _cos(th)];
  }

  final tris = <List<List<double>>>[];
  for (var j = 0; j < rings; j++) {
    for (var i = 0; i < seg; i++) {
      final a = at(i, j), b = at(i + 1, j), c = at(i + 1, j + 1), d = at(i, j + 1);
      if (j != 0) tris.add([a, d, b]);
      if (j != rings - 1) tris.add([b, d, c]);
    }
  }
  final bd = ByteData(84 + 50 * tris.length);
  bd.setUint32(80, tris.length, Endian.little);
  var off = 84;
  for (final tri in tris) {
    off += 12;
    for (final v in tri) {
      for (final c in v) {
        bd.setFloat32(off, c, Endian.little);
        off += 4;
      }
    }
    off += 2;
  }
  return bd.buffer.asUint8List();
}

double _sin(double x) => math.sin(x);
double _cos(double x) => math.cos(x);
