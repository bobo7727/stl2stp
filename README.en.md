# STL2STP — STL to STEP/IGES Desktop Tool (Complete Package)

A STL → STEP/IGES conversion tool built on Visual Studio 2019 MFC and OpenCASCADE.
This repository is a complete deliverable package: **source code + buildable + runnable binary + product docs**.

## Layout

| Folder | Description |
| --- | --- |
| `stl2stp-code/` | Visual Studio 2019 solution (`stl2stp-code.sln`) with full MFC source |
| `occt-build/` | OpenCASCADE Release subset: `inc` headers + `win64` runtime libraries (required for building) |
| `stl2stp-code-occt/` | Prebuilt product: portable, just run `stl2stp-code-occt.exe` |

**Product docs (in Chinese)**: `项目说明.txt` (structure & build guide), `转换能力说明书.txt` (conversion capabilities), `产品介绍与销售要点.txt` (product & sales points), `性能与使用场景分析.txt` (performance & scenarios).

## Build (VS2019, Release | x64)

1. Requires VS2019 with the "Desktop development with C++" workload and the C++ MFC v142 components;
2. Open `stl2stp-code/stl2stp-code.sln`, select **Release | x64**, and Build Solution;
3. The 50 OpenCASCADE DLL dependencies are copied to the output directory automatically by the post-build event;
4. `stl2stp-code/` references `occt-build/` by relative path — keep their relative locations unchanged;
5. Note: sources are a mix of GBK / UTF-8 encodings — check a file's encoding before editing.

## Run the prebuilt binary

Open the `stl2stp-code-occt/` folder and double-click `stl2stp-code-occt.exe` (Windows 10/11 64-bit, no installation needed).

## Main features

- STL loading with progress dialog, cancellation, truncation tolerance; model statistics
- QEM mesh simplification (undoable)
- Export to STEP / IGES (confirmation dialog for >100k faces, semantics identical to v1.0.2)
- STEP view-only preview (B-Rep topology statistics)
- Dark/light themes, auto-centered standard views, drag to rotate/pan
