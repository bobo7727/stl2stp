import 'dart:convert';
import 'dart:io';

import 'package:path/path.dart' as p;

import '../core/models.dart';

class HistoryEntry {
  HistoryEntry({
    required this.outputPath,
    required this.format,
    required this.bytes,
    required this.time,
    required this.sourceName,
    required this.settings,
  });

  final String outputPath;
  final ExportFormat format;
  final int bytes;
  final DateTime time;
  final String sourceName;
  final ConvertSettings settings;

  String get fileName => p.basename(outputPath);
  bool get exists => File(outputPath).existsSync();

  Map<String, Object?> toJson() => {
        'outputPath': outputPath,
        'format': format.name,
        'bytes': bytes,
        'time': time.toUtc().toIso8601String(),
        'sourceName': sourceName,
        'settings': settings.toJson(),
      };

  static HistoryEntry fromJson(Map<String, Object?> j) => HistoryEntry(
        outputPath: j['outputPath'] as String,
        format: ExportFormat.values.firstWhere((f) => f.name == j['format'], orElse: () => ExportFormat.step),
        bytes: (j['bytes'] as num).toInt(),
        time: DateTime.parse(j['time'] as String).toLocal(),
        sourceName: (j['sourceName'] as String?) ?? '',
        settings: ConvertSettings.fromJson((j['settings'] as Map).cast<String, Object?>()),
      );
}

/// History persisted as JSON next to the outputs (ui-spec §4.12), newest first, max 50.
class HistoryStore {
  HistoryStore(this.file);
  final File file;
  static const maxEntries = 50;

  List<HistoryEntry> load() {
    try {
      if (!file.existsSync()) return [];
      final list = jsonDecode(file.readAsStringSync()) as List;
      return [for (final e in list) HistoryEntry.fromJson((e as Map).cast<String, Object?>())];
    } catch (_) {
      return [];
    }
  }

  void save(List<HistoryEntry> entries) {
    file.parent.createSync(recursive: true);
    file.writeAsStringSync(jsonEncode([for (final e in entries.take(maxEntries)) e.toJson()]));
  }
}
