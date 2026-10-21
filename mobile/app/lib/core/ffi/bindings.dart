// Hand-written dart:ffi bindings for core/include/stl2stp/stl2stp.h.
//
// Kept by hand (instead of ffigen) because the API is small and stable; if the
// header changes, update the structs below in the same order. Appended fields in
// stl2stp_export_options must be appended here too (always start from
// stl2stp_default_export_options()).
// ignore_for_file: non_constant_identifier_names, camel_case_types
import 'dart:ffi';

import 'package:ffi/ffi.dart';

typedef ProgressNative = Void Function(Double fraction, Pointer<Void> userData);
typedef CancelNative = Int32 Function(Pointer<Void> userData);

final class Stl2StpCallbacks extends Struct {
  external Pointer<NativeFunction<ProgressNative>> progress;
  external Pointer<NativeFunction<CancelNative>> cancelled;
  external Pointer<Void> userData;
}

final class Stl2StpMeshStats extends Struct {
  @Uint64()
  external int vertices;
  @Uint64()
  external int edges;
  @Uint64()
  external int faces;
  @Array(3)
  external Array<Double> minimum;
  @Array(3)
  external Array<Double> maximum;
}

final class Stl2StpTopology extends Struct {
  @Uint64()
  external int solids;
  @Uint64()
  external int shells;
  @Uint64()
  external int faces;
  @Uint64()
  external int edges;
  @Uint64()
  external int vertices;
}

final class Stl2StpExportOptions extends Struct {
  @Double()
  external double sewingTolerance;
  @Int()
  external int mergeCoplanar;
  @Int32()
  external int mode;
  external Pointer<Utf8> author;
  external Pointer<Utf8> organization;
  external Pointer<Utf8> product;
  external Pointer<Utf8> description;
  external Pointer<Utf8> timestamp;
  @Double()
  external double mergeAngularTolerance;
  @Int()
  external int strictOriginal;
  // Appended in core/weld.
  @Int()
  external int weldVertices;
  @Double()
  external double weldTolerance;
}

/// Function lookups on an opened stl2stp-core library.
class Stl2StpBindings {
  Stl2StpBindings(DynamicLibrary lib)
      : create = lib.lookupFunction<Pointer<Void> Function(), Pointer<Void> Function()>('stl2stp_create'),
        destroy = lib.lookupFunction<Void Function(Pointer<Void>), void Function(Pointer<Void>)>('stl2stp_destroy'),
        lastError = lib.lookupFunction<Pointer<Utf8> Function(Pointer<Void>), Pointer<Utf8> Function(Pointer<Void>)>(
            'stl2stp_last_error'),
        defaultExportOptions = lib.lookupFunction<Stl2StpExportOptions Function(), Stl2StpExportOptions Function()>(
            'stl2stp_default_export_options'),
        loadStl = lib.lookupFunction<Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpCallbacks>),
            int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpCallbacks>)>('stl2stp_load_stl'),
        getMeshStats = lib.lookupFunction<Int32 Function(Pointer<Void>, Pointer<Stl2StpMeshStats>),
            int Function(Pointer<Void>, Pointer<Stl2StpMeshStats>)>('stl2stp_get_mesh_stats'),
        weldVertices = lib.lookupFunction<Int32 Function(Pointer<Void>, Double, Pointer<Uint64>),
            int Function(Pointer<Void>, double, Pointer<Uint64>)>('stl2stp_weld_vertices'),
        simplify = lib.lookupFunction<Int32 Function(Pointer<Void>, Uint64, Pointer<Stl2StpCallbacks>),
            int Function(Pointer<Void>, int, Pointer<Stl2StpCallbacks>)>('stl2stp_simplify'),
        saveStl = lib.lookupFunction<Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpCallbacks>),
            int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpCallbacks>)>('stl2stp_save_stl'),
        exportStep = lib.lookupFunction<
            Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpExportOptions>, Pointer<Stl2StpCallbacks>,
                Pointer<Stl2StpTopology>),
            int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpExportOptions>, Pointer<Stl2StpCallbacks>,
                Pointer<Stl2StpTopology>)>('stl2stp_export_step'),
        exportIges = lib.lookupFunction<
            Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpExportOptions>, Pointer<Stl2StpCallbacks>,
                Pointer<Stl2StpTopology>),
            int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpExportOptions>, Pointer<Stl2StpCallbacks>,
                Pointer<Stl2StpTopology>)>('stl2stp_export_iges'),
        readStep = lib.lookupFunction<
            Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpCallbacks>, Pointer<Stl2StpTopology>),
            int Function(
                Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpCallbacks>, Pointer<Stl2StpTopology>)>('stl2stp_read_step');

  final Pointer<Void> Function() create;
  final void Function(Pointer<Void>) destroy;
  final Pointer<Utf8> Function(Pointer<Void>) lastError;
  final Stl2StpExportOptions Function() defaultExportOptions;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpCallbacks>) loadStl;
  final int Function(Pointer<Void>, Pointer<Stl2StpMeshStats>) getMeshStats;
  final int Function(Pointer<Void>, double, Pointer<Uint64>) weldVertices;
  final int Function(Pointer<Void>, int, Pointer<Stl2StpCallbacks>) simplify;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpCallbacks>) saveStl;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpExportOptions>, Pointer<Stl2StpCallbacks>,
      Pointer<Stl2StpTopology>) exportStep;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpExportOptions>, Pointer<Stl2StpCallbacks>,
      Pointer<Stl2StpTopology>) exportIges;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Stl2StpCallbacks>, Pointer<Stl2StpTopology>) readStep;

  String errorOf(Pointer<Void> h) {
    final p = lastError(h);
    return p == nullptr ? '' : p.toDartString();
  }
}
