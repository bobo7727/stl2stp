import 'dart:io';

import 'package:desktop_drop/desktop_drop.dart';
import 'package:flutter/material.dart';

import '../l10n/strings.dart';
import '../theme/tokens.dart';

/// Desktop drag-and-drop of .stl files (Linux/macOS/Windows via desktop_drop).
/// On iOS/Android it is a no-op wrapper; iPad drag-and-drop needs a native
/// UIDropInteraction / document-picker path (see docs/mac-setup-guide.md).
class StlDropTarget extends StatefulWidget {
  const StlDropTarget({super.key, required this.child, required this.onFile});
  final Widget child;
  final void Function(String path) onFile;

  static bool get supported => Platform.isLinux || Platform.isMacOS || Platform.isWindows;

  /// First .stl path among [paths], or null.
  static String? pickStl(Iterable<String> paths) =>
      paths.where((x) => x.toLowerCase().endsWith('.stl')).firstOrNull;

  @override
  State<StlDropTarget> createState() => _StlDropTargetState();
}

class _StlDropTargetState extends State<StlDropTarget> {
  bool _hover = false;

  @override
  Widget build(BuildContext context) {
    if (!StlDropTarget.supported) return widget.child;
    final t = Tokens.of(context);
    return DropTarget(
      onDragEntered: (_) => setState(() => _hover = true),
      onDragExited: (_) => setState(() => _hover = false),
      onDragDone: (d) {
        setState(() => _hover = false);
        final path = StlDropTarget.pickStl(d.files.map((f) => f.path));
        if (path != null) {
          widget.onFile(path);
        } else {
          ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(S.of(context).t('x.drop_not_stl'))));
        }
      },
      child: Stack(children: [
        widget.child,
        if (_hover)
          Positioned.fill(
            child: IgnorePointer(
              child: Container(
                margin: const EdgeInsets.all(8),
                decoration: BoxDecoration(
                  color: t.accent.withValues(alpha: 0.08),
                  border: Border.all(color: t.accent, width: 2),
                  borderRadius: BorderRadius.circular(16),
                ),
                alignment: Alignment.center,
                child: Text(S.of(context).t('x.drop_here'),
                    style: TextStyle(fontSize: 20, fontWeight: FontWeight.w600, color: t.accent)),
              ),
            ),
          ),
      ]),
    );
  }
}
