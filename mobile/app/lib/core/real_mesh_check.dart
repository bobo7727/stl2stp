import 'memory_guard.dart';
import 'models.dart';

/// Decision on the mesh that will REALLY be built (after simplification, which
/// can stop short of its target: TARGET_NOT_REACHED). Applies to free and Pro.
///
/// Returns null when export may start, [CoreStatus.errorFreeLimit] when a free
/// user's face limit is exceeded, or [CoreStatus.errorMemory] when the real face
/// count would not fit the memory budget.
int? realMeshVerdict(
  int actualFaces,
  ConvertSettings settings, {
  int? maxFaces,
  int? budgetBytes,
  MemoryGuard guard = const MemoryGuard(),
}) {
  if (maxFaces != null && actualFaces > maxFaces) return CoreStatus.errorFreeLimit;
  if (budgetBytes != null && guard.peakBytesForBuiltFaces(actualFaces, settings) > budgetBytes) {
    return CoreStatus.errorMemory;
  }
  return null;
}
