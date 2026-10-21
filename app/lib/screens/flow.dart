import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:path/path.dart' as p;
import 'package:share_plus/share_plus.dart';

import '../core/estimate.dart';
import '../core/models.dart';
import '../core/stl_reader.dart';
import '../l10n/strings.dart';
import '../state/app_state.dart';
import '../state/history.dart';
import 'error_screen.dart';
import 'preview_screen.dart';

/// Loaded model handed to the preview screen.
class LoadedModel {
  LoadedModel(this.info, this.preview);
  final MeshInfo info;
  final StlMesh? preview;
}

/// Shows the system picker (STL only). Returns a local path or null.
/// Desktop fallback: a path text field when the picker is unavailable
/// (e.g. WSLg without an XDG desktop portal).
Future<String?> pickStl(BuildContext context) async {
  final s = S.of(context);
  try {
    final f = await FilePicker.pickFile(type: FileType.custom, allowedExtensions: const ['stl', 'STL']);
    if (f == null) return null;
    final path = f.path;
    if (path != null) return path;
    // Non-file URI (sandboxed platforms): copy into the app's temp dir.
    final tmp = File(p.join(Directory.systemTemp.path, f.name));
    await tmp.writeAsBytes(await f.readAsBytes(), flush: true);
    return tmp.path;
  } catch (e) {
    debugPrint('file picker failed: $e');
    if (!context.mounted) return null;
    return askPath(context, s.t('x.open_path.picker_failed'));
  }
}

Future<String?> askPath(BuildContext context, String note) {
  final s = S.of(context);
  final c = TextEditingController();
  return showDialog<String>(
    context: context,
    builder: (ctx) => AlertDialog(
      title: Text(s.t('x.open_path.title')),
      content: Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(note, style: const TextStyle(fontSize: 13)),
        const SizedBox(height: 8),
        TextField(
            key: const Key('pathField'),
            controller: c,
            autofocus: true,
            decoration: InputDecoration(hintText: s.t('x.open_path.hint'))),
      ]),
      actions: [
        TextButton(onPressed: () => Navigator.pop(ctx), child: Text(s.t('common.cancel'))),
        FilledButton(
            style: FilledButton.styleFrom(minimumSize: const Size(64, 40)),
            onPressed: () => Navigator.pop(ctx, c.text.trim().isEmpty ? null : c.text.trim()),
            child: Text(s.t('common.ok'))),
      ],
    ),
  );
}

/// Loads [path] (core stats + Dart preview) behind a loading overlay, then
/// opens the preview or the matching error screen (ui-spec §2.1/§2.2).
Future<void> openModel(BuildContext context, String path, {bool replace = false}) async {
  final state = AppScope.read(context);
  final nav = Navigator.of(context);
  final s = S.of(context);
  final overlay = _showLoading(context, s.t('import.loading'));
  LoadedModel? model;
  int? errorStatus;
  String errorMsg = '';
  try {
    final infoF = state.engine.inspect(path);
    final previewF = compute(_readPreview, path);
    final info = await infoF;
    StlMesh? preview;
    try {
      preview = await previewF;
    } catch (_) {}
    model = LoadedModel(info, preview);
  } on LoadException catch (e) {
    errorStatus = e.status;
    errorMsg = e.message;
  } catch (e) {
    errorStatus = CoreStatus.errorInternal;
    errorMsg = '$e';
  } finally {
    overlay.remove();
  }
  Route<void> route;
  if (model != null) {
    if (model.info.faces > AppState.limitFaces) {
      final m = model;
      route = MaterialPageRoute(
          builder: (_) => ErrorScreen(
              kind: ErrorKind.tooBig,
              faces: m.info.faces,
              onPrimary: (ctx) {
                final keep = ((AppState.limitFaces * 0.9 / m.info.faces) * 100 / 5).floor() * 5;
                Navigator.of(ctx).pushReplacement(MaterialPageRoute(
                    builder: (_) => PreviewScreen(model: m, forceSimplifyPercent: keep.clamp(5, 95))));
              }));
    } else {
      final m = model;
      route = MaterialPageRoute(builder: (_) => PreviewScreen(model: m));
    }
  } else {
    final kind = switch (errorStatus) {
      CoreStatus.errorEmpty => ErrorKind.empty,
      CoreStatus.errorMemory => ErrorKind.memory,
      CoreStatus.errorFormat || CoreStatus.errorIo => ErrorKind.format,
      _ => ErrorKind.generic,
    };
    route = MaterialPageRoute(
        builder: (_) => ErrorScreen(kind: kind, code: errorStatus, details: errorMsg, onPrimary: (ctx) async {
              final pth = await pickStl(ctx);
              if (pth != null && ctx.mounted) await openModel(ctx, pth, replace: true);
            }));
  }
  if (replace) {
    await nav.pushReplacement(route);
  } else {
    await nav.push(route);
  }
}

StlMesh _readPreview(String path) => readStlFile(path, maxDisplayTriangles: 40000);

OverlayEntry _showLoading(BuildContext context, String text) {
  final entry = OverlayEntry(
    builder: (_) => Material(
      color: Colors.black38,
      child: Center(
        child: Card(
          child: Padding(
            padding: const EdgeInsets.all(24),
            child: Column(mainAxisSize: MainAxisSize.min, children: [
              const CircularProgressIndicator(),
              const SizedBox(height: 16),
              Text(text),
            ]),
          ),
        ),
      ),
    ),
  );
  Overlay.of(context).insert(entry);
  return entry;
}

/// Share via the system share sheet; desktop fallback copies the path.
Future<void> shareFile(BuildContext context, String path, {Rect? origin}) async {
  final s = S.of(context);
  final messenger = ScaffoldMessenger.of(context);
  try {
    if (!(Platform.isIOS || Platform.isAndroid || Platform.isMacOS)) throw UnsupportedError('desktop');
    await SharePlus.instance.share(ShareParams(files: [XFile(path)], sharePositionOrigin: origin));
  } catch (_) {
    await Clipboard.setData(ClipboardData(text: path));
    messenger.showSnackBar(SnackBar(content: Text('${s.t('x.share_unavailable')}\n$path')));
  }
}

/// "存到“文件”": system save dialog with the file bytes.
Future<void> saveToFiles(BuildContext context, String path) async {
  final s = S.of(context);
  final messenger = ScaffoldMessenger.of(context);
  try {
    final bytes = await File(path).readAsBytes();
    final uri = await FilePicker.saveFile(fileName: p.basename(path), bytes: bytes);
    if (uri != null) {
      messenger.showSnackBar(SnackBar(content: Text(s.t('x.saved_to', {'path': uri.toFilePath()}))));
    }
  } catch (e) {
    await Clipboard.setData(ClipboardData(text: path));
    messenger.showSnackBar(SnackBar(content: Text('${s.t('x.output_path')}: $path (${s.t('x.copied')})')));
  }
}

String historySubtitle(BuildContext context, HistoryEntry e) {
  final now = DateTime.now();
  final t = e.time;
  final hm = '${t.hour.toString().padLeft(2, '0')}:${t.minute.toString().padLeft(2, '0')}';
  final sameDay = t.year == now.year && t.month == now.month && t.day == now.day;
  final date = sameDay ? hm : '${t.month}/${t.day} $hm';
  return '$date · ${formatBytes(e.bytes)}';
}
