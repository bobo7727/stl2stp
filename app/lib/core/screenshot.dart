import 'dart:io';
import 'dart:ui' as ui;

import 'package:flutter/rendering.dart';
import 'package:flutter/widgets.dart';

/// Saves a PNG of the widget under [key] (used by the integration test to
/// produce preview screenshots without OS screenshot tools).
Future<File> captureWidget(GlobalKey key, String path, {double pixelRatio = 2}) async {
  final boundary = key.currentContext!.findRenderObject()! as RenderRepaintBoundary;
  final image = await boundary.toImage(pixelRatio: pixelRatio);
  final data = await image.toByteData(format: ui.ImageByteFormat.png);
  image.dispose();
  final f = File(path);
  await f.parent.create(recursive: true);
  return f.writeAsBytes(data!.buffer.asUint8List(), flush: true);
}
