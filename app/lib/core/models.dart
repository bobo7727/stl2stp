/// Plain data types shared by the UI and the conversion engines.
library;

/// Mirrors `stl2stp_status` in core/include/stl2stp/stl2stp.h (values are stable).
abstract final class CoreStatus {
  static const ok = 0;
  static const warningPartial = 1;
  static const warningTargetNotReached = 2;
  static const warningNotSolid = 3;
  static const warningRepaired = 4;
  static const warningMetadataChanged = 5;
  static const errorArgument = -1;
  static const errorIo = -2;
  static const errorFormat = -3;
  static const errorEmpty = -4;
  static const errorCancelled = -5;
  static const errorOcct = -6;
  static const errorMemory = -7;
  static const errorInternal = -8;

  static bool isError(int s) => s < 0;
}

enum ExportFormat {
  step('STEP', 'step'),
  iges('IGES', 'igs');

  const ExportFormat(this.label, this.extension);
  final String label;
  final String extension;
}

/// Mirrors `stl2stp_mode`.
enum ConvertMode {
  original(0),
  sewing(1);

  const ConvertMode(this.value);
  final int value;
}

enum Preset { recommended, smaller, strict }

/// Everything the user can set on the preview screen (ui-spec §4.5/§4.6).
class ConvertSettings {
  const ConvertSettings({
    this.format = ExportFormat.step,
    this.simplify = false,
    this.keepPercent = 50,
    this.mode = ConvertMode.original,
    this.mergeCoplanar = false,
    this.verify = true,
    this.author = '',
    this.organization = '',
    this.product = '',
    this.description = '',
  });

  final ExportFormat format;
  final bool simplify;

  /// 5..95, step 5.
  final int keepPercent;
  final ConvertMode mode;
  final bool mergeCoplanar;
  final bool verify;
  final String author;
  final String organization;
  final String product;
  final String description;

  static ConvertSettings forPreset(Preset p, {ExportFormat format = ExportFormat.step}) {
    switch (p) {
      case Preset.recommended:
        return ConvertSettings(format: format);
      case Preset.smaller:
        return ConvertSettings(format: format, simplify: true, keepPercent: 50, mergeCoplanar: true);
      case Preset.strict:
        return ConvertSettings(format: format, mode: ConvertMode.sewing);
    }
  }

  /// The preset these settings correspond to, or null for "custom".
  /// Metadata and format do not affect the preset.
  Preset? get preset {
    for (final p in Preset.values) {
      final ref = ConvertSettings.forPreset(p);
      if (ref.simplify == simplify &&
          (!simplify || ref.keepPercent == keepPercent) &&
          ref.mode == mode &&
          ref.mergeCoplanar == mergeCoplanar &&
          ref.verify == verify) {
        return p;
      }
    }
    return null;
  }

  /// Applies a preset but keeps format and metadata.
  ConvertSettings withPreset(Preset p) {
    final ref = ConvertSettings.forPreset(p, format: format);
    return ref.copyWith(
        author: author, organization: organization, product: product, description: description);
  }

  bool get hasNonAsciiMeta =>
      [author, organization, product, description].any((s) => s.runes.any((r) => r > 0x7e || r < 0x20));

  bool get verifyEffective => verify && format == ExportFormat.step;

  int targetFaces(int faces) => (faces * keepPercent / 100).round().clamp(1, faces < 1 ? 1 : faces);

  ConvertSettings copyWith({
    ExportFormat? format,
    bool? simplify,
    int? keepPercent,
    ConvertMode? mode,
    bool? mergeCoplanar,
    bool? verify,
    String? author,
    String? organization,
    String? product,
    String? description,
  }) {
    return ConvertSettings(
      format: format ?? this.format,
      simplify: simplify ?? this.simplify,
      keepPercent: keepPercent ?? this.keepPercent,
      mode: mode ?? this.mode,
      mergeCoplanar: mergeCoplanar ?? this.mergeCoplanar,
      verify: verify ?? this.verify,
      author: author ?? this.author,
      organization: organization ?? this.organization,
      product: product ?? this.product,
      description: description ?? this.description,
    );
  }

  Map<String, Object?> toJson() => {
        'format': format.name,
        'simplify': simplify,
        'keepPercent': keepPercent,
        'mode': mode.name,
        'mergeCoplanar': mergeCoplanar,
        'verify': verify,
        'author': author,
        'organization': organization,
        'product': product,
        'description': description,
      };

  static ConvertSettings fromJson(Map<String, Object?> j) => ConvertSettings(
        format: ExportFormat.values.firstWhere((f) => f.name == j['format'], orElse: () => ExportFormat.step),
        simplify: j['simplify'] == true,
        keepPercent: (j['keepPercent'] as num?)?.toInt() ?? 50,
        mode: ConvertMode.values.firstWhere((m) => m.name == j['mode'], orElse: () => ConvertMode.original),
        mergeCoplanar: j['mergeCoplanar'] == true,
        verify: j['verify'] != false,
        author: (j['author'] as String?) ?? '',
        organization: (j['organization'] as String?) ?? '',
        product: (j['product'] as String?) ?? '',
        description: (j['description'] as String?) ?? '',
      );
}

/// Removes characters IGES cannot carry (ui-spec §4.6, core TODO 1).
String stripNonAscii(String s) => String.fromCharCodes(s.runes.where((r) => r >= 0x20 && r <= 0x7e)).trim();

/// Result of `stl2stp_load_stl` + `stl2stp_get_mesh_stats`.
class MeshInfo {
  const MeshInfo({
    required this.path,
    required this.fileName,
    required this.fileSize,
    required this.faces,
    required this.vertices,
    required this.edges,
    required this.min,
    required this.max,
    required this.isAscii,
  });

  final String path;
  final String fileName;
  final int fileSize;
  final int faces;
  final int vertices;
  final int edges;
  final List<double> min;
  final List<double> max;
  final bool isAscii;

  List<double> get size => [for (var i = 0; i < 3; i++) max[i] - min[i]];
}

class Topology {
  const Topology({this.solids = 0, this.shells = 0, this.faces = 0, this.edges = 0, this.vertices = 0});
  final int solids;
  final int shells;
  final int faces;
  final int edges;
  final int vertices;
}

/// Conversion stages shown on the converting screen (ui-spec §4.7).
enum Stage {
  read(10),
  simplify(10),
  build(50),
  write(20),
  verify(10);

  const Stage(this.weight);
  final int weight;
}

class ConvertRequest {
  const ConvertRequest({required this.inputPath, required this.outputPath, required this.settings, required this.faces});
  final String inputPath;
  final String outputPath;
  final ConvertSettings settings;

  /// Face count of the loaded mesh (used for the simplify target).
  final int faces;

  List<Stage> get stages => [
        Stage.read,
        if (settings.simplify) Stage.simplify,
        Stage.build,
        Stage.write,
        if (settings.verifyEffective) Stage.verify,
      ];
}

class ConvertResult {
  const ConvertResult({
    required this.status,
    required this.outputPath,
    this.message = '',
    this.topology = const Topology(),
    this.verified,
    this.facesAfterSimplify,
    this.warnings = const [],
    this.elapsed = Duration.zero,
    this.failedStage,
  });

  /// Final status (an error code if failed, else the most relevant warning or OK).
  final int status;
  final String outputPath;
  final String message;
  final Topology topology;

  /// null = not run, true = passed, false = failed.
  final bool? verified;
  final int? facesAfterSimplify;

  /// All non-OK, non-error statuses seen.
  final List<int> warnings;
  final Duration elapsed;
  final Stage? failedStage;

  bool get ok => !CoreStatus.isError(status);
}

/// Progress report from an engine: current stage and overall 0..1 fraction.
class ConvertProgress {
  const ConvertProgress(this.stage, this.stageFraction, this.overall);
  final Stage stage;
  final double stageFraction;
  final double overall;
}

/// Combines stage weights (read 10, simplify 10, build 50, write 20, verify 10).
double overallProgress(List<Stage> stages, Stage current, double fraction) {
  final total = stages.fold<int>(0, (a, s) => a + s.weight);
  var done = 0;
  for (final s in stages) {
    if (s == current) break;
    done += s.weight;
  }
  return ((done + current.weight * fraction.clamp(0.0, 1.0)) / total).clamp(0.0, 1.0);
}

class LoadException implements Exception {
  LoadException(this.status, this.message);
  final int status;
  final String message;
  @override
  String toString() => 'LoadException($status): $message';
}
