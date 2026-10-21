import 'dart:math' as math;
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';

import '../theme/tokens.dart';

/// Lightweight software 3D preview (pure Flutter, works on every platform).
///
/// Flat shading with one key light + ambient, painter's-algorithm depth sort,
/// drawn with a single [ui.Vertices] batch. Gestures: drag = rotate,
/// pinch / scroll = zoom (0.4–4×), double tap = reset (ui-spec §4.5).
/// Large meshes are decimated by the caller (display only, conversion untouched).
class MeshViewer extends StatefulWidget {
  const MeshViewer({super.key, required this.positions, this.wireframe = false, this.controller});

  /// 9 floats per triangle.
  final Float32List positions;
  final bool wireframe;
  final MeshViewerController? controller;

  @override
  State<MeshViewer> createState() => _MeshViewerState();
}

class MeshViewerController extends ChangeNotifier {
  void reset() => notifyListeners();
}

class _MeshViewerState extends State<MeshViewer> {
  static const _yaw0 = -0.7, _pitch0 = 0.5;
  double _yaw = _yaw0, _pitch = _pitch0, _zoom = 1, _zoomStart = 1;
  late _Prepared _prep;

  @override
  void initState() {
    super.initState();
    _prep = _Prepared(widget.positions);
    widget.controller?.addListener(_reset);
  }

  @override
  void didUpdateWidget(MeshViewer old) {
    super.didUpdateWidget(old);
    if (!identical(old.positions, widget.positions)) _prep = _Prepared(widget.positions);
    if (old.controller != widget.controller) {
      old.controller?.removeListener(_reset);
      widget.controller?.addListener(_reset);
    }
  }

  @override
  void dispose() {
    widget.controller?.removeListener(_reset);
    super.dispose();
  }

  void _reset() => setState(() {
        _yaw = _yaw0;
        _pitch = _pitch0;
        _zoom = 1;
      });

  @override
  Widget build(BuildContext context) {
    final t = Tokens.of(context);
    return Listener(
      onPointerSignal: (e) {
        if (e is PointerScrollEvent) {
          setState(() => _zoom = (_zoom * math.pow(0.998, e.scrollDelta.dy)).clamp(0.4, 4.0));
        }
      },
      child: GestureDetector(
        onDoubleTap: _reset,
        onScaleStart: (_) => _zoomStart = _zoom,
        onScaleUpdate: (d) => setState(() {
          if (d.pointerCount >= 2) {
            _zoom = (_zoomStart * d.scale).clamp(0.4, 4.0);
          } else {
            _yaw += d.focalPointDelta.dx * 0.01;
            _pitch = (_pitch + d.focalPointDelta.dy * 0.01).clamp(-math.pi / 2, math.pi / 2);
          }
        }),
        child: CustomPaint(
          painter: _MeshPainter(_prep, _yaw, _pitch, _zoom, t.mesh, widget.wireframe,
              Theme.of(context).brightness == Brightness.dark),
          size: Size.infinite,
        ),
      ),
    );
  }
}

class _Prepared {
  _Prepared(Float32List pos) : count = pos.length ~/ 9 {
    centered = Float32List(pos.length);
    normals = Float32List(count * 3);
    if (count == 0) return;
    final mn = [double.infinity, double.infinity, double.infinity];
    final mx = [-double.infinity, -double.infinity, -double.infinity];
    for (var i = 0; i < pos.length; i++) {
      final a = i % 3;
      mn[a] = math.min(mn[a], pos[i]);
      mx[a] = math.max(mx[a], pos[i]);
    }
    final c = [for (var a = 0; a < 3; a++) (mn[a] + mx[a]) / 2];
    var r = 0.0;
    for (var a = 0; a < 3; a++) {
      r = math.max(r, mx[a] - mn[a]);
    }
    final s = r > 0 ? 2 / r : 1.0;
    // STL is usually Z-up; convert to Y-up for viewing.
    for (var i = 0; i < pos.length; i += 3) {
      centered[i] = (pos[i] - c[0]) * s;
      centered[i + 1] = (pos[i + 2] - c[2]) * s;
      centered[i + 2] = -(pos[i + 1] - c[1]) * s;
    }
    for (var t = 0; t < count; t++) {
      final o = t * 9;
      final ux = centered[o + 3] - centered[o], uy = centered[o + 4] - centered[o + 1], uz = centered[o + 5] - centered[o + 2];
      final vx = centered[o + 6] - centered[o], vy = centered[o + 7] - centered[o + 1], vz = centered[o + 8] - centered[o + 2];
      var nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
      final l = math.sqrt(nx * nx + ny * ny + nz * nz);
      if (l > 0) {
        nx /= l;
        ny /= l;
        nz /= l;
      }
      normals[t * 3] = nx;
      normals[t * 3 + 1] = ny;
      normals[t * 3 + 2] = nz;
    }
  }

  final int count;
  late final Float32List centered;
  late final Float32List normals;
}

class _MeshPainter extends CustomPainter {
  _MeshPainter(this.p, this.yaw, this.pitch, this.zoom, this.color, this.wire, this.dark);
  final _Prepared p;
  final double yaw, pitch, zoom;
  final Color color;
  final bool wire, dark;

  @override
  void paint(Canvas canvas, Size size) {
    if (p.count == 0) return;
    final cy = math.cos(yaw), sy = math.sin(yaw), cp = math.cos(pitch), sp = math.sin(pitch);
    final scale = math.min(size.width, size.height) * 0.36 * zoom;
    final ox = size.width / 2, oy = size.height / 2;
    final n = p.count;
    final sx = Float32List(n * 3), syy = Float32List(n * 3);
    final depth = Float32List(n);
    final shade = Float32List(n);
    final src = p.centered;
    // Light direction (normalized) in view space.
    const lx = -0.35, ly = 0.6, lz = 0.72;
    for (var t = 0; t < n; t++) {
      var dsum = 0.0;
      for (var k = 0; k < 3; k++) {
        final o = t * 9 + k * 3;
        final x = src[o], y = src[o + 1], z = src[o + 2];
        final x1 = x * cy + z * sy, z1 = -x * sy + z * cy;
        final y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
        sx[t * 3 + k] = ox + x1 * scale;
        syy[t * 3 + k] = oy - y2 * scale;
        dsum += z2;
      }
      depth[t] = dsum;
      final nx = p.normals[t * 3], ny = p.normals[t * 3 + 1], nz = p.normals[t * 3 + 2];
      final nx1 = nx * cy + nz * sy, nz1 = -nx * sy + nz * cy;
      final ny2 = ny * cp - nz1 * sp, nz2 = ny * sp + nz1 * cp;
      shade[t] = (nx1 * lx + ny2 * ly + nz2 * lz).abs();
    }
    final order = List<int>.generate(n, (i) => i)..sort((a, b) => depth[a].compareTo(depth[b]));
    final verts = Float32List(n * 6);
    final colors = Int32List(n * 3);
    final r = (color.r * 255), g = (color.g * 255), b = (color.b * 255);
    final amb = dark ? 0.30 : 0.38;
    var i = 0;
    for (final t in order) {
      final k = amb + (1 - amb) * shade[t];
      final c = (0xFF << 24) |
          ((r * k).clamp(0, 255).toInt() << 16) |
          ((g * k).clamp(0, 255).toInt() << 8) |
          (b * k).clamp(0, 255).toInt();
      for (var v = 0; v < 3; v++) {
        verts[i * 6 + v * 2] = sx[t * 3 + v];
        verts[i * 6 + v * 2 + 1] = syy[t * 3 + v];
        colors[i * 3 + v] = c;
      }
      i++;
    }
    canvas.drawVertices(
        ui.Vertices.raw(VertexMode.triangles, verts, colors: colors), BlendMode.dst, Paint()..isAntiAlias = true);
    if (wire && n <= 30000) {
      final lines = Float32List(n * 12);
      var o = 0;
      for (var t = 0; t < n; t++) {
        for (var e = 0; e < 3; e++) {
          final a = t * 3 + e, bIdx = t * 3 + (e + 1) % 3;
          lines[o++] = sx[a];
          lines[o++] = syy[a];
          lines[o++] = sx[bIdx];
          lines[o++] = syy[bIdx];
        }
      }
      canvas.drawRawPoints(
          ui.PointMode.lines,
          lines,
          Paint()
            ..color = (dark ? Colors.white : Colors.black).withValues(alpha: 0.25)
            ..strokeWidth = 0.6);
    }
  }

  @override
  bool shouldRepaint(_MeshPainter o) =>
      o.p != p || o.yaw != yaw || o.pitch != pitch || o.zoom != zoom || o.wire != wire || o.color != color || o.dark != dark;
}
